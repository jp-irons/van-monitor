cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/framework/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)

# After the build produces <project>.bin, create a versioned copy alongside it.
# PROJECT_VER is populated automatically from version.txt by ESP-IDF's project.cmake.
# Result: build/my_app-0.1.0.bin
add_custom_command(
    TARGET app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin"
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VER}.bin"
    VERBATIM
    COMMENT "OTA binary → ${PROJECT_NAME}-${PROJECT_VER}.bin"
)
