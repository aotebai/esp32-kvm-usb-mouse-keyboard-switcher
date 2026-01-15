idf_component_register(SRCS "ch9350_led_switch.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver freertos esp_timer)

# 优化编译选项，减小固件体积
target_compile_options(${COMPONENT_LIB} PRIVATE -Os -ffunction-sections -fdata-sections)
target_link_options(${COMPONENT_LIB} PRIVATE -Wl,--gc-sections)

# 适配ESP-IDF v5.5.1，禁用无用功能
idf_build_set_property(CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240 OFF)
idf_build_set_property(CONFIG_ESP32S3_DEFAULT_CPU_FREQ_160 ON)
idf_build_set_property(CONFIG_LOG_DEFAULT_LEVEL_INFO ON)
idf_build_set_property(CONFIG_UART_USE_REFCOUNT OFF)
idf_build_set_property(CONFIG_RMT_USE_REFCOUNT OFF)

