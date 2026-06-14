#ifndef AT_COMMAND_H
#define AT_COMMAND_H

#include <stdbool.h>

extern bool mqtt_sub_success;

void read_and_send_to_queue_task(void *pvParameters);
void send_at(const char *command);
void send_at_get_respond(const char *cmd, int timeout);
void send_at_data_get_respond(const char *data_cmd, int timeout);
void at_command_init(void);
void mqtt_connect();
void mqtt_sub(const char *subtopic);
void mqtt_init();
void mqtt_publish(const char *topic, const char *data);
void publish_gpsposition(float latitude_decimal,float longitude_decimal);

#endif