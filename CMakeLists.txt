# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD OFF)
project(rf-bridge-cc1101)

target_add_binary_data(${PROJECT_NAME}.elf "main/isrgrootx1.pem" TEXT)
target_add_binary_data(${PROJECT_NAME}.elf "main/discovery_payload.json" TEXT)
