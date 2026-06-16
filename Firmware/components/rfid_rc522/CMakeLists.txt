idf_component_register(SRCS "rfid_rc522.c"
                    INCLUDE_DIRS include
                    REQUIRES
                        driver
                        freertos
                        config_parameter
                        at_command
)
