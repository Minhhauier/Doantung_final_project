idf_component_register(SRCS "at_command.c"
                    INCLUDE_DIRS include
                    REQUIRES
                        driver
                        freertos
                        config_parameter
                        json
                        rfid_rc522
                        l9110s
                        driver
)