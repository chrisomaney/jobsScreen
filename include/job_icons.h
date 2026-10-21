#pragma once

#include <lvgl.h>
#include <stdint.h>

// Kid-friendly pictogram for a job id from config.h (make_bed, feed_cat, …).
// Returns a coloured badge with a white silhouette inside; size is 48×48.
lv_obj_t *job_icon_create(lv_obj_t *parent, const char *job_id);
