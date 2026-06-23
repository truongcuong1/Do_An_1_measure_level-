
esp32_us016_level_monitor/
├── CMakeLists.txt
├── sdkconfig.defaults
├── README.md
├── main/
│   ├── CMakeLists.txt
│   ├── app_config.h
│   └── main.c
└── components/
    ├── signal_filter/      (lọc median + EMA, không phụ thuộc IDF)
    │   ├── CMakeLists.txt
    │   ├── include/signal_filter.h
    │   └── signal_filter.c
    ├── level_sensor/       (đọc ADC hiệu chỉnh + quy đổi khoảng cách)
    │   ├── CMakeLists.txt
    │   ├── include/level_sensor.h
    │   └── level_sensor.c
    ├── lcd_pcf8574/        (driver LCD 16x2 qua I2C)
    │   ├── CMakeLists.txt
    │   ├── include/lcd_pcf8574.h
    │   └── lcd_pcf8574.c
    └── level_storage/      (lưu hệ số hiệu chỉnh vào NVS)
        ├── CMakeLists.txt
        ├── include/level_storage.h
        └── level_storage.c