/**
 * app_config.h
 *
 * Toàn bộ thông số phần cứng/thuật toán của dự án "đo mức nước US-016" được
 * tập trung tại đây. Khi đổi chân, đổi tỉ lệ chia áp, đổi chiều cao bể... thì
 * chỉ cần sửa trong file này, không cần lục lại từng file .c.
 */
#pragma once

#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * KHỐI CẢM BIẾN US-016
 *
 * Theo sơ đồ nguyên lý: tín hiệu OUT của US-016 đi qua cầu phân áp
 * R2(10K)/R3(15K) rồi vào bộ đệm op-amp (MCP6002, cấp nguồn 3V3) trước khi
 * vào GPIO15 (ADC2_CH3) của ESP32.
 *
 * !!! QUAN TRỌNG: trước khi tin các hằng số dưới đây, PHẢI xác minh bằng
 * VOM rằng chân đang đưa vào R2 thực sự là chân OUT (tín hiệu analog tỉ lệ
 * khoảng cách), không phải chân RANGE (chân chọn dải đo 1m/3m) - xem phần
 * trao đổi trước đó về rủi ro đấu nhầm chân của module này.
 * ===================================================================== */

/* GPIO15 trên ESP32 tương ứng ADC_UNIT_2, channel 3 */
#define APP_ADC_CHANNEL           ADC_CHANNEL_3

/* Tỉ số chia áp đầu vào = R3 / (R2 + R3) = 15k / (10k + 15k) = 0.6 */
#define APP_DIVIDER_RATIO         0.6f

/* Điện áp cấp cho BẢN THÂN cảm biến US-016 (không phải điện áp ESP32 đọc) */
#define APP_SENSOR_VCC_MV         5000u

/* Chân RANGE của US-016 phải được nối THẲNG xuống GND để chọn dải đo 1m.
 * Khi đó: khoang_cach_mm = 1024 * Vout_cam_bien / Vcc_cam_bien */
#define APP_RANGE_FULL_SCALE_MM   1024u

/* Giới hạn vật lý hợp lệ của cảm biến (datasheet: đo được từ ~2cm) */
#define APP_SENSOR_MIN_MM         20u
#define APP_SENSOR_MAX_MM         1024u

/* Số mẫu ADC lấy liên tiếp (oversampling) cho MỖI lần đọc, sau đó lấy
 * trung vị (median) để loại mẫu lỗi đơn lẻ trước khi đưa vào lọc EMA */
#define APP_OVERSAMPLE_COUNT      10u

/* =====================================================================
 * KHỐI I2C / LCD
 * Theo sơ đồ: SDA = GPIO21, SCL = GPIO22 (đúng với chân I2C mặc định của
 * ESP32 DEVKIT). Trở kéo 4.7K trên board kéo lên 3V3 - KHÔNG cho driver
 * I2C của ESP32 bật pull-up nội bộ nữa để tránh chồng hai cấp pull-up.
 * ===================================================================== */
#define APP_I2C_PORT              I2C_NUM_0
#define APP_I2C_SDA_GPIO          21
#define APP_I2C_SCL_GPIO          22
#define APP_I2C_FREQ_HZ           100000u

/* Địa chỉ I2C phổ biến của module LCD PCF8574 là 0x27 hoặc 0x3F.
 * Nếu lcd_pcf8574_init() báo lỗi ESP_ERR_TIMEOUT, hãy thử đổi sang 0x3F. */
#define APP_LCD_I2C_ADDRESS       0x27

/* =====================================================================
 * KHỐI THỜI GIAN / LỌC TÍN HIỆU
 * ===================================================================== */

/* Chu kỳ lấy mẫu cảm biến - tách biệt với chu kỳ hiển thị để việc lọc tín
 * hiệu không bị giới hạn bởi tốc độ ghi I2C ra LCD (vốn chậm hơn nhiều) */
#define APP_SENSOR_PERIOD_MS      200u
#define APP_DISPLAY_PERIOD_MS     500u

/* Hệ số làm mượt EMA: 0 = rất mượt nhưng trễ nhiều, 1 = bám sát nhưng
 * không lọc được gì. 0.25 là điểm cân bằng tốt cho mức nước biến đổi chậm */
#define APP_EMA_ALPHA             0.25f

/* Chỉ vẽ lại LCD khi mức nước thay đổi từ giá trị đang hiển thị nhiều hơn
 * ngưỡng này (mm) - tránh màn hình nhấp nháy liên tục do nhiễu dư sau lọc */
#define APP_DISPLAY_HYSTERESIS_MM 3.0f

#ifdef __cplusplus
}
#endif