# ESP-IDF implementations of the sensor_core HAL plus ESP-NOW link helpers.
idf_component_register(
    SRCS src/esp_hal.cpp src/espnow_link.cpp
    INCLUDE_DIRS include
    REQUIRES sensor_core esp_driver_gpio esp_timer esp_wifi esp_event nvs_flash
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -std=gnu++20)
