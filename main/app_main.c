/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_random.h"
#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"
#include "dht22.h"
#include "driver/gptimer_types.h"

#define FAN_MODE3_PIN GPIO_NUM_4
#define FAN_MODE2_PIN GPIO_NUM_5
#define FAN_MODE1_PIN GPIO_NUM_6
#define FAN_OCSILLATION_PIN GPIO_NUM_7
#define BUZZER_PIN GPIO_NUM_9
#define LED_ON_PIN GPIO_NUM_2
#define LED_OFF_PIN GPIO_NUM_3

bool running = false;
typedef enum
{
    FAN_MODE_OFF = 0,
    FAN_MODE_1,
    FAN_MODE_2,
    FAN_MODE_3,
    FAN_MODE_OSCILLATION,
    FAN_MODE_OFF_OSCILLATION
} fan_mode;

const char *ssid = "Son Tra";
const char *pass = "L02012001";

static const char *TAG = "mqtt5_example";
esp_mqtt_client_handle_t mqttClient;
adc_oneshot_unit_handle_t adc1_handle;

/* HiveMQ Cloud configuration - EDIT these before flashing */
/* Use mqtts:// scheme to enable TLS */
static const char *HIVEMQ_URI = "mqtt://afe100349ba44464b15f0bfb86846d85.s1.eu.hivemq.cloud:8883";
static const char *HIVEMQ_USERNAME = "lethanhtra";
static const char *HIVEMQ_PASSWORD = "Thanhtra2004";

extern const uint8_t isrgrootx1_pem_start[] asm("_binary_isrgrootx1_pem_start");
extern const uint8_t isrgrootx1_pem_end[] asm("_binary_isrgrootx1_pem_end");

// wifi
int retry_num = 0;

static EventGroupHandle_t s_wifi_event_group;
QueueHandle_t adc_queue, control_queue, temp_queue, alarm_queue;
TimerHandle_t fan_off_timer;

#define WIFI_CONNECTED_BIT (1 << 0)

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START)
    {
        printf("WIFI CONNECTING....\n");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
        printf("WiFi CONNECTED\n");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        printf("WiFi lost connection\n");
        if (retry_num < 5)
        {
            esp_wifi_connect();
            retry_num++;
            printf("Retrying to Connect...\n");
        }
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
        printf("Wifi got IP...\n\n");
        if (s_wifi_event_group)
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

void wifi_connection()
{
    //                          s1.4
    // 2 - Wi-Fi Configuration Phase
    esp_netif_init();
    esp_event_loop_create_default();     // event loop                    s1.2
    esp_netif_create_default_wifi_sta(); // WiFi station                      s1.3
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); //
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    /* create event group to wait for IP */
    if (s_wifi_event_group == NULL)
    {
        s_wifi_event_group = xEventGroupCreate();
    }
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",

        }

    };
    strcpy((char *)wifi_configuration.sta.ssid, ssid);
    strcpy((char *)wifi_configuration.sta.password, pass);
    // esp_log_write(ESP_LOG_INFO, "Kconfig", "SSID=%s, PASS=%s", ssid, pass);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    // 3 - Wi-Fi Start Phase
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    // 4- Wi-Fi Connect Phase
    esp_wifi_connect();
    printf("wifi_init_softap finished. SSID:%s  password:%s", ssid, pass);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void mqtt_publish_task(void *pvParameters)
{
    char datatoSend[20];
    // if(esp_mqtt_client_publish(mqttClient, "iot/fan/connect", "connected", 0, 0, 0 == 0))
    // {
    //     ESP_LOGI(TAG, "Published mqtt connected");
    // }
    while (1)
    {
        float temp = 25.0;
        xQueueReceive(temp_queue, &temp, portMAX_DELAY);
        sprintf(datatoSend, "%.2f", temp);
        int msg_id1 = esp_mqtt_client_publish(mqttClient, "iot/room/temp", datatoSend, 0, 0, 0);
        if (msg_id1 == 0)
        {
            ESP_LOGI(TAG, "Published temperature %.2f *C", temp);
        }
        else
            ESP_LOGI(TAG, "Error msg_id:%d while publishing temperature");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "iot/fan/state", 0);
        esp_mqtt_client_subscribe(client, "iot/fan/speed", 0);
        esp_mqtt_client_subscribe(client, "iot/fan/osc", 0);
        esp_mqtt_client_subscribe(client, "iot/fan/alarm", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        xTaskCreate(mqtt_publish_task, "mqtt_publish_task", 4096, NULL, 6, NULL);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ", event->msg_id, (uint8_t)*event->data);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if (strncmp(event->topic, "iot/fan/state", event->topic_len) == 0)
        {
            if (strncmp(event->data, "OFF", event->data_len) == 0)
            {
                ESP_LOGI(TAG, "Turning Off FAN");
                fan_mode cmd = FAN_MODE_OFF;
                xQueueSend(control_queue, &cmd, portMAX_DELAY);
            }
            
        }
        if (strncmp(event->topic, "iot/fan/osc", event->topic_len) == 0)
        {
            if (strncmp(event->data, "ON", event->data_len) == 0)
            {
                ESP_LOGI(TAG, "Turning on OCSILLATION");
                fan_mode cmd = FAN_MODE_OSCILLATION;
                xQueueSend(control_queue, &cmd, portMAX_DELAY);
            }
            else
            {
                ESP_LOGI(TAG, "Turning OSCILLATION OFF");
                fan_mode cmd = FAN_MODE_OFF_OSCILLATION;
                xQueueSend(control_queue, &cmd, portMAX_DELAY);
            }
        }
        if (strncmp(event->topic, "iot/fan/speed", event->topic_len) == 0)
        {

            if (strncmp(event->data, "1", event->data_len) == 0)
            {
                ESP_LOGI(TAG, "1");
                fan_mode cmd = FAN_MODE_1;
                xQueueSend(control_queue, &cmd, portMAX_DELAY);
            }
            else if (strncmp(event->data, "2", event->data_len) == 0)
            {
                ESP_LOGI(TAG, "Mode 2");
                fan_mode cmd = FAN_MODE_2;
                xQueueSend(control_queue, &cmd, portMAX_DELAY);
            }
            else if (strncmp(event->data, "3", event->data_len) == 0)
            {
                ESP_LOGI(TAG, "Mode 3");
                fan_mode cmd = FAN_MODE_3;
                xQueueSend(control_queue, &cmd, portMAX_DELAY);
            }

            else
            {
                ESP_LOGI(TAG, "Fan OFF");
                fan_mode cmd = FAN_MODE_OFF;
                xQueueSend(control_queue, &cmd, portMAX_DELAY);
            }
        }
        if (strncmp(event->topic, "iot/fan/alarm", event->topic_len) == 0)
        {
            char buf[16] = {0};
            int len = event->data_len;

            if (len >= sizeof(buf))
                len = sizeof(buf) - 1;
            memcpy(buf, event->data, len);
            uint32_t time_alarm = atoi(buf);
            if (time_alarm > 0)
            {
                ESP_LOGI(TAG, "Set alarm in %d second", time_alarm);
                xTimerStop(fan_off_timer, 0);
                xTimerChangePeriod(fan_off_timer, pdMS_TO_TICKS(time_alarm * 1000), 0);
                xTimerStart(fan_off_timer, 0);
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = "mqtts://afe100349ba44464b15f0bfb86846d85.s1.eu.hivemq.cloud",
            .address.port = 8883,
            .verification.certificate = (const char *)isrgrootx1_pem_start,
        },
        .credentials = {
            .username = HIVEMQ_USERNAME,
            .authentication.password = HIVEMQ_PASSWORD,
        },
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    mqttClient = client;
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void control_task(void *pvParemeters)
{
    fan_mode cmd;
    while (1)
    {
        xQueueReceive(control_queue, &cmd, portMAX_DELAY);
        gpio_set_level(BUZZER_PIN, 0);

        ESP_LOGI(TAG, "Received fan mode command: %d", cmd);
        if (cmd != FAN_MODE_OFF)
        {
            gpio_set_level(LED_OFF_PIN, 1);
            gpio_set_level(LED_ON_PIN, 0);
            switch (cmd)
            {
            case FAN_MODE_1:
                ESP_LOGI(TAG, "FAN MODE 1");
                gpio_set_level(FAN_MODE2_PIN, 1);
                gpio_set_level(FAN_MODE3_PIN, 1);
                gpio_set_level(FAN_MODE1_PIN, 0);
                running = true;
                break;
            case FAN_MODE_2:
                gpio_set_level(FAN_MODE1_PIN, 1);
                gpio_set_level(FAN_MODE3_PIN, 1);
                gpio_set_level(FAN_MODE2_PIN, 0);
                running = true;
                break;
            case FAN_MODE_3:
                gpio_set_level(FAN_MODE1_PIN, 1);
                gpio_set_level(FAN_MODE2_PIN, 1);
                gpio_set_level(FAN_MODE3_PIN, 0);
                running = true;
                break;
            case FAN_MODE_OSCILLATION:  // on
                if (running)
                    gpio_set_level(FAN_OCSILLATION_PIN, 0);
                break;
            case FAN_MODE_OFF_OSCILLATION:
                gpio_set_level(FAN_OCSILLATION_PIN, 1);
                break;
            default:
                break;
            }
        }
        else
        {
            ESP_LOGI(TAG, "FAN OFF");
            gpio_set_level(FAN_MODE1_PIN, 1);
            gpio_set_level(FAN_MODE2_PIN, 1);
            gpio_set_level(FAN_MODE3_PIN, 1);
            gpio_set_level(FAN_OCSILLATION_PIN, 1);
            gpio_set_level(LED_OFF_PIN, 0); // led off ON
            gpio_set_level(LED_ON_PIN, 1);  // led on OFF
            running = false;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(BUZZER_PIN, 1);
    }
}

void temp_task(void *pv)
{
    float temp = 0.0;
    float humi = 0.0;
    while (1)
    {
        if (dht22_read(&temp, &humi))
        {
            xQueueSend(temp_queue, &temp, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void fan_off_callback(TimerHandle_t xTimer)
{
    fan_mode cmd = FAN_MODE_OFF;
    xQueueSendFromISR(control_queue, &cmd, NULL);
}

void gpio_output_config()
{
    gpio_config_t gpio_output = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << FAN_MODE1_PIN | 1ULL << FAN_MODE2_PIN | 1ULL << FAN_MODE3_PIN | 1ULL << FAN_OCSILLATION_PIN | 1ULL << LED_OFF_PIN | 1ULL << LED_ON_PIN, // gpio 4,5,6,7
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    gpio_config_t buzzer_config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT_OD, // open drain mode
        .pin_bit_mask = 1ULL << BUZZER_PIN,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    gpio_config(&gpio_output);
    gpio_config(&buzzer_config);
    // level 1 to turn off all
    gpio_set_level(BUZZER_PIN, 1);
    gpio_set_level(LED_OFF_PIN, 0);
    gpio_set_level(LED_ON_PIN, 1);
    gpio_set_level(FAN_MODE1_PIN, 1);
    gpio_set_level(FAN_MODE2_PIN, 1);
    gpio_set_level(FAN_MODE3_PIN, 1);
    gpio_set_level(FAN_OCSILLATION_PIN, 1);
}

void app_main(void)
{
    gpio_output_config();
    control_queue = xQueueCreate(5, sizeof(fan_mode));
    temp_queue = xQueueCreate(5, sizeof(float));
    nvs_flash_init();
    wifi_connection();

    /* Wait for Wi-Fi IP (more robust than a fixed delay) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
    if ((bits & WIFI_CONNECTED_BIT) == 0)
    {
        ESP_LOGW(TAG, "Timeout waiting for Wi-Fi IP, attempting MQTT anyway");
    }
    else
    {
        ESP_LOGI(TAG, "Wi-Fi connected, starting MQTT client to %s", HIVEMQ_URI);
    }
    mqtt_app_start();
    xTaskCreate(control_task, "control_task", 4096, NULL, 4, NULL);
    xTaskCreate(temp_task, "temp_task", 4096, NULL, 3, NULL);
    fan_off_timer = xTimerCreate("fan_off_timer", pdMS_TO_TICKS(1000), pdFALSE, (void *)0, fan_off_callback);
}
