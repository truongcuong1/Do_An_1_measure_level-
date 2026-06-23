idf_component_register(
    SRCS "level_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash log
)