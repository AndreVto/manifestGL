#include "texture/pixel_layout.h"

#include <stdint.h>
#include <string.h>

static int checked_add(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static int checked_mul(size_t a, size_t b, size_t *out) {
    if (a != 0u && b > SIZE_MAX / a)
        return 0;
    *out = a * b;
    return 1;
}

static int aligned_size(size_t value, GLint alignment, size_t *out) {
    size_t mask;
    if (alignment != 1 && alignment != 2 && alignment != 4 && alignment != 8)
        return 0;
    mask = (size_t)alignment - 1u;
    if (value > SIZE_MAX - mask)
        return 0;
    *out = (value + mask) & ~mask;
    return 1;
}

static int layout_common(GLsizei width, GLsizei height, GLsizei depth, size_t bytes_per_pixel,
                         GLint alignment, GLint row_length, GLint image_height, GLint skip_pixels,
                         GLint skip_rows, GLint skip_images, int use_image_modes,
                         struct mgl_pixel_layout *layout) {
    size_t logical_row_pixels, logical_image_rows;
    size_t storage_row_bytes, row_stride, image_stride;
    size_t row_bytes, start = 0u, term, span;

    if (layout == NULL || width < 0 || height < 0 || depth < 0 || bytes_per_pixel == 0u ||
        row_length < 0 || skip_pixels < 0 || skip_rows < 0 ||
        (use_image_modes && (image_height < 0 || skip_images < 0)))
        return 0;
    memset(layout, 0, sizeof(*layout));
    if (!checked_mul((size_t)width, bytes_per_pixel, &row_bytes))
        return 0;
    logical_row_pixels = row_length != 0 ? (size_t)row_length : (size_t)width;
    if (!checked_mul(logical_row_pixels, bytes_per_pixel, &storage_row_bytes) ||
        !aligned_size(storage_row_bytes, alignment, &row_stride))
        return 0;
    logical_image_rows =
        use_image_modes && image_height != 0 ? (size_t)image_height : (size_t)height;
    if (!checked_mul(row_stride, logical_image_rows, &image_stride))
        return 0;

    if (width == 0 || height == 0 || depth == 0) {
        layout->row_bytes = row_bytes;
        layout->row_stride = row_stride;
        layout->image_stride = image_stride;
        return 1;
    }

    if (use_image_modes) {
        if (!checked_mul((size_t)skip_images, image_stride, &start))
            return 0;
    }
    if (!checked_mul((size_t)skip_rows, row_stride, &term) || !checked_add(start, term, &start) ||
        !checked_mul((size_t)skip_pixels, bytes_per_pixel, &term) ||
        !checked_add(start, term, &start))
        return 0;

    span = start;
    if (depth > 1) {
        if (!checked_mul((size_t)depth - 1u, image_stride, &term) ||
            !checked_add(span, term, &span))
            return 0;
    }
    if (height > 1) {
        if (!checked_mul((size_t)height - 1u, row_stride, &term) || !checked_add(span, term, &span))
            return 0;
    }
    if (!checked_add(span, row_bytes, &span))
        return 0;

    layout->row_bytes = row_bytes;
    layout->row_stride = row_stride;
    layout->image_stride = image_stride;
    layout->start_offset = start;
    layout->span_bytes = span;
    return 1;
}

int mgl_pixel_layout_2d(GLsizei width, GLsizei height, size_t bytes_per_pixel, GLint alignment,
                        GLint row_length, GLint skip_pixels, GLint skip_rows,
                        struct mgl_pixel_layout *layout) {
    return layout_common(width, height, 1, bytes_per_pixel, alignment, row_length, 0, skip_pixels,
                         skip_rows, 0, 0, layout);
}

int mgl_pixel_layout_3d(GLsizei width, GLsizei height, GLsizei depth, size_t bytes_per_pixel,
                        GLint alignment, GLint row_length, GLint image_height, GLint skip_pixels,
                        GLint skip_rows, GLint skip_images, struct mgl_pixel_layout *layout) {
    return layout_common(width, height, depth, bytes_per_pixel, alignment, row_length, image_height,
                         skip_pixels, skip_rows, skip_images, 1, layout);
}
