#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "driver/uart.h"
#include "gps_neo6m.h"
#include "at_command.h"
#include "config_parameter.h"
#include "rfid_rc522.h"
#include "l9110s.h"
#include <driver/gpio.h>
#include "esp_sleep.h"


static const char *TAG = "MAIN";

void init_rfid_irq_pin(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 21),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Thường chân IRQ tích cực thấp nên cần Pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE // Không cần ngắt FreeRTOS thông thường, ta dùng ngắt Sleep
    };
    gpio_config(&io_conf);
}

void app_main(void)
{
    // esp_log_level_set("*", ESP_LOG_NONE);
    gps_neo6m_init();
    at_command_init();
    rfid_rc522_init();
    l9110s_lock_init();
    init_queues();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    mqtt_init();

    xTaskCreate(read_and_send_to_queue_task, "sim_rx_task",  1024 * 8, NULL, 5, NULL);
    xTaskCreate(read_gps_data_task,          "gps_task",     4096,     NULL, 5, NULL);
    xTaskCreate(rfid_rc522_task,             "rfid_task",    8192,     NULL, 4, NULL);
    xTaskCreate(l9110s_lock_task,            "lock_task",    3072,     NULL, 6, NULL);
    xTaskCreate(queue_data_task,           "queue_data_task", 1024 * 8, NULL, 5, NULL);
   
   // enable sleep mode
    uart_set_wakeup_threshold(UART_SIM_NUM, 1);
    esp_sleep_enable_uart_wakeup(UART_SIM_NUM);
    esp_sleep_enable_timer_wakeup(30*1000*1000); //30 seconds
    init_rfid_irq_pin();
    esp_sleep_enable_ext0_wakeup(21, 0);
    esp_light_sleep_start();
    while (1) {
        // gpio_set_level(BUZZER_PIN, 1);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        // gpio_set_level(BUZZER_PIN, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
