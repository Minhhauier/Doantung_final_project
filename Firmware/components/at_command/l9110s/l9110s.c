#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_timer.h>
#include <esp_log.h>

#include "l9110s.h"
#include "config_parameter.h"
#include "hal/gpio_types.h"

static const char *TAG = "LOCK";

/* ── Trạng thái nội bộ ──────────────────────────────────────────────────── */
static volatile lock_state_t s_state        = LOCK_STATE_CLOSING;
static volatile bool         s_req_open     = false;
static volatile bool         s_req_close    = false;

/* ── Helper: thời gian ms ───────────────────────────────────────────────── */
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ── Điều khiển motor qua LEDC ──────────────────────────────────────────── */
static void motor_stop(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* Mở khóa: PIN_A chạy, PIN_B dừng */
static void motor_open(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LOCK_PWM_DUTY);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* Đóng khóa: PIN_A dừng, PIN_B chạy */
static void motor_close(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LOCK_PWM_DUTY);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* ── Khởi tạo LEDC (PWM) ────────────────────────────────────────────────── */
static void setup_pwm(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = LOCK_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch_a = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = LOCK_PIN_A,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_a);

    ledc_channel_config_t ch_b = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_1,
        .timer_sel  = LEDC_TIMER_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = LOCK_PIN_B,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_b);
}

/* ── Khởi tạo GPIO (nút nhấn + công tắc hành trình) ──────────────────────── */
static void setup_gpio(void)
{
    /* Nút mở — pull-up nội bộ, tích cực thấp */
    gpio_set_direction(BUZZER_PIN,  GPIO_MODE_OUTPUT);
    // gpio_set_level(BUZZER_PIN, 1);
    // vTaskDelay(1000 / portTICK_PERIOD_MS);
    // gpio_set_level(BUZZER_PIN, 0);

    /* Nút đóng — pull-up nội bộ, tích cực thấp */
    // gpio_set_direction(LOCK_PIN_BTN_CLOSE, GPIO_MODE_INPUT);
    // gpio_set_pull_mode(LOCK_PIN_BTN_CLOSE, GPIO_PULLUP_ONLY);

    /* Công tắc hành trình — GPIO35 không có pull nội bộ, dùng trở pull-up ngoài 10kΩ lên 3.3V
     * Switch nối: Com → GPIO35, chân còn lại → GND → tích cực thấp (active LOW) */
    gpio_set_direction(LOCK_PIN_LIMIT, GPIO_MODE_INPUT);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void l9110s_lock_init(void)
{
    setup_pwm();
   setup_gpio();
    ESP_LOGI(TAG, "Khoa chot dien tu khoi tao OK");
    ESP_LOGI(TAG, "  Motor  : PIN_A=%d  PIN_B=%d", LOCK_PIN_A, LOCK_PIN_B);
    // ESP_LOGI(TAG, "  Button : OPEN=%d  CLOSE=%d", LOCK_PIN_BTN_OPEN, LOCK_PIN_BTN_CLOSE);
    ESP_LOGI(TAG, "  Button : OPEN=false  CLOSE=false");
    ESP_LOGI(TAG, "  Limit  : PIN=%d", LOCK_PIN_LIMIT);
}

void l9110s_lock_request_open(void)
{
    s_req_open = true;
}

void l9110s_lock_request_close(void)
{
    s_req_close = true;
}

lock_state_t l9110s_lock_get_state(void)
{
    return s_state;
}

bool l9110s_lock_is_open(void)
{
    return (s_state == LOCK_STATE_OPEN || s_state == LOCK_STATE_OPENING);
}

/* ── FreeRTOS Task: State Machine ───────────────────────────────────────── */
void l9110s_lock_task(void *pvParameters)
{
    uint32_t open_start_ms  = 0;
    uint32_t close_start_ms = now_ms();   /* khởi tạo = now để timeout tính đúng từ lúc task bắt đầu */

    /* Biến debounce nhỏ cho limit switch */
    bool     limit_stable       = false;
    bool     limit_last_reading = false;
    uint32_t limit_debounce_t   = 0;

    ESP_LOGI(TAG, "Lock task started — dang dong khoa...");

    while (1) {
        uint32_t ms = now_ms();

        /* ── Đọc nút nhấn (tích cực thấp) ── */
        // bool btn_open  = (gpio_get_level(LOCK_PIN_BTN_OPEN)  == 0);
        // bool btn_close = (gpio_get_level(LOCK_PIN_BTN_CLOSE) == 0);
        bool btn_open = false;
        bool btn_close = false;

        /* ── Đọc & debounce nhỏ công tắc hành trình ──
         * Switch nối Com→GPIO35, chân kia→GND → kích = LOW (active LOW) */
        bool limit_reading = (gpio_get_level(LOCK_PIN_LIMIT) == 0);
        if (limit_reading != limit_last_reading) {
            limit_debounce_t = ms;
        }
        if ((ms - limit_debounce_t) >= LOCK_DEBOUNCE_MS) {
            limit_stable = limit_reading;
        }
        limit_last_reading = limit_reading;

        /* ── Lấy và xóa request ngoài ── */
        bool req_open  = s_req_open;
        bool req_close = s_req_close;
        if (req_open)  s_req_open  = false;
        if (req_close) s_req_close = false;

        /* ── State Machine ── */
        switch (s_state) {

            /* DANG_DONG: chạy motor đóng, dừng khi limit kích hoặc timeout */
            case LOCK_STATE_CLOSING: {
                uint32_t elapsed = ms - close_start_ms;

                if (elapsed >= LOCK_CLOSE_TIMEOUT_MS) {
                    /* Timeout — dừng dù limit chưa kích (tránh motor kêu liên tục) */
                    motor_stop();
                    s_state = LOCK_STATE_CLOSED;
                    ESP_LOGW(TAG, "[TRANG THAI] DA DONG (timeout — kiem tra limit switch)");
                } else if (elapsed >= LOCK_CLOSE_MIN_RUN_MS && limit_stable) {
                    /* Bình thường — limit switch kích */
                    motor_stop();
                    s_state = LOCK_STATE_CLOSED;
                    ESP_LOGI(TAG, "[TRANG THAI] DA DONG (limit switch)");
                } else {
                    motor_close();
                }
                break;
            }

            /* DA_DONG: Đã đóng, chờ lệnh mở (nút / RFID / MQTT) */
            case LOCK_STATE_CLOSED:
                if (btn_open || req_open) {
                    open_start_ms = now_ms();
                    s_state       = LOCK_STATE_OPENING;
                    ESP_LOGI(TAG, "[TRANG THAI] DANG MO (nguon: %s)",
                             req_open ? "RFID/MQTT" : "nut nhan");
                }
                break;

            /* DANG_MO: Motor đang chạy mở, chờ đủ thời gian */
            case LOCK_STATE_OPENING:
                motor_open();
                if ((now_ms() - open_start_ms) >= LOCK_OPEN_DURATION_MS) {
                    motor_stop();
                    s_state = LOCK_STATE_OPEN;
                    ESP_LOGI(TAG, "[TRANG THAI] DA MO");
                }
                break;

            /* DA_MO: Đã mở, chờ lệnh đóng (nút / RFID / MQTT) */
            case LOCK_STATE_OPEN:
                if (btn_close || req_close) {
                    close_start_ms = now_ms();   /* ghi nhớ thời điểm bắt đầu đóng */
                    limit_stable   = false;       /* reset debounce */
                    s_state        = LOCK_STATE_CLOSING;
                    ESP_LOGI(TAG, "[TRANG THAI] DANG DONG (nguon: %s)",
                             req_close ? "RFID/MQTT" : "nut nhan");
                }
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));  /* 10ms — tránh Watchdog, đủ responsive */
    }
}
