idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        level_sensor
        signal_filter
        lcd_pcf8574
        level_storage
        driver
        console
        freertos
        log
)