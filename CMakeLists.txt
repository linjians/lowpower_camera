# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
string(TIMESTAMP COMPILE_TIME %Y%m%d)        # Generate compile time
set(PROJECT_VER "NE_101.1.0.1")             # # NE_101.2.x.x HALOW CE version NE_101.1.x.x HALOW FCC version
project(NE101_${COMPILE_TIME})            # Firmware Name
