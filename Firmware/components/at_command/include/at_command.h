#ifndef AT_COMMAND_H
#define AT_COMMAND_H

#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

extern bool mqtt_sub_success;
extern QueueHandle_t publish_queue_handle;
void read_and_send_to_queue_task(void *pvParameters);
void send_at(const char *command);
void send_at_get_respond(const char *cmd, int timeout);
void send_at_data_get_respond(const char *data_cmd, int timeout);
void init_queues();
void at_command_init(void);
void mqtt_connect();
void mqtt_sub(const char *subtopic);
void mqtt_init();
void mqtt_publish(const char *topic, const char *data);
void publish_gpsposition(float latitude_decimal,float longitude_decimal);
void queue_data_task(void *pvParameters);
#endif