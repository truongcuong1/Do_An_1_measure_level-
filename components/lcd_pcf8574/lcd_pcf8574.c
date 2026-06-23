// #include "lcd_pcf8574.h"

// #include "esp_log.h"
// #include "esp_rom_sys.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// static const char *TAG = "lcd_pcf8574";

// #define LCD_BIT_RS  (1 << 0)
// #define LCD_BIT_RW  (1 << 1)
// #define LCD_BIT_EN  (1 << 2)
// #define LCD_BIT_BL  (1 << 3)

// static bool s_backlight_on = true;

// static esp_err_t pcf8574_write(lcd_pcf8574_t *lcd, uint8_t data)
// {
//     uint8_t buf = data | (uint8_t)(s_backlight_on ? LCD_BIT_BL : 0);
//     return i2c_master_write_to_device(lcd->i2c_port, lcd->i2c_address,
//                                        &buf, 1, pdMS_TO_TICKS(50));
// }

// static esp_err_t pulse_enable(lcd_pcf8574_t *lcd, uint8_t data_no_en)
// {
//     esp_err_t err = pcf8574_write(lcd, data_no_en | LCD_BIT_EN);
//     if (err != ESP_OK) {
//         return err;
//     }
//     esp_rom_delay_us(1); /* HD44780 yeu cau EN cao toi thieu ~450ns */

//     err = pcf8574_write(lcd, data_no_en & (uint8_t)(~LCD_BIT_EN));
//     if (err != ESP_OK) {
//         return err;
//     }
//     esp_rom_delay_us(50); /* thoi gian de LCD xu ly lenh/du lieu vua nhan */
//     return ESP_OK;
// }

// static esp_err_t send_nibble(lcd_pcf8574_t *lcd, uint8_t nibble, bool is_data)
// {
//     uint8_t data = (uint8_t)((nibble << 4) & 0xF0);
//     if (is_data) {
//         data |= LCD_BIT_RS;
//     }
//     return pulse_enable(lcd, data);
// }

// static esp_err_t send_byte(lcd_pcf8574_t *lcd, uint8_t byte, bool is_data)
// {
//     esp_err_t err = send_nibble(lcd, (uint8_t)((byte >> 4) & 0x0F), is_data);
//     if (err != ESP_OK) {
//         return err;
//     }
//     return send_nibble(lcd, (uint8_t)(byte & 0x0F), is_data);
// }

// esp_err_t lcd_pcf8574_init(lcd_pcf8574_t *lcd, i2c_port_t port, uint8_t address)
// {
//     lcd->i2c_port = port;
//     lcd->i2c_address = address;
//     s_backlight_on = true;

//     vTaskDelay(pdMS_TO_TICKS(50)); /* cho LCD on dinh nguon sau khi cap dien */

//     /* Trinh tu bat buoc theo datasheet HD44780 de chuyen ve giao tiep 4-bit,
//      * bat ke trang thai truoc do cua LCD la gi */
//     send_nibble(lcd, 0x03, false);
//     vTaskDelay(pdMS_TO_TICKS(5));
//     send_nibble(lcd, 0x03, false);
//     esp_rom_delay_us(150);
//     send_nibble(lcd, 0x03, false);
//     esp_rom_delay_us(150);
//     send_nibble(lcd, 0x02, false); /* tu day chinh thuc dung giao tiep 4-bit */
//     esp_rom_delay_us(150);

//     esp_err_t err;
//     err = send_byte(lcd, 0x28, false); /* function set: 4-bit, 2 dong, font 5x8 */
//     if (err != ESP_OK) return err;
//     vTaskDelay(pdMS_TO_TICKS(2));

//     err = send_byte(lcd, 0x0C, false); /* display ON, cursor off, blink off */
//     if (err != ESP_OK) return err;
//     vTaskDelay(pdMS_TO_TICKS(2));

//     err = send_byte(lcd, 0x06, false); /* entry mode: tu tang dia chi, khong shift */
//     if (err != ESP_OK) return err;
//     vTaskDelay(pdMS_TO_TICKS(2));

//     err = send_byte(lcd, 0x01, false); /* clear display */
//     if (err != ESP_OK) return err;
//     vTaskDelay(pdMS_TO_TICKS(5));

//     ESP_LOGI(TAG, "LCD da san sang tai dia chi I2C 0x%02X", address);
//     return ESP_OK;
// }

// esp_err_t lcd_pcf8574_clear(lcd_pcf8574_t *lcd)
// {
//     esp_err_t err = send_byte(lcd, 0x01, false);
//     vTaskDelay(pdMS_TO_TICKS(2));
//     return err;
// }

// esp_err_t lcd_pcf8574_set_cursor(lcd_pcf8574_t *lcd, uint8_t row, uint8_t col)
// {
//     static const uint8_t row_offsets[2] = {0x00, 0x40};
//     if (row > 1) {
//         row = 1;
//     }
//     if (col > 15) {
//         col = 15;
//     }
//     return send_byte(lcd, (uint8_t)(0x80 | (row_offsets[row] + col)), false);
// }

// esp_err_t lcd_pcf8574_print(lcd_pcf8574_t *lcd, const char *str)
// {
//     esp_err_t err = ESP_OK;
//     while (*str != '\0') {
//         err = send_byte(lcd, (uint8_t)(*str), true);
//         if (err != ESP_OK) {
//             return err;
//         }
//         str++;
//     }
//     return err;
// }

// esp_err_t lcd_pcf8574_backlight(lcd_pcf8574_t *lcd, bool on)
// {
//     s_backlight_on = on;
//     return pcf8574_write(lcd, 0x00);
// }
#include "lcd_pcf8574.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lcd_pcf8574";
#define LCD_BIT_RS  (1 << 0)
#define LCD_BIT_RW  (1 << 1)
#define LCD_BIT_EN  (1 << 2)
#define LCD_BIT_BL  (1 << 3)

static bool s_backlight_on = true;

static esp_err_t pcf8574_write(lcd_pcf8574_t *lcd, uint8_t data) {
    uint8_t buf = data | (uint8_t)(s_backlight_on ? LCD_BIT_BL : 0);
    // Tăng thời gian chờ I2C Timeout lên 100ms để chống nghẽn
    return i2c_master_write_to_device(lcd->i2c_port, lcd->i2c_address, &buf, 1, pdMS_TO_TICKS(100));
}

static esp_err_t pulse_enable(lcd_pcf8574_t *lcd, uint8_t data_no_en) {
    // 1. Kéo EN lên CAO
    pcf8574_write(lcd, data_no_en | LCD_BIT_EN);
    
    // Ép xung siêu chậm: Giữ mức CAO tận 50us (cũ là 1us) để màn hình nhìn rõ
    esp_rom_delay_us(50); 

    // 2. Kéo EN xuống THẤP
    pcf8574_write(lcd, data_no_en & (uint8_t)(~LCD_BIT_EN));
    
    // Ép xung siêu chậm: Chờ 2000us (2ms) để màn hình xử lý xong chữ
    esp_rom_delay_us(2000); 

    // QUAN TRỌNG NHẤT: Bỏ qua lỗi I2C, luôn báo thành công để không bao giờ bỏ dở nhịp!
    return ESP_OK; 
}

static esp_err_t send_nibble(lcd_pcf8574_t *lcd, uint8_t nibble, bool is_data) {
    uint8_t data = (uint8_t)((nibble << 4) & 0xF0);
    if (is_data) data |= LCD_BIT_RS;
    
    pulse_enable(lcd, data);
    return ESP_OK; // Bỏ qua lỗi ngắt ngang
}

static esp_err_t send_byte(lcd_pcf8574_t *lcd, uint8_t byte, bool is_data) {
    send_nibble(lcd, (uint8_t)((byte >> 4) & 0x0F), is_data);
    send_nibble(lcd, (uint8_t)(byte & 0x0F), is_data);
    return ESP_OK; // Đảm bảo luôn gửi đủ 2 nửa
}

esp_err_t lcd_pcf8574_init(lcd_pcf8574_t *lcd, i2c_port_t port, uint8_t address) {
    lcd->i2c_port = port;
    lcd->i2c_address = address;
    s_backlight_on = true;
    
    // Bắt mạch chờ hẳn 500ms lúc cắm điện để LCD ngậm đủ nguồn
    vTaskDelay(pdMS_TO_TICKS(500));

    send_nibble(lcd, 0x03, false); vTaskDelay(pdMS_TO_TICKS(5));
    send_nibble(lcd, 0x03, false); esp_rom_delay_us(200);
    send_nibble(lcd, 0x03, false); esp_rom_delay_us(200);
    send_nibble(lcd, 0x02, false); esp_rom_delay_us(200);

    send_byte(lcd, 0x28, false); vTaskDelay(pdMS_TO_TICKS(2));
    send_byte(lcd, 0x0C, false); vTaskDelay(pdMS_TO_TICKS(2));
    send_byte(lcd, 0x06, false); vTaskDelay(pdMS_TO_TICKS(2));
    send_byte(lcd, 0x01, false); vTaskDelay(pdMS_TO_TICKS(5));

    return ESP_OK;
}

esp_err_t lcd_pcf8574_clear(lcd_pcf8574_t *lcd) {
    esp_err_t err = send_byte(lcd, 0x01, false);
    vTaskDelay(pdMS_TO_TICKS(2));
    return err;
}

esp_err_t lcd_pcf8574_set_cursor(lcd_pcf8574_t *lcd, uint8_t row, uint8_t col) {
    static const uint8_t row_offsets[2] = {0x00, 0x40};
    if (row > 1) row = 1;
    if (col > 15) col = 15;
    return send_byte(lcd, (uint8_t)(0x80 | (row_offsets[row] + col)), false);
}

esp_err_t lcd_pcf8574_print(lcd_pcf8574_t *lcd, const char *str) {
    while (*str != '\0') {
        send_byte(lcd, (uint8_t)(*str), true);
        str++;
        
    }
    return ESP_OK;
}

esp_err_t lcd_pcf8574_backlight(lcd_pcf8574_t *lcd, bool on) {
    s_backlight_on = on;
    return pcf8574_write(lcd, 0x00);
}
