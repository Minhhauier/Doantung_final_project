#ifndef CONFIG_PARAMETER
#define CONFIG_PARAMETER

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/uart.h>

//define parameter for sim
#define BUF_SIZE_SIM            2048
#define UART_SIM_NUM            UART_NUM_2
#define SIM_AT_QUEUE_SIZE       10
#define SIM_TX                  2
#define SIM_RX                  4
#define SIM_BaudRate            115200
//define parameter for gps
#define GPS_UART_NUM            UART_NUM_1
#define GPS_BAUD_RATE           9600
#define GPS_DATA_SIZE           256
#define GPS_QUEUE_SIZE          10
#define GPS_TX                  16
#define GPS_RX                  17
#define GPS_BaudRate            9600
//define parameter for rfid rc522 (SPI2/HSPI default pins on ESP32 30-pin)
#define RFID_SPI_HOST           SPI2_HOST
#define RFID_SPI_FREQ_HZ        5000000     // 5 MHz
#define RFID_PIN_MOSI           23
#define RFID_PIN_MISO           19
#define RFID_PIN_CLK            18
#define RFID_PIN_CS             5
#define RFID_PIN_RST            32
#define RFID_QUEUE_SIZE         10
//define parameter for L9110S electronic bolt lock
#define LOCK_PIN_A              25   // L9110S B1A → turnLeft (mở khóa)
#define LOCK_PIN_B              26   // L9110S B1B → turnRight (đóng khóa)
#define BUZZER_PIN              33   // buzzer pin
#define LOCK_PIN_LIMIT          35   // C nối GPIO35, NO nối 3.3V → kích = HIGH (dùng khi MỞ)
#define LOCK_PWM_FREQ_HZ        5000
#define LOCK_PWM_DUTY           204  // 80% of 255 (8-bit)
#define LOCK_OPEN_TIMEOUT_MS    3000 // Timeout an toàn khi mở (limit hỏng)
#define LOCK_CLOSE_TIMEOUT_MS   2000 // Thời gian chạy motor để đóng (ms)
#define LOCK_DEBOUNCE_MS        50   // Debounce limit switch (ms)
//define parameter for mqtt
#define MQTT_BROKER_URL         "tcp://broker.chtlab.us:1883"
#define MQTT_BROKER_PORT        1883
#define MQTT_USERNAME           "username"
#define MQTT_PASSWORD           "password"
#define MQTT_CLIENT_ID          "minh_test_esp32"
#define MQTT_SUBTOPIC           "esp32/subtopic"
#define MQTT_PRIVATE_TOPIC       "esp32/private_topic"
#define MQTT_PUBTOPIC           "esp32/pubtopic"



#endif