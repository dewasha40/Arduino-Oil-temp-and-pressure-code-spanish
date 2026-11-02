idf_component_register(SRCS "main.cpp" "gc9b72_spi.cpp" "hella_reader.cpp" INCLUDE_DIRS "." REQUIRES lvgl driver esp_driver_gpio esp_driver_spi esp_timer esp_adc)
