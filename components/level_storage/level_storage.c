#include "level_storage.h"

#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "level_storage";

#define NVS_NAMESPACE   "level_cal"
#define NVS_KEY_OFFSET  "offset_mm"
#define NVS_KEY_TANK    "tank_mm"

/* Mặc định: chưa hiệu chỉnh (offset=0), bể cao 1000mm - khớp với chế độ
 * đo 1m đã chọn ở chân RANGE của US-016. Sửa lại theo bể thực tế của cậu
 * nếu khác, hoặc đơn giản hơn là dùng lệnh "cal" sau khi lắp đặt xong. */
#define DEFAULT_TANK_HEIGHT_MM 1000u

esp_err_t level_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS can xoa va khoi tao lai (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t level_storage_load(level_calibration_t *out_cal)
{
    if (out_cal == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Giá trị mặc định an toàn, dùng khi chưa từng lưu hoặc lỗi đọc NVS */
    out_cal->offset_mm = 0.0f;
    out_cal->tank_height_mm = DEFAULT_TANK_HEIGHT_MM;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Chua co du lieu hieu chinh, dung gia tri mac dinh");
        return ESP_OK;
    }

    uint32_t raw_offset_bits = 0;
    if (nvs_get_u32(handle, NVS_KEY_OFFSET, &raw_offset_bits) == ESP_OK) {
        memcpy(&out_cal->offset_mm, &raw_offset_bits, sizeof(float));
    }

    uint32_t tank_mm = DEFAULT_TANK_HEIGHT_MM;
    if (nvs_get_u32(handle, NVS_KEY_TANK, &tank_mm) == ESP_OK) {
        out_cal->tank_height_mm = tank_mm;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t level_storage_save(const level_calibration_t *cal)
{
    if (cal == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khong mo duoc NVS de luu: %s", esp_err_to_name(err));
        return err;
    }

    uint32_t raw_offset_bits;
    memcpy(&raw_offset_bits, &cal->offset_mm, sizeof(float));

    nvs_set_u32(handle, NVS_KEY_OFFSET, raw_offset_bits);
    nvs_set_u32(handle, NVS_KEY_TANK, cal->tank_height_mm);

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}