/**
 * level_sensor.h
 *
 * Lớp trừu tượng hoá việc đọc cảm biến US-016 qua mạch đệm/chia áp:
 *   - Đọc ADC2 có hiệu chỉnh (adc_cali) thay vì quy đổi tuyến tính thô,
 *     giảm sai số lệch chuẩn giữa các chip ESP32 khác nhau.
 *   - Oversampling + lọc trung vị NGAY TRONG một lần đọc để loại mẫu lỗi
 *     đơn lẻ trước khi trả kết quả ra ngoài.
 *   - Quy đổi điện áp đo được thành khoảng cách (mm) theo đúng công thức
 *     của US-016 ở chế độ đo 1m, có "undo" lại tỉ số chia áp R2/R3 để suy
 *     ra điện áp gốc tại chân OUT thật của cảm biến.
 *
 * Lớp này KHÔNG biết gì về "mức nước" hay "bể chứa" - chỉ trả về khoảng
 * cách vật lý từ cảm biến đến vật cản gần nhất. Việc quy đổi sang mức
 * nước (tank_height - distance) thuộc về main.c.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int      adc_channel;          /* Kênh ADC2, ví dụ ADC_CHANNEL_3 cho GPIO15 */
    float    divider_ratio;        /* R3 / (R2 + R3) của cầu phân áp đầu vào */
    uint32_t sensor_vcc_mv;        /* Điện áp cấp cho BẢN THÂN cảm biến US-016 */
    uint32_t range_full_scale_mm;  /* Khoảng cách ứng với Vout == Vcc (1024 cho mode 1m) */
    uint32_t min_valid_mm;         /* Khoảng cách nhỏ nhất hợp lệ theo datasheet */
    uint32_t max_valid_mm;         /* Khoảng cách lớn nhất hợp lệ cho mode đang chọn */
    uint8_t  oversample_count;     /* Số mẫu ADC lấy liên tiếp mỗi lần đọc (<=16) */
} level_sensor_config_t;

/** Khởi tạo ADC2 + (nếu chip hỗ trợ) calibration scheme tương ứng. */
esp_err_t level_sensor_init(const level_sensor_config_t *config);

/**
 * Đọc điện áp tại chân ADC của ESP32 (đã qua oversampling + median),
 * đơn vị mV. Đây là điện áp SAU cầu phân áp, chưa quy đổi khoảng cách.
 */
esp_err_t level_sensor_read_mv(uint32_t *out_mv);

/**
 * Đọc và quy đổi trực tiếp ra khoảng cách (mm) từ cảm biến đến vật cản.
 * Trả về ESP_ERR_INVALID_RESPONSE nếu giá trị nằm ngoài dải hợp lệ
 * (mất echo, vật cản quá gần, cảm biến lỗi/đứt dây...).
 */
esp_err_t level_sensor_read_distance_mm(float *out_distance_mm);

#ifdef __cplusplus
}
#endif