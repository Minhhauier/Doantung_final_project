#ifndef L9110S_H
#define L9110S_H

#include <stdbool.h>
#include <stdint.h>

/* ── Trạng thái khóa ─────────────────────────────────────────────────────
 *
 *  LOCK_CLOSING → LOCK_CLOSED  : motor đóng đến khi công tắc hành trình kích
 *  LOCK_CLOSED  → LOCK_OPENING : nhận lệnh mở (nút / RFID / MQTT)
 *  LOCK_OPENING → LOCK_OPEN    : motor mở đủ LOCK_OPEN_DURATION_MS
 *  LOCK_OPEN    → LOCK_CLOSING : nhận lệnh đóng (nút / timeout tự động)
 */
typedef enum {
    LOCK_STATE_CLOSED  = 0,   /* DA_DONG  — đã đóng, chờ lệnh mở   */
    LOCK_STATE_OPENING = 1,   /* DANG_MO  — đang chạy motor mở      */
    LOCK_STATE_OPEN    = 2,   /* DA_MO    — đã mở, chờ lệnh đóng    */
    LOCK_STATE_CLOSING = 3,   /* DANG_DONG — đang chạy motor đóng   */
} lock_state_t;

/**
 * @brief Khởi tạo PWM (LEDC) và GPIO cho khóa.
 *        Gọi một lần trong app_main trước khi tạo task.
 */
void l9110s_lock_init(void);

/**
 * @brief FreeRTOS task — chạy state machine điều khiển khóa.
 *        Stack khuyến nghị: 3072 bytes.
 */
void l9110s_lock_task(void *pvParameters);

/**
 * @brief Yêu cầu mở khóa (thread-safe).
 *        Có thể gọi từ bất kỳ task nào (RFID, MQTT, nút nhấn phần mềm).
 */
void l9110s_lock_request_open(void);

/**
 * @brief Yêu cầu đóng khóa (thread-safe).
 */
void l9110s_lock_request_close(void);

/**
 * @brief Trả về trạng thái hiện tại của khóa.
 */
lock_state_t l9110s_lock_get_state(void);

/**
 * @brief Trả về true nếu khóa đang ở trạng thái DA_MO hoặc DANG_MO.
 */
bool l9110s_lock_is_open(void);

#endif /* L9110S_H */
