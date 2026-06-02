# Compress web assets at build time using ESP-IDF's bundled Python.
idf_build_get_property(python PYTHON)
set(COMPRESS_SCRIPT "${CMAKE_SOURCE_DIR}/main/app_files/compress.py")

set(GZ_DIR "${CMAKE_CURRENT_BINARY_DIR}/app_files_gz")
file(GLOB_RECURSE APP_SOURCE_FILES "app_files/files/*")

set(APP_COMPRESSED_FILES "")
foreach(SRC ${APP_SOURCE_FILES})
    get_filename_component(FNAME ${SRC} NAME)
    set(GZ_FILE "${GZ_DIR}/${FNAME}.gz")
    if(NOT CMAKE_SCRIPT_MODE_FILE)
        add_custom_command(
            OUTPUT  ${GZ_FILE}
            COMMAND ${python} ${COMPRESS_SCRIPT} ${SRC} ${GZ_FILE}
            DEPENDS ${SRC} ${COMPRESS_SCRIPT}
            VERBATIM
        )
    endif()
    list(APPEND APP_COMPRESSED_FILES ${GZ_FILE})
endforeach()

if(NOT CMAKE_SCRIPT_MODE_FILE)
    add_custom_target(compress_app_files DEPENDS ${APP_COMPRESSED_FILES})
endif()

idf_component_register(
    SRCS
        app_main.cpp
        LoggingConfig.cpp
        app/ApplicationContext.cpp
        app/ActivityManager.cpp
        app/TemperatureHandler.cpp
        app/StatusHandler.cpp
        app/CalibrateHandler.cpp
        app/VenusConfigHandler.cpp
        app/VenusMqttClient.cpp
        app/WaterLevelSensor.cpp
        app/ImuSensor.cpp
        app_files/AppFileTable.cpp
    REQUIRES
        auth
        device
        display
        esp_platform
        network_store
        framework
        framework_files
        http
        http_types
        logger
        ota
        wifi_manager
        driver
        nvs_flash
        esp_adc
        esp_driver_tsens
        esp_wifi
        esp_netif
        app_update
    INCLUDE_DIRS
        .
        app
        app_files
    EMBED_FILES
        ${APP_COMPRESSED_FILES}
)

if(NOT CMAKE_SCRIPT_MODE_FILE)
    add_dependencies(${COMPONENT_LIB} compress_app_files)
endif()
