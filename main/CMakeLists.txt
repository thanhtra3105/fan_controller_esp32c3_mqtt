idf_component_register(SRCS "app_main.c"
                    PRIV_REQUIRES mqtt nvs_flash esp_netif esp_wifi esp_driver_gpio esp_adc
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "certs/isrgrootx1.pem")
