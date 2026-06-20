# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Component l9110s nằm lồng trong components/at_command/l9110s
# nên cần khai báo thêm để ESP-IDF tự phát hiện
set(EXTRA_COMPONENT_DIRS "components/at_command/l9110s")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(main)
