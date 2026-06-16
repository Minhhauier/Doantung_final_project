idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        gps_neo6m
                        at_command
                        config_parameter
                        rfid_rc522
                    )
