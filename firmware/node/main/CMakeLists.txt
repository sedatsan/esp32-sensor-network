idf_component_register(SRCS main.cpp REQUIRES sensor_core esp_platform esp_wifi esp_hw_support)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -std=gnu++20)
