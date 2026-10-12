idf_component_register(
    SRCS main.cpp settings_store.cpp setup_portal.cpp uplink.cpp
    REQUIRES sensor_core esp_platform esp_wifi esp_netif esp_hw_support esp_http_server
             esp_timer mqtt mbedtls nvs_flash
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -std=gnu++20)
