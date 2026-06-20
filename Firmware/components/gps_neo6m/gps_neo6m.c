#include "gps_neo6m.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <driver/uart.h>
#include "esp_sleep.h"

#include "config_parameter.h"       
#include "at_command.h"

static const char *TAG = "GPS";

static bool nmea_get_field(const char *sentence, int target_field, char *out, size_t out_size)
{
    if (sentence == NULL || out == NULL || out_size == 0 || target_field < 0) {
        return false;
    }

    int current_field = 0;
    const char *field_start = sentence;
    const char *p = sentence;

    while (*p != '\0' && *p != '*') {
        if (*p == ',') {
            if (current_field == target_field) {
                size_t len = (size_t)(p - field_start);
                if (len >= out_size) {
                    len = out_size - 1;
                }
                memcpy(out, field_start, len);
                out[len] = '\0';
                return true;
            }
            current_field++;
            field_start = p + 1;
        }
        p++;
    }

    if (current_field == target_field) {
        size_t len = (size_t)(p - field_start);
        if (len >= out_size) {
            len = out_size - 1;
        }
        memcpy(out, field_start, len);
        out[len] = '\0';
        return true;
    }

    return false;
}

static double nmea_to_decimal(const char *nmea_coord, const char *direction)
{
    if (nmea_coord == NULL || nmea_coord[0] == '\0') {
        return 0.0;
    }

    double raw = atof(nmea_coord);
    int degrees = (int)(raw / 100);
    double minutes = raw - (degrees * 100);
    double decimal = degrees + (minutes / 60.0);

    if (direction != NULL && (direction[0] == 'S' || direction[0] == 'W')) {
        decimal = -decimal;
    }

    return decimal;
}

static void parse_gpgga(const char *sentence, gps_data_t *gps_data)
{
    char lat[20] = {0};
    char lat_dir[4] = {0};
    char lon[20] = {0};
    char lon_dir[4] = {0};
    char fix_quality[8] = {0};
    char satellites_used[8] = {0};

    nmea_get_field(sentence, 2, lat, sizeof(lat));
    nmea_get_field(sentence, 3, lat_dir, sizeof(lat_dir));
    nmea_get_field(sentence, 4, lon, sizeof(lon));
    nmea_get_field(sentence, 5, lon_dir, sizeof(lon_dir));

    gps_data->fix_quality = nmea_get_field(sentence, 6, fix_quality, sizeof(fix_quality))
                                ? atoi(fix_quality)
                                : 0;
    gps_data->satellites_used = nmea_get_field(sentence, 7, satellites_used, sizeof(satellites_used))
                                    ? atoi(satellites_used)
                                    : 0;

    gps_data->has_fix = (gps_data->fix_quality > 0 &&
                         lat[0] != '\0' &&
                         lon[0] != '\0');

    if (gps_data->has_fix) {
        gps_data->latitude = nmea_to_decimal(lat, lat_dir[0] != '\0' ? lat_dir : NULL);
        gps_data->longitude = nmea_to_decimal(lon, lon_dir[0] != '\0' ? lon_dir : NULL);
    }
}

static void parse_gpgsv(const char *sentence, gps_data_t *gps_data)
{
    char satellites_in_view_field[8] = {0};
    int satellites_in_view = nmea_get_field(sentence, 3, satellites_in_view_field, sizeof(satellites_in_view_field))
                                 ? atoi(satellites_in_view_field)
                                 : 0;

    if (satellites_in_view > gps_data->satellites_in_view) {
        gps_data->satellites_in_view = satellites_in_view;
    }
}

void gps_parse_nmea_line(const char *line, gps_data_t *gps_data)
{
    if (line == NULL || line[0] != '$') {
        return;
    }

    if (strncmp(line, "$GPGGA", 6) == 0 || strncmp(line, "$GNGGA", 6) == 0) {
        parse_gpgga(line, gps_data);
    } else if (strncmp(line, "$GPGSV", 6) == 0 || strncmp(line, "$GNGSV", 6) == 0) {
        parse_gpgsv(line, gps_data);
    }
}

void gps_parse_nmea(const char *nmea_data, gps_data_t *gps_data)
{
    char buffer[1024];
    strncpy(buffer, nmea_data, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *saveptr = NULL;
    char *line = strtok_r(buffer, "\r\n", &saveptr);
    while (line != NULL) {
        gps_parse_nmea_line(line, gps_data);
        line = strtok_r(NULL, "\r\n", &saveptr);
    }
}

void gps_print_data(const gps_data_t *gps_data)
{
    ESP_LOGI(TAG, "Số lượng vệ tinh nhìn thấy: %d", gps_data->satellites_in_view);
    ESP_LOGI(TAG, "Số lượng vệ tinh dùng để fix: %d", gps_data->satellites_used);

    if (gps_data->has_fix) {
        ESP_LOGI(TAG, "Vĩ tuyến: %.6f", gps_data->latitude);
        ESP_LOGI(TAG, "Kinh tuyến: %.6f", gps_data->longitude);
        if(mqtt_sub_success)
        {
            publish_gpsposition(gps_data->latitude, gps_data->longitude);
        }
    } else {
        ESP_LOGI(TAG, "Chua co GPS fix (fix_quality=%d).", gps_data->fix_quality);
    }
}

void gps_neo6m_init()
{
    uart_config_t config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(GPS_UART_NUM, &config);
    uart_set_pin(GPS_UART_NUM, GPS_TX, GPS_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(GPS_UART_NUM, GPS_DATA_SIZE * 4, 0, 0, NULL, 0);
}

void read_gps_data_task(void *pvParameters)
{
    char line_buffer[256];
    size_t line_len = 0;
    gps_data_t gps_data = {
        .satellites_in_view = 0,
        .satellites_used = 0,
        .fix_quality = 0,
        .latitude = 0.0,
        .longitude = 0.0,
        .has_fix = false,
    };
    TickType_t last_time = xTaskGetTickCount();
    TickType_t weakup_time = xTaskGetTickCount();
    while (1) {
        weakup_time = xTaskGetTickCount();
        uint8_t byte;
        int len = uart_read_bytes(GPS_UART_NUM, &byte, 1, 100 / portTICK_PERIOD_MS);
        if (len <= 0) {
            continue;
        }

        if (byte == '\n') {
            if (line_len > 0) {
                line_buffer[line_len] = '\0';

                if (strncmp(line_buffer, "$GPRMC", 6) == 0 || strncmp(line_buffer, "$GNRMC", 6) == 0) {
                    gps_data.satellites_in_view = 0;
                }

                // ESP_LOGI(TAG, "UART raw: %s", line_buffer);
                gps_parse_nmea_line(line_buffer, &gps_data);

                if (strncmp(line_buffer, "$GPGLL", 6) == 0 || strncmp(line_buffer, "$GNGLL", 6) == 0) {
                    if(xTaskGetTickCount() - last_time > 5000/portTICK_PERIOD_MS)
                    {
                        last_time = xTaskGetTickCount();
                        gps_print_data(&gps_data);
                    }
                }
            }
            line_len = 0;
            continue;
        }

        if (byte == '\r') {
            continue;
        }

        if (line_len < sizeof(line_buffer) - 1) {
            line_buffer[line_len++] = (char)byte;
        } else {
            line_len = 0;
        }
        if(xTaskGetTickCount() - weakup_time > 10000 / portTICK_PERIOD_MS) {
            esp_light_sleep_start();
            weakup_time = xTaskGetTickCount();
        }

        // vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
