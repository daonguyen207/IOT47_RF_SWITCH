#ifndef _IOT47_RF_DECODER_
#define _IOT47_RF_DECODER_

// --- Timing µs ---
#define T_START_LOW_US   12000  // ~12 ms
#define T_SHORT_US       430
#define T_LONG_US        1200

// Dung sai ±35%: tùy môi trường mà siết/lỏng
#define TOL_FRAC         0.35f
#define MIN_T(v)   (int)((v) * (1.0f - TOL_FRAC))
#define MAX_T(v)   (int)((v) * (1.0f + TOL_FRAC))

#define START_MIN_US MIN_T(T_START_LOW_US)
#define START_MAX_US MAX_T(T_START_LOW_US)
#define SHORT_MIN_US MIN_T(T_SHORT_US)
#define SHORT_MAX_US MAX_T(T_SHORT_US)
#define LONG_MIN_US  MIN_T(T_LONG_US)
#define LONG_MAX_US  MAX_T(T_LONG_US)

// Bỏ cạnh nhiễu siêu ngắn
#define MIN_EDGE_US          100
// Hết khung nếu đang nhận mà quá lâu không có cạnh
#define FRAME_GAP_TIMEOUT_US 5000

typedef enum {
    ST_IDLE = 0,
    ST_WAIT_BITS,   // đã thấy start, đang nhận cặp (HIGH,LOW)
} rf_state_t;

typedef struct {
    volatile rf_state_t state;
    volatile uint64_t   last_edge_us;
    volatile int        last_level;   // 0/1

    volatile int        have_high;
    volatile int        have_low;
    volatile int        high_us;
    volatile int        low_us;

    volatile uint32_t   data;
    volatile int        bit_count;

    volatile int        frame_ready;  // set khi đủ 24 bit
    volatile int        error_count;
} rf_decoder_t;

void rf_gpio_init(int pin);
void rf_print_bits24(uint32_t v);
int rf_available();
int rf_read();

extern rf_decoder_t g;

inline void rf_reset(void)
{
    g.state = ST_IDLE;
    g.have_high = 0;
    g.have_low  = 0;
    g.high_us = 0;
    g.low_us  = 0;
    g.data = 0;
    g.bit_count = 0;
    g.frame_ready = 0;
}

#endif