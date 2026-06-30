# GPS NMEA parser and MQTT telemetry bridge

# Paho MQTT C++ depends on the Paho MQTT C library. On Ubuntu 22.04+ both can be
# installed via apt:
#   sudo apt install libpaho-mqtt-dev libpaho-mqttpp-dev
# Alternatively, set PAHO_MQTT_CPP_DIR and PAHO_MQTT_C_DIR to point to locally
# built install trees.

set(PAHO_MQTT_CPP_DIR "" CACHE PATH "Path to Paho MQTT C++ install prefix (optional)")
set(PAHO_MQTT_C_DIR "" CACHE PATH "Path to Paho MQTT C install prefix (optional)")

if (PAHO_MQTT_CPP_DIR)
    list(APPEND CMAKE_PREFIX_PATH ${PAHO_MQTT_CPP_DIR})
endif ()
if (PAHO_MQTT_C_DIR)
    list(APPEND CMAKE_PREFIX_PATH ${PAHO_MQTT_C_DIR})
endif ()

# Try CMake config first (C library provides one)
find_package(eclipse-paho-mqtt-c QUIET)

if (eclipse-paho-mqtt-c_FOUND)
    if (NOT TARGET paho-mqtt3c)
        add_library(paho-mqtt3c ALIAS eclipse-paho-mqtt-c::paho-mqtt3c)
    endif ()
else ()
    find_path(PAHO_MQTT_C_INCLUDE_DIR MQTTAsync.h
            PATHS ${PAHO_MQTT_C_DIR}/include /usr/include /usr/local/include)
    find_library(PAHO_MQTT_C_LIBRARY NAMES paho-mqtt3c paho-mqtt3cs
            PATHS ${PAHO_MQTT_C_DIR}/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu /usr/local/lib)

    if (NOT PAHO_MQTT_C_INCLUDE_DIR OR NOT PAHO_MQTT_C_LIBRARY)
        message(FATAL_ERROR "Paho MQTT C library not found. Install libpaho-mqtt-dev or set PAHO_MQTT_C_DIR.")
    endif ()

    add_library(paho-mqtt3c SHARED IMPORTED GLOBAL)
    set_target_properties(paho-mqtt3c PROPERTIES
            IMPORTED_LOCATION ${PAHO_MQTT_C_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${PAHO_MQTT_C_INCLUDE_DIR})
endif ()

find_path(PAHO_MQTT_CPP_INCLUDE_DIR mqtt/async_client.h
        PATHS ${PAHO_MQTT_CPP_DIR}/include /usr/include /usr/local/include)
find_library(PAHO_MQTT_CPP_LIBRARY NAMES paho-mqttpp3
        PATHS ${PAHO_MQTT_CPP_DIR}/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu /usr/local/lib)

if (NOT PAHO_MQTT_CPP_INCLUDE_DIR OR NOT PAHO_MQTT_CPP_LIBRARY)
    message(FATAL_ERROR "Paho MQTT C++ library not found. Install libpaho-mqttpp-dev or set PAHO_MQTT_CPP_DIR.")
endif ()

add_library(paho-mqttpp3 SHARED IMPORTED GLOBAL)
set_target_properties(paho-mqttpp3 PROPERTIES
        IMPORTED_LOCATION ${PAHO_MQTT_CPP_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${PAHO_MQTT_CPP_INCLUDE_DIR})
target_link_libraries(paho-mqttpp3 INTERFACE paho-mqtt3c Threads::Threads)
add_library(PahoMqttCpp::PahoMqttCpp ALIAS paho-mqttpp3)

add_executable(gps_mqtt
        main.cpp
        program_options.cpp
        nmea_parser.cpp
        serial_reader.cpp
        mqtt_client.cpp
        json_utils.cpp
        emergency_listener.cpp
)

target_link_libraries(gps_mqtt PRIVATE
        cxxopts::cxxopts
        spdlog::spdlog
        iceoryx2-cxx::iceoryx2-cxx
        PahoMqttCpp::PahoMqttCpp
        Threads::Threads
)

set_target_properties(gps_mqtt PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

install(TARGETS gps_mqtt RUNTIME DESTINATION bin)
