#include "signal_filter.h"
#include <string.h>

void signal_filter_init(signal_filter_t *filter, float ema_alpha)
{
    memset(filter, 0, sizeof(*filter));
    filter->ema_alpha = ema_alpha;
    filter->ema_initialized = false;
}

void signal_filter_reset(signal_filter_t *filter)
{
    float alpha = filter->ema_alpha;
    memset(filter, 0, sizeof(*filter));
    filter->ema_alpha = alpha;
}

float signal_filter_median(const float *samples, size_t n)
{
    float tmp[16];

    if (n == 0) {
        return 0.0f;
    }
    if (n > 16) {
        n = 16;
    }
    memcpy(tmp, samples, n * sizeof(float));

    /* Insertion sort: đủ nhanh vì n luôn nhỏ (<=16) trong dự án này */
    for (size_t i = 1; i < n; i++) {
        float key = tmp[i];
        size_t j = i;
        while (j > 0 && tmp[j - 1] > key) {
            tmp[j] = tmp[j - 1];
            j--;
        }
        tmp[j] = key;
    }

    return tmp[n / 2];
}

float signal_filter_process(signal_filter_t *filter, float new_sample)
{
    /* Đẩy mẫu mới vào cửa sổ trượt (circular buffer) */
    filter->history[filter->index] = new_sample;
    filter->index = (uint8_t)((filter->index + 1) % SIGNAL_FILTER_MEDIAN_WINDOW);
    if (filter->count < SIGNAL_FILTER_MEDIAN_WINDOW) {
        filter->count++;
    }

    float median = signal_filter_median(filter->history, filter->count);

    if (!filter->ema_initialized) {
        /* Lần đầu: nhận giá trị ngay, tránh hiệu ứng "khởi động từ 0" */
        filter->ema_value = median;
        filter->ema_initialized = true;
    } else {
        filter->ema_value = filter->ema_alpha * median
                             + (1.0f - filter->ema_alpha) * filter->ema_value;
    }

    return filter->ema_value;
}