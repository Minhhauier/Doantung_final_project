#include "at_command.h"

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include <stdbool.h>

#include "config_parameter.h"
//define global variable
static char *MQTT_TAG = "MQTT";
char data[BUF_SIZE_SIM];
bool mqtt_sub_success = false;
//for normal at command
void at_command_init(void)
{
    uart_config_t config = {
        .baud_rate = SIM_BaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_SIM_NUM, &config);
    uart_set_pin(UART_SIM_NUM, SIM_TX, SIM_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_SIM_NUM, BUF_SIZE_SIM, 0, 0, NULL, 0);
}

void send_at(const char *command){
    ESP_LOGI("AT", "Sending command: %s", command);
    uart_write_bytes(UART_SIM_NUM, command, strlen(command));
    uart_write_bytes(UART_SIM_NUM, "\r\n", 2);
}


static void read_sim_respond(int timeout)
{
    int total_len = 0;
    int count = 0;
    bool got_data = false;
    for (int i = 0; i < timeout; i += 100)
    {
        int len = uart_read_bytes(UART_SIM_NUM, data + total_len, BUF_SIZE_SIM - total_len - 1, 100 / portTICK_PERIOD_MS);
        if (len > 0)
        {
            total_len = total_len + len;
            got_data = true;
            count = 0;
        }
        else if (got_data)
        {
            count++;
        }
        if (got_data && count > 3)
        {
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    if (total_len > 0)
    {
        data[total_len] = '\0';
        ESP_LOGI("SIM", "RX: %s", data);
    }
}

void send_at_get_respond(const char *cmd, int timeout)
{
    ESP_LOGI("SIM", "sent: %s", cmd);
    uart_flush_input(UART_SIM_NUM);
    uart_write_bytes(UART_SIM_NUM, cmd, strlen(cmd));
    uart_write_bytes(UART_SIM_NUM, "\r\n", 2);
    read_sim_respond(timeout);
}

void send_at_data_get_respond(const char *data_cmd, int timeout)
{
    ESP_LOGI("SIM", "sent data: %s", data_cmd);
    uart_write_bytes(UART_SIM_NUM, data_cmd, strlen(data_cmd));
    read_sim_respond(timeout);
}

// void init_queues() {
//     sim_at_queue_handle = xQueueCreate(10, BUF_SIZE_SIM);
//     mqtt_queue_handle = xQueueCreate(10, BUF_SIZE_SIM);
//     gps_queue_handle = xQueueCreate(10, BUF_SIZE_SIM);
//     publish_queue_handle = xQueueCreate(10, BUF_SIZE_SIM);
//     queue_created = 1;
// }
void read_and_send_to_queue_task(void *pvParameters)
{
    char *data_receiver=malloc(2048);
    char data_copy[2048];
   // printf("Start read_and_send_to_queue_task\r\n");
    while (1)
    {
        int len = uart_read_bytes(UART_SIM_NUM, data_receiver, 2048, 30 / portTICK_PERIOD_MS);
        if (len > 0)
        {
            data_receiver[len] = '\0';
            // printf("data_rx: %s\r\n",data_receiver);
            if (strstr(data_receiver, "+QMTRECV:") != NULL)
            {
                memcpy(data_copy,data_receiver,len+1);
            
                printf("data_rx: %s\r\n",data_receiver);
                //  convert_to_json_update(data_copy);
                //   printf("Send to mqtt queue: %s\r\n", data_receiver);
                //  xQueueSend(mqtt_queue_handle, data_receiver, portMAX_DELAY);
            }
        }
    }
    free(data_receiver);
}
//for mqtt at command

// void mqtt_connect(){
//     send_at_get_respond("AT+QMTCFG=\"keepalive\",1,60",1000);
//     snprintf(cmd,sizeof(cmd),"AT+QMTOPEN=1,%s",addr);
//     send_at_get_respond(cmd,1000);
//     snprintf(client, sizeof(client), "%s_%s", private_topic[enviroment], device_name);
//     snprintf(cmd,sizeof(cmd),"AT+QMTCONN=1,\"%s\",[\"%s\",\"%s\"]",client,username,password);
//     send_at_get_respond(cmd,1000);
// }

// void mqtt_sub(char *gen_subtopic, char *prv_subtopic){
//     char cmd_sub[256];
//     snprintf(cmd_sub,sizeof(cmd_sub),"AT+QMTSUB=1,1,%s,0,%s_%s,0",gen_subtopic,prv_subtopic, device_name);
//     send_at(cmd_sub);
//     char *data = get_respond(1000);
//     if(data!=NULL){
//         if(strstr(data,"+QMTSUB: 1,1,0,0,0")!=NULL){ printf("Sub success\r\n"); mqtt_sub_success = true;}
//         else mqtt_sub_success = false;
//     }
//     else {
//         mqtt_sub_success = false;
//         printf("No data respond\r\n");
//     }
//     free(data);
// }


// void  mqtt_init(){
//     int count=0;
//     mqtt_connect();
//     mqtt_sub(MQTT_SUBTOPIC, MQTT_PRIVATE_TOPIC);
//     while (mqtt_sub_success == false)
//     {
//         count=count+100;
//         mqtt_connect();
//         mqtt_sub(MQTT_SUBTOPIC, MQTT_PRIVATE_TOPIC);
//         if(count>1000) {
//             count=0;
//             send_at("AT+CFUN=1,1");
//             vTaskDelay(1000/portTICK_PERIOD_MS);
//         }
//     }
//     ESP_LOGI(MQTT_TAG,"Connect and subscribe to mqtt success");
// }