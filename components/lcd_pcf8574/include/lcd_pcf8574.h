/**
 * lcd_pcf8574.h
 *
 * Driver tối giản cho LCD ký tự (HD44780, 16x2) gắn qua module mở rộng I2C
 * dùng IC PCF8574 - đúng loại module phổ biến trong sơ đồ J2_I2C/J1_LCD.
 *
 * Sơ đồ chân PCF8574 -> HD44780 theo chuẩn module phổ biến nhất trên
 * thị trường (LCM1602 IIC / "I2C LCD adapter"):
 *   P0 -> RS    P1 -> RW    P2 -> EN    P3 -> Backlight
 *   P4 -> D4    P5 -> D5    P6 -> D6    P7 -> D7
 *
 * Nếu LCD của cậu không hiển thị gì / hiện toàn ô vuông, khả năng cao là
 * module dùng PCF8574A (địa chỉ 0x38-0x3F) thay vì PCF8574 (0x20-0x27) -
 * chỉ cần đổi APP_LCD_I2C_ADDRESS trong app_config.h.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t i2c_port;
    uint8_t    i2c_address;
} lcd_pcf8574_t;

/** Khởi tạo LCD theo đúng trình tự HD44780 ở chế độ 4-bit. */
esp_err_t lcd_pcf8574_init(lcd_pcf8574_t *lcd, i2c_port_t port, uint8_t address);

/** Xoá toàn bộ màn hình và đưa con trỏ về (0,0). */
esp_err_t lcd_pcf8574_clear(lcd_pcf8574_t *lcd);

/** Di chuyển con trỏ tới hàng (0 hoặc 1), cột (0-15). */
esp_err_t lcd_pcf8574_set_cursor(lcd_pcf8574_t *lcd, uint8_t row, uint8_t col);

/** In một chuỗi ký tự tại vị trí con trỏ hiện tại. */
esp_err_t lcd_pcf8574_print(lcd_pcf8574_t *lcd, const char *str);

/** Bật/tắt đèn nền LCD. */
esp_err_t lcd_pcf8574_backlight(lcd_pcf8574_t *lcd, bool on);

#ifdef __cplusplus
}
#endif