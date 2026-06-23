/**
 * main.c - Đo mức nước 0-1m bằng US-016 + ESP32, hiển thị LCD 16x2 qua I2C
 *
 * Kiến trúc:
 *   sensor_task   (chu kỳ 200ms, ưu tiên cao hơn)
 *       -> đọc cảm biến (level_sensor) -> lọc (signal_filter) -> áp dụng
 *          hiệu chỉnh (level_storage) -> tính mức nước -> ghi vào biến
 *          dùng chung (có mutex bảo vệ)
 *
 *   display_task  (chu kỳ 500ms, ưu tiên thấp hơn)
 *       -> đọc biến dùng chung -> chỉ vẽ lại LCD khi thay đổi đủ lớn
 *          (hysteresis) để tránh nhấp nháy màn hình
 *
 *   console "cal" -> hiệu chỉnh điểm 0 ngay trên thực địa, lưu vào NVS
 *       Cách dùng qua idf.py monitor:
 *           level> cal 150
 *       (nghĩa là: hiện tại mũi cảm biến đang cách mặt nước đúng 150mm)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_console.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

#include "app_config.h"
#include "level_sensor.h"
#include "signal_filter.h"
#include "lcd_pcf8574.h"
#include "level_storage.h"

static const char *TAG = "main";

/* ---------------------------------------------------------------------
 * Dữ liệu dùng chung giữa sensor_task và display_task ("mailbox pattern")
 * ------------------------------------------------------------------- */
typedef struct {
    float distance_mm;     /* khoảng cách cảm biến -> mặt nước, đã lọc + hiệu chỉnh */
    float level_mm;        /* mức nước = tank_height_mm - distance_mm, đã giới hạn [0, tank] */
    float level_percent;   /* level_mm / tank_height_mm * 100 */
    bool  valid;           /* false nếu lần đọc gần nhất lỗi/ngoài dải */
} shared_measurement_t;

static shared_measurement_t s_measurement = {0};
static SemaphoreHandle_t    s_data_mutex;
static level_calibration_t  s_calibration;
static signal_filter_t      s_distance_filter;
static lcd_pcf8574_t        s_lcd;

/* ---------------------------------------------------------------------
 * Lệnh console "cal <khoang_cach_thuc_te_mm>" để hiệu chỉnh điểm 0
 * ------------------------------------------------------------------- */
static int cmd_cal(int argc, char **argv)
{
    if (argc != 2) {
        printf("Cach dung: cal <khoang_cach_thuc_te_mm>\n");
        printf("Vi du: cal 150   (mui cam bien dang cach mat nuoc 150mm)\n");
        return 1;
    }

    float known_mm = strtof(argv[1], NULL);
    if (known_mm <= 0.0f) {
        printf("Gia tri khong hop le.\n");
        return 1;
    }

    float current_corrected_mm;
    bool  current_valid;
    /* FIX: Tránh Deadlock bằng cách chờ Mutex tối đa 100ms */
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_corrected_mm = s_measurement.distance_mm;
        current_valid = s_measurement.valid;
        xSemaphoreGive(s_data_mutex);
    } else {
        printf("Loi: He thong dang ban, khong the hieu chinh luc nay.\n");
        return 1;
    }

    if (!current_valid) {
        printf("Khong the hieu chinh: lan doc gan nhat bi loi, kiem tra cam bien truoc.\n");
        return 1;
    }

    /* distance hien tai = raw + offset_cu  =>  raw = distance_hien_tai - offset_cu */
    float raw_mm = current_corrected_mm - s_calibration.offset_mm;
    s_calibration.offset_mm = known_mm - raw_mm;

    esp_err_t err = level_storage_save(&s_calibration);
    if (err != ESP_OK) {
        printf("Luu NVS loi: %s\n", esp_err_to_name(err));
        return 1;
    }

    printf("Da hieu chinh xong. Offset moi = %.1f mm (da luu, con giu sau khi mat dien)\n",
           s_calibration.offset_mm);
    return 0;
}

static void register_console_commands(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    repl_config.prompt = "level>";

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    const esp_console_cmd_t cal_cmd = {
        .command = "cal",
        .help = "Hieu chinh diem 0: cal <khoang_cach_thuc_te_mm>",
        .hint = NULL,
        .func = &cmd_cal,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cal_cmd));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGI(TAG, "Console san sang. Go 'cal <mm>' de hieu chinh diem 0.");
}

/* ---------------------------------------------------------------------
 * Khởi tạo bus I2C dùng cho LCD
 * ------------------------------------------------------------------- */
static esp_err_t i2c_bus_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = APP_I2C_SDA_GPIO,
        .scl_io_num = APP_I2C_SCL_GPIO,
        /* Board đã có trở kéo ngoài 4.7K lên 3V3 (R4/R5) -> KHÔNG bật thêm
         * pull-up nội bộ của ESP32 để tránh làm méo thêm mức điện áp bus */
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = APP_I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(APP_I2C_PORT, &conf);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(APP_I2C_PORT, conf.mode, 0, 0, 0);
}

/* ---------------------------------------------------------------------
 * sensor_task: đo - lọc - hiệu chỉnh - tính mức nước
 * ------------------------------------------------------------------- */
static void sensor_task(void *arg)
{
    signal_filter_init(&s_distance_filter, APP_EMA_ALPHA);
    while (1) {
        float raw_distance_mm = 0.0f;
        esp_err_t err = level_sensor_read_distance_mm(&raw_distance_mm);

        /* FIX: Tránh Deadlock */
        if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (err == ESP_OK) {
                float filtered_mm = signal_filter_process(&s_distance_filter, raw_distance_mm);
                float corrected_mm = filtered_mm + s_calibration.offset_mm;
                s_measurement.distance_mm = corrected_mm;

                float level_mm = (float)s_calibration.tank_height_mm - corrected_mm;
                if (level_mm < 0.0f) level_mm = 0.0f;
                if (level_mm > (float)s_calibration.tank_height_mm) level_mm = (float)s_calibration.tank_height_mm;

                s_measurement.level_mm = level_mm;
                s_measurement.level_percent = (level_mm / (float)s_calibration.tank_height_mm) * 100.0f;
                s_measurement.valid = true;
            } else {
                s_measurement.valid = false;
            }
            xSemaphoreGive(s_data_mutex);
        } else {
            ESP_LOGW(TAG, "Cảnh báo: Không lấy được Mutex trong sensor_task");
        }
        vTaskDelay(pdMS_TO_TICKS(APP_SENSOR_PERIOD_MS));
    }
}
/* ---------------------------------------------------------------------
 * display_task: chỉ vẽ lại LCD khi giá trị thay đổi đủ lớn (hysteresis)
 * ------------------------------------------------------------------- */
static void display_task(void *arg)
{
    char line0[17];
    char line1[17];
    float last_shown_mm = -9999.0f;
    bool  last_valid = true; /* ép lần đầu luôn vẽ lại bằng cách lệch trạng thái */

    while (1) {
        shared_measurement_t m;
        xSemaphoreTake(s_data_mutex, portMAX_DELAY);
        m = s_measurement;
        xSemaphoreGive(s_data_mutex);

        bool changed_enough = fabsf(m.level_mm - last_shown_mm) >= APP_DISPLAY_HYSTERESIS_MM;
        bool validity_changed = (m.valid != last_valid);

        if (changed_enough || validity_changed) {
            if (m.valid) {
                snprintf(line0, sizeof(line0), "Muc nuoc:%4.0fmm", m.level_mm);
                snprintf(line1, sizeof(line1), "Ty le:   %5.1f%%", m.level_percent);
                /* ---- THÊM DÒNG NÀY ĐỂ IN RA MÁY TÍNH ---- */
                printf("=> [Terminal] Muc nuoc: %.0f mm | Ty le: %.1f %%\n", m.level_mm, m.level_percent);
            } else {
                snprintf(line0, sizeof(line0), "Loi cam bien!");
                snprintf(line1, sizeof(line1), "Kiem tra US-016");
                /* ---- THÊM DÒNG NÀY NỮA ---- */
                printf("=> [Terminal] Loi doc cam bien US-016!\n");
            }

            /* Ghi đè bằng khoảng trắng trước khi in chữ mới (thay vì lcd_clear)
             * để tránh hiệu ứng nhấp nháy toàn màn hình mỗi lần cập nhật */
            lcd_pcf8574_set_cursor(&s_lcd, 0, 0);
            lcd_pcf8574_print(&s_lcd, "                ");
            lcd_pcf8574_set_cursor(&s_lcd, 0, 0);
            lcd_pcf8574_print(&s_lcd, line0);

            lcd_pcf8574_set_cursor(&s_lcd, 1, 0);
            lcd_pcf8574_print(&s_lcd, "                ");
            lcd_pcf8574_set_cursor(&s_lcd, 1, 0);
            lcd_pcf8574_print(&s_lcd, line1);

            last_shown_mm = m.level_mm;
            last_valid = m.valid;
        }

        vTaskDelay(pdMS_TO_TICKS(APP_DISPLAY_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Khoi dong he thong do muc nuoc US-016 + ESP32");

    s_data_mutex = xSemaphoreCreateMutex();

    /* 1. NVS + hệ số hiệu chỉnh đã lưu từ trước (nếu có) */
    ESP_ERROR_CHECK(level_storage_init());
    level_storage_load(&s_calibration);
    ESP_LOGI(TAG, "Hieu chinh hien tai: offset=%.1fmm, chieu cao be=%lumm",
             s_calibration.offset_mm, (unsigned long)s_calibration.tank_height_mm);

    /* 2. I2C + LCD */
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(lcd_pcf8574_init(&s_lcd, APP_I2C_PORT, APP_LCD_I2C_ADDRESS));
    lcd_pcf8574_set_cursor(&s_lcd, 0, 0);
    lcd_pcf8574_print(&s_lcd, "He thong san sang");

    /* 3. Cảm biến US-016 */
    level_sensor_config_t sensor_cfg = {
        .adc_channel = APP_ADC_CHANNEL,
        .divider_ratio = APP_DIVIDER_RATIO,
        .sensor_vcc_mv = APP_SENSOR_VCC_MV,
        .range_full_scale_mm = APP_RANGE_FULL_SCALE_MM,
        .min_valid_mm = APP_SENSOR_MIN_MM,
        .max_valid_mm = APP_SENSOR_MAX_MM,
        .oversample_count = APP_OVERSAMPLE_COUNT,
    };
    ESP_ERROR_CHECK(level_sensor_init(&sensor_cfg));

    /* 4. Console hiệu chỉnh */
    register_console_commands();

    /* 5. Hai task độc lập, tách rời tốc độ đo và tốc độ hiển thị */
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 4, NULL);
}