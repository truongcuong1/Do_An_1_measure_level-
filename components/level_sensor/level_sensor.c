#include "level_sensor.h"
#include "signal_filter.h"

#include <stdbool.h>
#include <string.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "level_sensor";

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t         s_cali_handle = NULL;
static bool                      s_cali_enabled = false;
static level_sensor_config_t     s_cfg;

/*
 * Khởi tạo scheme hiệu chỉnh ADC phù hợp với chip đang dùng.
 * ESP32 (gốc) chỉ hỗ trợ "line fitting"; các chip mới hơn (S2/S3/C3...)
 * hỗ trợ "curve fitting" chính xác hơn. Viết theo cả 2 nhánh để code này
 * vẫn build đúng nếu sau này đổi sang chip ESP32 khác đời.
 */
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel,
                                  adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (ret != ESP_OK) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (ret != ESP_OK) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Da bat hieu chinh ADC (adc_cali)");
    } else {
        ESP_LOGW(TAG, "Chip khong ho tro adc_cali, dung quy doi tuyen tinh tho");
    }
    return ret == ESP_OK;
}

esp_err_t level_sensor_init(const level_sensor_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg = *config;

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &s_adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit loi: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,      /* cho phep do toi ~3.3V full-scale */
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(s_adc_handle, (adc_channel_t)s_cfg.adc_channel, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel loi: %s", esp_err_to_name(err));
        return err;
    }

    s_cali_enabled = adc_calibration_init(ADC_UNIT_2, (adc_channel_t)s_cfg.adc_channel,
                                           ADC_ATTEN_DB_12, &s_cali_handle);

    ESP_LOGI(TAG, "level_sensor san sang (channel=%d, divider=%.2f, vcc=%lumV)",
             s_cfg.adc_channel, s_cfg.divider_ratio, (unsigned long)s_cfg.sensor_vcc_mv);
    return ESP_OK;
}

static esp_err_t read_single_mv(int *out_mv)
{
    int raw = 0;
    esp_err_t err = adc_oneshot_read(s_adc_handle, (adc_channel_t)s_cfg.adc_channel, &raw);
    if (err != ESP_OK) {
        return err;
    }

    if (s_cali_enabled) {
        return adc_cali_raw_to_voltage(s_cali_handle, raw, out_mv);
    }

    /* Fallback nếu chip không hỗ trợ adc_cali: quy đổi thô 12-bit / 3.3V */
    *out_mv = (raw * 3300) / 4095;
    return ESP_OK;
}

esp_err_t level_sensor_read_mv(uint32_t *out_mv)
{
    if (out_mv == NULL || s_adc_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t n = s_cfg.oversample_count ? s_cfg.oversample_count : 1;
    if (n > 16) {
        n = 16;
    }

    float samples[16];
    for (uint8_t i = 0; i < n; i++) {
        int mv = 0;
        esp_err_t err = read_single_mv(&mv);
        if (err != ESP_OK) {
            return err;
        }
        samples[i] = (float)mv;
        vTaskDelay(pdMS_TO_TICKS(2)); /* giãn cách nhỏ giữa các mẫu oversample */
    }

    float median_mv = signal_filter_median(samples, n);
    *out_mv = (uint32_t)(median_mv + 0.5f);
    return ESP_OK;
}

esp_err_t level_sensor_read_distance_mm(float *out_distance_mm)
{
    if (out_distance_mm == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t v_adc_mv = 0;
    esp_err_t err = level_sensor_read_mv(&v_adc_mv);
    if (err != ESP_OK) {
        return err;
    }

    /* "Undo" cầu phân áp R2/R3 để suy ra điện áp thật tại chân OUT của cảm biến */
    float v_sensor_mv = (float)v_adc_mv / s_cfg.divider_ratio;

    /* Công thức US-016 ở chế độ đo đang chọn (RANGE nối GND -> 1m):
     *   khoang_cach_mm = range_full_scale_mm * Vout / Vcc                */
    float distance_mm = (float)s_cfg.range_full_scale_mm * v_sensor_mv
                         / (float)s_cfg.sensor_vcc_mv;

    if (distance_mm < (float)s_cfg.min_valid_mm
        || distance_mm > (float)s_cfg.max_valid_mm) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    *out_distance_mm = distance_mm;
    return ESP_OK;
}