#ifndef RFID_RC522_H
#define RFID_RC522_H

#include <stdint.h>
#include <stdbool.h>

#define RFID_UID_MAX_BYTES  10

typedef enum {
    RFID_OK          = 0,
    RFID_ERR_TIMEOUT = 1,
    RFID_ERR_CRC     = 2,
    RFID_ERR_COLL    = 3,
    RFID_ERR_GENERIC = 4,
} rfid_status_t;

typedef struct {
    uint8_t size;                    // số byte UID hợp lệ (4, 7 hoặc 10)
    uint8_t uid_byte[RFID_UID_MAX_BYTES];
    uint8_t sak;                     // Select Acknowledge
} rfid_uid_t;

/**
 * @brief Khởi tạo SPI bus và RC522, bật ăng-ten.
 *        Gọi một lần từ app_main trước khi tạo task.
 */
void rfid_rc522_init(void);

/**
 * @brief FreeRTOS task: liên tục quét thẻ, in UID ra log.
 *        Khi phát hiện thẻ mới sẽ publish lên MQTT topic.
 */
void rfid_rc522_task(void *pvParameters);

/**
 * @brief Đọc UID của thẻ đang đặt gần reader.
 * @param uid  Con trỏ tới struct lưu kết quả.
 * @return RFID_OK nếu thành công, mã lỗi khác nếu không có thẻ / lỗi.
 */
rfid_status_t rfid_read_uid(rfid_uid_t *uid);

#endif // RFID_RC522_H
