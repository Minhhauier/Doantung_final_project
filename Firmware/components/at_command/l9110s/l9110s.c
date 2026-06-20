#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_timer.h>
#include <esp_log.h>

#include "l9110s.h"
#include "config_parameter.h"

static const char *TAG = "LOCK";

static volatile lock_state_t s_state     = LOCK_STATE_OPENING;
static volatile bool         s_req_open  = false;
static volatile bool         s_req_close = false;

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void motor_stop(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* Mở khóa (turnLeft): B1A chạy, B1B dừng */
static void motor_open(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LOCK_PWM_DUTY);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* Đóng khóa (turnRight): B1A dừng, B1B chạy */
static void motor_close(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LOCK_PWM_DUTY);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

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

static void setup_gpio(void)
{
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    /* GPIO35: C nối D35, NO nối 3.3V → kích = HIGH (giống code Arduino) */
    gpio_set_direction(LOCK_PIN_LIMIT, GPIO_MODE_INPUT);
}

void l9110s_lock_init(void)
{
    setup_pwm();
    setup_gpio();
    ESP_LOGI(TAG, "Khoa chot dien tu khoi tao OK");
    ESP_LOGI(TAG, "  Motor  : B1A=%d (mo)  B1B=%d (dong)", LOCK_PIN_A, LOCK_PIN_B);
    ESP_LOGI(TAG, "  Limit  : PIN=%d  level=%d (1=kich — dung khi mo)", LOCK_PIN_LIMIT,
             gpio_get_level(LOCK_PIN_LIMIT));
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

void l9110s_lock_task(void *pvParameters)
{
    uint32_t open_start_ms    = now_ms();
    uint32_t close_start_ms   = 0;
    bool     limit_stable     = false;
    bool     limit_last_read  = false;
    uint32_t limit_debounce_t = 0;

    ESP_LOGI(TAG, "Lock task started — dang mo khoa...");

    while (1) {
        uint32_t ms = now_ms();

        bool btn_open  = false;
        bool btn_close = false;

        /* Debounce limit — active HIGH (Arduino: digitalRead == HIGH) */
        bool limit_reading = (gpio_get_level(LOCK_PIN_LIMIT) == 0);
        if (limit_reading != limit_last_read) {
            limit_debounce_t = ms;
        }
        if ((ms - limit_debounce_t) >= LOCK_DEBOUNCE_MS) {
            limit_stable = limit_reading;
        }
        limit_last_read = limit_reading;

        bool req_open  = s_req_open;
        bool req_close = s_req_close;
        if (req_open)  s_req_open  = false;
        if (req_close) s_req_close = false;

        switch (s_state) {

            /* DANG_DONG: turnRight, dừng theo timeout */
            case LOCK_STATE_CLOSING: {
                uint32_t elapsed = ms - close_start_ms;

                motor_close();
                if (elapsed >= LOCK_CLOSE_TIMEOUT_MS) {
                    motor_stop();
                    s_state = LOCK_STATE_CLOSED;
                    ESP_LOGI(TAG, "[TRANG THAI] DA DONG (timeout %lums)",
                             (unsigned long)elapsed);
                }
                break;
            }

            /* DA_DONG: dừng motor, chờ lệnh mở */
            case LOCK_STATE_CLOSED:
                motor_stop();
                if (btn_open || req_open) {
                    open_start_ms    = now_ms();
                    limit_stable     = false;
                    limit_last_read  = false;
                    limit_debounce_t = ms;
                    s_state          = LOCK_STATE_OPENING;
                    ESP_LOGI(TAG, "[TRANG THAI] DANG MO (nguon: %s, limit=%d)",
                             req_open ? "RFID/MQTT" : "nut nhan",
                             gpio_get_level(LOCK_PIN_LIMIT));
                }
                break;

            /* DANG_MO: turnLeft, dừng khi limit HIGH hoặc timeout */
            case LOCK_STATE_OPENING: {
                uint32_t elapsed = ms - open_start_ms;

                motor_open();
                if (limit_stable) {
                    motor_stop();
                    s_state = LOCK_STATE_OPEN;
                    ESP_LOGI(TAG, "[TRANG THAI] DA MO (limit switch, %lums)",
                             (unsigned long)elapsed);
                } else if (elapsed >= LOCK_OPEN_TIMEOUT_MS) {
                    motor_stop();
                    s_state = LOCK_STATE_OPEN;
                    ESP_LOGW(TAG, "[TRANG THAI] DA MO (timeout — limit=%d)",
                             gpio_get_level(LOCK_PIN_LIMIT));
                }
                break;
            }

            /* DA_MO: dừng motor, chờ lệnh đóng */
            case LOCK_STATE_OPEN:
                motor_stop();
                if (btn_close || req_close) {
                    close_start_ms = now_ms();
                    s_state        = LOCK_STATE_CLOSING;
                    ESP_LOGI(TAG, "[TRANG THAI] DANG DONG (nguon: %s)",
                             req_close ? "RFID/MQTT" : "nut nhan");
                }
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
