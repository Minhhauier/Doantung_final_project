#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_err.h>

#include "rfid_rc522.h"
#include "config_parameter.h"
#include "at_command.h"
#include "l9110s.h"

static const char *TAG = "RFID_RC522";

/* ───────────────────────────── Registers ──────────────────────────────── */
#define REG_COMMAND         0x01
#define REG_COM_I_EN        0x02
#define REG_DIV_I_EN        0x03
#define REG_COM_IRQ         0x04
#define REG_DIV_IRQ         0x05
#define REG_ERROR           0x06
#define REG_FIFO_DATA       0x09
#define REG_FIFO_LEVEL      0x0A
#define REG_CONTROL         0x0C
#define REG_BIT_FRAMING     0x0D
#define REG_COLL            0x0E
#define REG_MODE            0x11
#define REG_TX_CONTROL      0x14
#define REG_TX_ASK          0x15
#define REG_CRC_RESULT_H    0x21
#define REG_CRC_RESULT_L    0x22
#define REG_T_MODE          0x2A
#define REG_T_PRESCALER     0x2B
#define REG_T_RELOAD_H      0x2C
#define REG_T_RELOAD_L      0x2D
#define REG_VERSION         0x37

/* ───────────────────────────── PCD commands ────────────────────────────── */
#define CMD_IDLE            0x00
#define CMD_CALC_CRC        0x03
#define CMD_TRANSCEIVE      0x0C
#define CMD_SOFT_RESET      0x0F

/* ───────────────────────────── PICC commands ───────────────────────────── */
#define PICC_REQA           0x26
#define PICC_SEL_CL1        0x93
#define PICC_SEL_CL2        0x95
#define PICC_SEL_CL3        0x97
#define PICC_HLTA           0x50
#define CASCADE_TAG         0x88

/* ────────────────────────────── Internals ──────────────────────────────── */
static spi_device_handle_t s_spi = NULL;

/*
 * Shared static transaction — safe because all SPI access is done exclusively
 * from rfid_rc522_task (single task, no concurrent access).
 */
static spi_transaction_t s_trans;

/* SPI helpers --------------------------------------------------------------- */
static void pcd_write_reg(uint8_t reg, uint8_t val)
{
    memset(&s_trans, 0, sizeof(s_trans));
    s_trans.length      = 16;
    s_trans.flags       = SPI_TRANS_USE_TXDATA;
    s_trans.tx_data[0]  = (uint8_t)((reg << 1) & 0x7E);
    s_trans.tx_data[1]  = val;
    spi_device_polling_transmit(s_spi, &s_trans);
}

static uint8_t pcd_read_reg(uint8_t reg)
{
    memset(&s_trans, 0, sizeof(s_trans));
    s_trans.length      = 16;
    s_trans.rxlength    = 16;
    s_trans.flags       = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    s_trans.tx_data[0]  = (uint8_t)(((reg << 1) & 0x7E) | 0x80);
    s_trans.tx_data[1]  = 0x00;
    spi_device_polling_transmit(s_spi, &s_trans);
    return s_trans.rx_data[1];
}

static void pcd_set_bits(uint8_t reg, uint8_t mask)
{
    pcd_write_reg(reg, pcd_read_reg(reg) | mask);
}

static void pcd_clear_bits(uint8_t reg, uint8_t mask)
{
    pcd_write_reg(reg, pcd_read_reg(reg) & ~mask);
}

/* CRC ----------------------------------------------------------------------- */
static rfid_status_t pcd_calc_crc(const uint8_t *data, uint8_t len,
                                   uint8_t *out_lsb, uint8_t *out_msb)
{
    pcd_write_reg(REG_COMMAND,    CMD_IDLE);
    pcd_write_reg(REG_DIV_IRQ,    0x04);
    pcd_write_reg(REG_FIFO_LEVEL, 0x80);

    for (uint8_t i = 0; i < len; i++) {
        pcd_write_reg(REG_FIFO_DATA, data[i]);
    }
    pcd_write_reg(REG_COMMAND, CMD_CALC_CRC);

    for (int i = 5000; i > 0; i--) {
        if (pcd_read_reg(REG_DIV_IRQ) & 0x04) {
            pcd_write_reg(REG_COMMAND, CMD_IDLE);
            *out_lsb = pcd_read_reg(REG_CRC_RESULT_L);
            *out_msb = pcd_read_reg(REG_CRC_RESULT_H);
            return RFID_OK;
        }
    }
    return RFID_ERR_TIMEOUT;
}

/* Transceive --------------------------------------------------------------- */
static rfid_status_t pcd_transceive(const uint8_t *send_data, uint8_t send_len,
                                     uint8_t *recv_data, uint8_t *recv_len,
                                     uint8_t tx_last_bits)
{
    pcd_write_reg(REG_COMMAND,     CMD_IDLE);
    pcd_write_reg(REG_COM_IRQ,     0x7F);
    pcd_write_reg(REG_FIFO_LEVEL,  0x80);

    for (uint8_t i = 0; i < send_len; i++) {
        pcd_write_reg(REG_FIFO_DATA, send_data[i]);
    }
    pcd_write_reg(REG_BIT_FRAMING, tx_last_bits & 0x07);
    pcd_write_reg(REG_COMMAND,     CMD_TRANSCEIVE);
    pcd_set_bits(REG_BIT_FRAMING,  0x80);  /* StartSend */

    int deadline  = 36;
    bool completed = false;
    do {
        uint8_t irq = pcd_read_reg(REG_COM_IRQ);
        if (irq & 0x30) { completed = true; break; }  /* RxIRq | IdleIRq */
        if (irq & 0x01) { break; }                     /* TimerIRq */
        vTaskDelay(1);
    } while (--deadline);

    pcd_clear_bits(REG_BIT_FRAMING, 0x80);
    if (!completed) return RFID_ERR_TIMEOUT;
    if (pcd_read_reg(REG_ERROR) & 0x13) return RFID_ERR_GENERIC;

    uint8_t n = pcd_read_reg(REG_FIFO_LEVEL);
    if (recv_len) {
        if (n > *recv_len) n = *recv_len;
        *recv_len = n;
    } else {
        n = 0;
    }

    if (recv_data) {
        for (uint8_t i = 0; i < n; i++) {
            recv_data[i] = pcd_read_reg(REG_FIFO_DATA);
        }
    } else {
        for (uint8_t i = 0; i < n; i++) {
            pcd_read_reg(REG_FIFO_DATA); /* drain FIFO */
        }
    }
    return RFID_OK;
}

/* REQA --------------------------------------------------------------------- */
static rfid_status_t picc_reqa(void)
{
    /* REQA uses 7-bit frame (short frame) — TxLastBits = 7 */
    uint8_t cmd      = PICC_REQA;
    uint8_t atqa[2]  = {0};
    uint8_t resp_len = 2;

    pcd_clear_bits(REG_COLL, 0x80);
    rfid_status_t st = pcd_transceive(&cmd, 1, atqa, &resp_len, 7);
    if (st != RFID_OK)   return st;
    if (resp_len != 2)   return RFID_ERR_GENERIC;
    return RFID_OK;
}

/* HLTA --------------------------------------------------------------------- */
static void picc_hlta(void)
{
    uint8_t buf[4] = { PICC_HLTA, 0x00, 0x00, 0x00 };
    pcd_calc_crc(buf, 2, &buf[2], &buf[3]);
    uint8_t recv_len = 0;
    pcd_transceive(buf, 4, NULL, &recv_len, 0);
}

/* SELECT / anti-collision ---------------------------------------------------
 *
 * Simplified single-shot anti-collision (NVB=0x20, no known bits).
 * Works correctly when only one card is in the field at a time.
 *
 * Algorithm per ISO 14443-3:
 *  For each cascade level (CL1 → CL2 → CL3):
 *    1. Send anti-collision frame (SEL + NVB=0x20) — card returns 4 UID bytes + BCC
 *    2. Verify BCC
 *    3. If first byte == 0x88 (CT), this cascade level contributes 3 UID bytes
 *       and we need the next cascade level.
 *    4. Send SELECT frame (SEL + NVB=0x70 + 4 bytes + BCC + CRC)
 *    5. Card replies with SAK; if SAK bit 2 is set → more cascade levels.
 * ------------------------------------------------------------------ */
static rfid_status_t picc_select(rfid_uid_t *uid)
{
    /*
     * buffer layout: [SEL][NVB][UID0][UID1][UID2][UID3][BCC][CRC_L][CRC_H]
     *                  0    1    2     3     4     5     6     7      8
     */
    uint8_t buffer[9];
    uint8_t uid_index = 0;

    for (uint8_t level = 1; level <= 3; level++) {
        uint8_t sel_cmd;
        switch (level) {
            case 1:  sel_cmd = PICC_SEL_CL1; break;
            case 2:  sel_cmd = PICC_SEL_CL2; break;
            default: sel_cmd = PICC_SEL_CL3; break;
        }

        /* ── Step 1: Anti-collision ── */
        buffer[0] = sel_cmd;
        buffer[1] = 0x20;  /* NVB: 2-byte frame length, 0 known bits */

        uint8_t resp_len = 5;  /* expect 4 UID bytes + BCC */
        rfid_status_t st = pcd_transceive(buffer, 2, &buffer[2],
                                           &resp_len, 0);
        if (st != RFID_OK)                    return st;
        if (resp_len < 5)                      return RFID_ERR_GENERIC;

        /* ── Step 2: BCC integrity check ── */
        uint8_t bcc = buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5];
        if (bcc != buffer[6])                  return RFID_ERR_CRC;

        /* ── Step 3: Extract UID bytes for this level ── */
        bool has_ct = (buffer[2] == CASCADE_TAG);
        uint8_t n_uid = has_ct ? 3u : 4u;   /* bytes to keep from this level */
        uint8_t src   = has_ct ? 3u : 2u;   /* start index in buffer */

        if ((uid_index + n_uid) > RFID_UID_MAX_BYTES) return RFID_ERR_GENERIC;
        memcpy(&uid->uid_byte[uid_index], &buffer[src], n_uid);
        uid_index += n_uid;

        /* ── Step 4: SELECT ── */
        buffer[1] = 0x70;  /* NVB: all 4 bytes known */
        /* buffer[2..5] still holds the 4 bytes from anti-collision */
        /* buffer[6] still holds BCC */
        if (pcd_calc_crc(buffer, 7, &buffer[7], &buffer[8]) != RFID_OK) {
            return RFID_ERR_TIMEOUT;
        }

        uint8_t sak_buf[3] = {0};
        uint8_t sak_len    = 3;
        st = pcd_transceive(buffer, 9, sak_buf, &sak_len, 0);
        if (st != RFID_OK) return st;
        if (sak_len < 1)   return RFID_ERR_GENERIC;

        uid->sak = sak_buf[0];

        /* ── Step 5: SAK bit 2 clear → UID complete ── */
        if (!(uid->sak & 0x04)) break;

        /* More cascade levels needed — but only if CT was present */
        if (!has_ct) return RFID_ERR_GENERIC;
    }

    uid->size = uid_index;
    return RFID_OK;
}

/* ────────────────────────────── Public API ─────────────────────────────── */
void rfid_rc522_init(void)
{
    /* Reset pin */
    gpio_config_t rst_cfg = {
        .pin_bit_mask = (1ULL << RFID_PIN_RST),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&rst_cfg);
    gpio_set_level(RFID_PIN_RST, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(RFID_PIN_RST, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    /* Init SPI bus */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = RFID_PIN_MOSI,
        .miso_io_num     = RFID_PIN_MISO,
        .sclk_io_num     = RFID_PIN_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 64,
    };
    esp_err_t ret = spi_bus_initialize(RFID_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = RFID_SPI_FREQ_HZ,
        .mode           = 0,
        .spics_io_num   = RFID_PIN_CS,
        .queue_size     = 4,
    };
    ret = spi_bus_add_device(RFID_SPI_HOST, &dev_cfg, &s_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Soft reset */
    pcd_write_reg(REG_COMMAND, CMD_SOFT_RESET);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    uint8_t tries = 50;
    while ((pcd_read_reg(REG_COMMAND) & (1 << 4)) && tries--) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    /* Timer: TAuto=1, fTimer = 6.78 MHz / TPreScaler, reload ~25 ms */
    pcd_write_reg(REG_T_MODE,      0x80);
    pcd_write_reg(REG_T_PRESCALER, 0xA9);
    pcd_write_reg(REG_T_RELOAD_H,  0x03);
    pcd_write_reg(REG_T_RELOAD_L,  0xE8);

    pcd_write_reg(REG_TX_ASK, 0x40);  /* 100% ASK modulation */
    pcd_write_reg(REG_MODE,   0x3D);  /* CRC preset 0x6363 */

    pcd_set_bits(REG_TX_CONTROL, 0x03);  /* enable antenna */

    uint8_t ver = pcd_read_reg(REG_VERSION);
    ESP_LOGI(TAG, "RC522 firmware version: 0x%02X", ver);
    if      (ver == 0x91) ESP_LOGI(TAG, "  => v1.0");
    else if (ver == 0x92) ESP_LOGI(TAG, "  => v2.0");
    else                  ESP_LOGW(TAG, "  => Unknown - kiem tra ket noi SPI");
}

rfid_status_t rfid_read_uid(rfid_uid_t *uid)
{
    rfid_status_t st = picc_reqa();
    if (st != RFID_OK) return st;

    memset(uid, 0, sizeof(*uid));   /* uid->size = 0 — picc_select sẽ điền */

    st = picc_select(uid);

    picc_hlta();
    pcd_clear_bits(REG_TX_CONTROL, 0x03);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    pcd_set_bits(REG_TX_CONTROL, 0x03);

    return st;
}
char *history_uid[100];
int history_uid_index = 0;

void add_history_uid(char *uid_str)
{
    if (history_uid_index >= 100) return;
    history_uid[history_uid_index] = strdup(uid_str);  /* copy string, không lưu pointer */
    if (history_uid[history_uid_index]) {
        history_uid_index++;
    }
}

void remove_history_uid(char *uid_str)
{
    for (int i = 0; i < history_uid_index; i++) {
        if (history_uid[i] && strcmp(history_uid[i], uid_str) == 0) {
            free(history_uid[i]);          /* giải phóng bộ nhớ đã strdup */
            history_uid_index--;
            for (int j = i; j < history_uid_index; j++) {  /* shift đúng */
                history_uid[j] = history_uid[j + 1];
            }
            history_uid[history_uid_index] = NULL;
            return;
        }
    }
}

bool check_history_uid(char *uid_str)
{
    for (int i = 0; i < history_uid_index; i++) {
        if (history_uid[i] && strcmp(history_uid[i], uid_str) == 0) {
            return true;
        }
    }
    return false;
}
int key_status = 0;
void rfid_rc522_task(void *pvParameters)
{
    char uid_str[32];
    char mqtt_payload[512];
    rfid_uid_t last_uid = {0};
    rfid_uid_t uid      = {0}; 

    ESP_LOGI(TAG, "Task started - dat the len dau doc...");
   
    int new_card = 0;
    int count_error = 0;
    while (1) {
        if (s_spi == NULL) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        if (rfid_read_uid(&uid) == RFID_OK) {
            /* Skip same card held continuously */
            if (uid.size > 0 &&
                uid.size == last_uid.size &&
                memcmp(uid.uid_byte, last_uid.uid_byte, uid.size) == 0) {
                vTaskDelay(500 / portTICK_PERIOD_MS);
                continue;
            }
            last_uid = uid;

            /* Format UID string */
            int off = 0;
            for (uint8_t i = 0; i < uid.size && off < (int)sizeof(uid_str) - 3; i++) {
                off += snprintf(uid_str + off, sizeof(uid_str) - (size_t)off,
                                "%02X", uid.uid_byte[i]);
                if (i < uid.size - 1u) uid_str[off++] = ':';
            }
            uid_str[off] = '\0';

            ESP_LOGI(TAG, "The phat hien - UID (%d bytes): %s  SAK:0x%02X",
                     uid.size, uid_str, uid.sak);

            if(strcmp(uid_str, "67:0A:84:33") == 0){
                gpio_set_level(BUZZER_PIN, 1);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                gpio_set_level(BUZZER_PIN, 0);
                // l9110s_lock_request_open();
                if(key_status == 0) {
                    key_status = 1;
                    l9110s_lock_request_open();
                }
                else {
                    key_status = 0;
                    l9110s_lock_request_close();
                }
                count_error = 0;
                new_card = 0;
            }
            else{
                gpio_set_level(BUZZER_PIN, 1);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                gpio_set_level(BUZZER_PIN, 0);
                if(check_history_uid(uid_str)) {
                    new_card = 0;
                    if(key_status == 0) {
                        key_status = 1;
                        l9110s_lock_request_open();
                    }
                    else {
                        key_status = 0;
                        l9110s_lock_request_close();
                    }
                    count_error = 0;
                }
                else {
                    count_error++;
                    if(count_error > 3) {
                        gpio_set_level(BUZZER_PIN, 1);
                        vTaskDelay(3000 / portTICK_PERIOD_MS);
                        gpio_set_level(BUZZER_PIN, 0);
                        if(mqtt_sub_success){
                            if (mqtt_sub_success) {
                                snprintf(mqtt_payload, sizeof(mqtt_payload),
                                         "{\"warning\":\"%d\", \"times\":\"%d\"}", 1, count_error);
                                // mqtt_publish(MQTT_PUBTOPIC, mqtt_payload);
                                xQueueSend(publish_queue_handle, mqtt_payload, portMAX_DELAY);
                            }
                        }
                        count_error = 0;
                    }
                    new_card = 1;
                } 
            }

            /* Publish lên MQTT khi đã kết nối */
            if (mqtt_sub_success) {
                snprintf(mqtt_payload, sizeof(mqtt_payload),
                         "{\"rfid\":\"%s\", \"key_status\":\"%d\", \"new_card\":\"%d\"}", uid_str, key_status, new_card);
                // mqtt_publish(MQTT_PUBTOPIC, mqtt_payload);
                xQueueSend(publish_queue_handle, mqtt_payload, portMAX_DELAY);
            }
        } else {
            memset(&last_uid, 0, sizeof(last_uid));
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}
