#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "gps_neo6m.h"


void app_main(void)
{
    gps_neo6m_init();
    xTaskCreate(read_gps_data_task, "read_gps_data_task", 4096, NULL, 5, NULL);
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
