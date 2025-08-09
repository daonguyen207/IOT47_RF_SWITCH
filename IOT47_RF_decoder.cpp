#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "IOT47_RF_decoder.h"

static const char *TAG = "RF_DECODE";

// ==== CHỌN CHÂN Ở ĐÂY ====
int RF_PIN = GPIO_NUM_10;

rf_decoder_t g;

static inline bool in_range(int v, int lo, int hi) { return (v >= lo) && (v <= hi); }

void rf_print_bits24(uint32_t v)
{
    // In ra 24 bit MSB->LSB để dễ nhìn
    char s[25];
    for (int i = 0; i < 24; ++i) {
        s[i] = (v & (1U << (23 - i))) ? '1' : '0';
    }
    s[24] = '\0';
    ESP_LOGI(TAG, "BIN: %s", s);
}

static void IRAM_ATTR rf_gpio_isr(void *arg)
{
    int level = gpio_get_level((gpio_num_t)RF_PIN);     // mức sau cạnh
    uint64_t now = esp_timer_get_time();
    uint64_t dt  = now - g.last_edge_us;   // thời gian mức trước

    if (dt < MIN_EDGE_US) {
        g.last_edge_us = now;
        g.last_level   = level;
        return;
    }

    if (g.state == ST_WAIT_BITS && dt > FRAME_GAP_TIMEOUT_US) {
        rf_reset();
    }

    if (g.state == ST_IDLE) {
        // Tìm start: một LOW dài ~12ms, phát hiện tại cạnh lên
        if (g.last_level == 0 && in_range((int)dt, START_MIN_US, START_MAX_US) && level == 1) {
            g.state     = ST_WAIT_BITS;
            g.bit_count = 0;
            g.data      = 0;
            g.have_high = 0;
            g.have_low  = 0;
        }
    } else { // ST_WAIT_BITS
        if (g.last_level == 1) {
            // vừa kết thúc HIGH (cạnh xuống)
            g.high_us = (int)dt;
            g.have_high = 1;
        } else {
            // vừa kết thúc LOW (cạnh lên)
            if (g.bit_count > 0 || g.have_high) {
                g.low_us = (int)dt;
                g.have_low = 1;
            }
        }

        // đủ cặp (HIGH,LOW) thì phân loại bit
        if (g.have_high && g.have_low) {
            int h = g.high_us;
            int l = g.low_us;
            int bit = -1;

            // Bit 1: HIGH ~ 0.43ms (short), LOW ~ 1.2ms (long)
            if (in_range(h, SHORT_MIN_US, SHORT_MAX_US) && in_range(l, LONG_MIN_US, LONG_MAX_US)) {
                bit = 1;
            }
            // Bit 0: HIGH ~ 1.2ms (long), LOW ~ 0.43ms (short)
            else if (in_range(h, LONG_MIN_US, LONG_MAX_US) && in_range(l, SHORT_MIN_US, SHORT_MAX_US)) {
                bit = 0;
            }

            if (bit < 0) {
                g.error_count++;
                rf_reset();
            } else {
                g.data = (g.data << 1) | (uint32_t)bit;  // MSB-first
                g.bit_count++;

                g.have_high = 0;
                g.have_low  = 0;
                g.high_us = 0;
                g.low_us  = 0;

                if (g.bit_count >= 24) {
                    g.frame_ready = 1;
                    g.state = ST_IDLE; // chờ start mới
                }
            }
        }
    }

    g.last_edge_us = now;
    g.last_level   = level;
}

void rf_gpio_init(int pin)
{
    int RF_PIN = pin;

    rf_reset();

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << RF_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,      // tùy module RF của bạn
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    // Cài ISR service (chỉ gọi 1 lần trong app)
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)RF_PIN, rf_gpio_isr, NULL));

    // Khởi tạo mốc thời gian/level ban đầu
    g.last_edge_us = esp_timer_get_time();
    g.last_level   = gpio_get_level((gpio_num_t)RF_PIN);
}

int rf_available()
{
      if (g.frame_ready) return g.frame_ready;
      else
      {
        // an toàn: nếu đang đợi bit nhưng im lặng lâu quá, reset
        uint64_t now = esp_timer_get_time();
        if (g.state == ST_WAIT_BITS && (now - g.last_edge_us) > FRAME_GAP_TIMEOUT_US) {
            rf_reset();
        }
        return 0;
      }
}
int rf_read()
{
    uint32_t v = g.data;
    rf_reset(); // xóa cờ, sẵn sàng khung kế tiếp
    return v;
}