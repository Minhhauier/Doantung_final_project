#ifndef CONFIG_PARAMETER
#define CONFIG_PARAMETER

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

//define parameter for sim
#define BUF_SIZE_SIM            2048
#define UART_SIM_NUM            UART_NUM_0
#define SIM_AT_QUEUE_SIZE       10
#define SIM_TX                  GPIO_NUM_26
#define SIM_RX                  GPIO_NUM_27
#define SIM_BaudRate            115200
//define parameter for gps
#define GPS_UART_NUM            UART_NUM_1
#define GPS_BAUD_RATE           9600
#define GPS_DATA_SIZE           256
#define GPS_QUEUE_SIZE          10
#define GPS_TX                  16
#define GPS_RX                  17
#define GPS_BaudRate            9600
//define parameter for mqtt
#define MQTT_BROKER_URL         "broker.chtlab.us"
#define MQTT_BROKER_PORT        1883
#define MQTT_USERNAME           "username"
#define MQTT_PASSWORD           "password"
#define MQTT_CLIENT_ID          "minh_test_esp32"
#define MQTT_SUBTOPIC           "esp32/subtopic"
#define MQTT_PUBTOPIC           "esp32/pubtopic"



#endif