#include "dht22.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#define DHT22_PIN GPIO_NUM_1

void gpio_config_input()
{
    gpio_config_t gpio_input = {
        .pin_bit_mask = 1UL << DHT22_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_input);
}

void gpio_config_output()
{
    gpio_config_t gpio_output = {
        .pin_bit_mask = 1UL << DHT22_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_output); 
}

int dht_read_bit()
{
    int timeout = 0;
    // wait for 50us low signal
    while(gpio_get_level(DHT22_PIN) == 0)
    {
        timeout++;
        if(timeout > 100)
        {
            return -1; // timeout error
        }
        esp_rom_delay_us(1);
    }

    // measure length of high signal
    timeout = 0;
    while(gpio_get_level(DHT22_PIN) == 1)
    {
        timeout++;
        if(timeout > 100)
        {
            return -1; // timeout error
        }
        esp_rom_delay_us(1);
    }
    // if high signal > 40us, it is bit 1
    return (timeout > 40 ) ? 1: 0;
}
int dht22_read(float *temp, float *humi)
{
    // starting signal
    gpio_config_output();
    gpio_set_direction(DHT22_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT22_PIN, 0);
    esp_rom_delay_us(1000); // hold for at least 1ms
    gpio_set_level(DHT22_PIN, 1);
    esp_rom_delay_us(30); // wait for 20-40us
    
    gpio_config_input();
    gpio_set_direction(DHT22_PIN, GPIO_MODE_INPUT);
    int timeout = 0;
    // wait for DHT22 response
    while(gpio_get_level(DHT22_PIN) == 0)
    {
        timeout++;
        if(timeout > 100)
        {
            return -1; // timeout error
        }
        esp_rom_delay_us(1);
    }

    // prepare read data from DHt22
    timeout = 0;
    while(gpio_get_level(DHT22_PIN) == 1)
    {
        timeout++;
        if(timeout > 100)
        {
            return -1; // timeout error
        }
        esp_rom_delay_us(1);
    }

    // read 40 bits data
    uint8_t data[5] = {0};
    for(int i=0; i< 40; i++)
    {
        int bit = dht_read_bit(); // RH bit 
        if(bit == -1)
        {
            return -1; // timeout error
        }
        data[i/8] = (data[i/8] << 1) | bit;
    }
    // pares data
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if(checksum != data[4])
    {
        return -2; // checksum error
    }
    *humi = (data[0] << 8 | data[1])*0.1;
    *temp = (data[2] << 8 | data[3])*0.1;
    return 1; // success
}
