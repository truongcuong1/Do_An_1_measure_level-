/**
 * level_storage.h
 *
 * Lưu trữ hệ số hiệu chỉnh (calibration) vào NVS (flash) để không bị mất
 * mỗi khi cấp lại điện. Đây chính là nơi xử lý "sai số hệ thống": sai số
 * do dung sai điện trở R2/R3, sai số lắp đặt thực tế của cảm biến so với
 * đáy bể... được nén lại thành một con số offset_mm duy nhất, xác định
 * bằng thực nghiệm qua lệnh "cal" trên console (xem main.c).
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    offset_mm;       /* Độ lệch cộng thêm vào khoảng cách đo được, xác định bằng calibrate */
    uint32_t tank_height_mm;  /* Khoảng cách từ vị trí gắn cảm biến tới đáy bể (mức 0%) */
} level_calibration_t;

/** Khởi tạo NVS flash (tự xoá & khởi tạo lại nếu phát hiện phiên bản không khớp). */
esp_err_t level_storage_init(void);

/** Đọc hệ số hiệu chỉnh đã lưu; nếu chưa từng lưu, trả về giá trị mặc định an toàn. */
esp_err_t level_storage_load(level_calibration_t *out_cal);

/** Lưu hệ số hiệu chỉnh hiện tại vào NVS (giữ nguyên qua các lần mất điện). */
esp_err_t level_storage_save(const level_calibration_t *cal);

#ifdef __cplusplus
}
#endif