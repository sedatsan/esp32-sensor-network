# Platform-independent core: HAL interfaces, drivers, protocol and node logic.
# Built as an ESP-IDF component on target and as a plain library for host tests.
set(SENSOR_CORE_SRCS
    src/hcsr04.cpp
    src/pir.cpp
    src/report.cpp
    src/node_controller.cpp
    src/gateway_tracker.cpp
    src/telemetry.cpp
    src/gateway_settings.cpp
)

if(ESP_PLATFORM)
    idf_component_register(SRCS ${SENSOR_CORE_SRCS} INCLUDE_DIRS include)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -std=gnu++20)
else()
    add_library(sensor_core STATIC ${SENSOR_CORE_SRCS})
    target_include_directories(sensor_core PUBLIC include)
    target_compile_features(sensor_core PUBLIC cxx_std_20)
    target_compile_options(sensor_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
endif()
