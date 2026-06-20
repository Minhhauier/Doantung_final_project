idf_component_register(SRCS "l9110s.c"
                    INCLUDE_DIRS include
                    REQUIRES
                        driver
                        freertos
                        esp_timer
                        config_parameter
                   
)
