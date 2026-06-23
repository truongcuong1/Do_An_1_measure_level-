/**
 * signal_filter.h
 *
 * Bộ lọc tín hiệu dùng chung, không phụ thuộc vào ESP-IDF (thuần C), có thể
 * tái sử dụng cho bất kỳ tín hiệu analog biến đổi chậm nào, không riêng gì
 * cảm biến mức nước.
 *
 * Cơ chế lọc 2 lớp:
 *   1) Median của một cửa sổ trượt N mẫu gần nhất -> loại các giá trị
 *      "nhảy số" bất thường (do nhiễu sóng phản xạ, dao động mặt nước...)
 *   2) EMA (Exponential Moving Average) trên kết quả median -> làm mượt
 *      đường cong hiển thị cuối cùng.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIGNAL_FILTER_MEDIAN_WINDOW 5

typedef struct {
    float   history[SIGNAL_FILTER_MEDIAN_WINDOW];
    uint8_t count;
    uint8_t index;
    float   ema_value;
    float   ema_alpha;
    bool    ema_initialized;
} signal_filter_t;

/**
 * Khởi tạo bộ lọc. ema_alpha trong khoảng (0, 1], càng nhỏ càng mượt
 * nhưng càng trễ theo thực tế.
 */
void signal_filter_init(signal_filter_t *filter, float ema_alpha);

/**
 * Đưa bộ lọc về trạng thái ban đầu (giữ lại ema_alpha đã cấu hình).
 * Hữu ích sau khi hiệu chỉnh (calibrate) để tránh lẫn dữ liệu cũ/mới.
 */
void signal_filter_reset(signal_filter_t *filter);

/**
 * Hàm tiện ích độc lập: tính trung vị của một mảng mẫu bất kỳ (tối đa 16
 * phần tử). Dùng cho cả việc lọc oversampling bên trong level_sensor.
 */
float signal_filter_median(const float *samples, size_t n);

/**
 * Đưa một mẫu mới vào bộ lọc 2 lớp (median window + EMA), trả về giá trị
 * đã lọc. Gọi hàm này mỗi chu kỳ đo (ví dụ mỗi 200ms).
 */
float signal_filter_process(signal_filter_t *filter, float new_sample);

#ifdef __cplusplus
}
#endif