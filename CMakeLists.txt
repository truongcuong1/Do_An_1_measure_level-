cmake_minimum_required(VERSION 3.16) 
# phần mềm CMake từ phiên bản 3.16 trở lên thì mới được phép biên dịch dự án này.

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Dự án đo mức nước 0-1m bằng cảm biến siêu âm US-016 + ESP32, hiển thị LCD qua I2C
project(esp32_us016_level_monitor)