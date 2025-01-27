idf_component_register(SRCS "main.c" "wifi.c" "http.c" "gpio.c"
                    PRIV_REQUIRES esp_wifi nvs_flash esp_adc esp_http_client esp-tls esp_driver_gpio
                    INCLUDE_DIRS ".")
