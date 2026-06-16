#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "gps_neo6m.h"
#include "at_command.h"
#include "config_parameter.h"
#include "rfid_rc522.h"

char cmd[256];
void app_main(void)
{
    gps_neo6m_init();
    at_command_init();
    rfid_rc522_init();

    // xTaskCreate(read_and_send_to_queue_task, "read_and_send_to_queue_task", 4096, NULL, 5, NULL);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    mqtt_init();
    xTaskCreate(read_and_send_to_queue_task, "read_and_send_to_queue_task", 1024*8, NULL, 5, NULL);
    xTaskCreate(read_gps_data_task, "read_gps_data_task", 4096, NULL, 5, NULL);
    xTaskCreate(rfid_rc522_task, "rfid_rc522_task", 4096, NULL, 4, NULL);
    // send_at_get_respond("ATE0", 1000);
    // send_at_get_respond("AT+CMQTTSTART", 5000);
    // send_at_get_respond("AT+CMQTTACCQ=0,\"clienttest0\"", 2000);
    // snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=0,\"%s\",60,1", MQTT_BROKER_URL);
    // send_at_get_respond(cmd, 10000);
    // snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=0,%d,1", (int)strlen(MQTT_SUBTOPIC));
    // send_at_get_respond(cmd, 2000);
    // send_at_data_get_respond(MQTT_SUBTOPIC, 5000);
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
