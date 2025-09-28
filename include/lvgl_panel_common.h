#pragma once

#include <esp32_smartdisplay.h>
#include <esp_lcd_panel_io.h>
#if SOC_LCD_RGB_SUPPORTED
#include <esp_lcd_panel_rgb.h>
#endif

static inline lv_display_t *lvgl_create_display()
{
    lv_display_t *display = lv_display_create(LVGL_HOR_RES, LVGL_VER_RES);
    lv_color_format_t cf = lv_display_get_color_format(display);
    uint32_t px_size = lv_color_format_get_size(cf);
    uint32_t drawBufferSize = px_size * LVGL_BUFFER_PIXELS;
    void *drawBuffer = heap_caps_malloc(drawBufferSize, LVGL_BUFFER_MALLOC_FLAGS);
    lv_display_set_buffers(display, drawBuffer, NULL, drawBufferSize, LVGL_DISPLAY_RENDER_MODE);
    return display;
}

#if SOC_LCD_RGB_SUPPORTED
static inline bool lvgl_panel_frame_trans_done(esp_lcd_panel_handle_t panel, esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    lv_display_t *display = (lv_display_t *)user_ctx;
    lv_display_flush_ready(display);
    return false;
}
#endif

static inline bool lvgl_panel_color_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *display = (lv_display_t *)user_ctx;
    lv_display_flush_ready(display);
    return false;
}

// Hardware rotation is supported
static inline void lv_flush_hardware(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(display);
    uint16_t *p = (uint16_t *)px_map;
    uint32_t pixels = lv_area_get_size(area);
    if (display->color_format == LV_COLOR_FORMAT_RGB565)
    {
        while (pixels--)
        {
            *p = __builtin_bswap16(*p);
            p++;
        }
    }

    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map));
};

// Hardware rotation is not supported
static inline void lv_flush_software(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    const esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(display);
    lv_display_rotation_t rotation = lv_display_get_rotation(display);
    if (rotation == LV_DISPLAY_ROTATION_0)
    {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map));
        return;
    }

    // Rotated
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    lv_color_format_t cf = lv_display_get_color_format(display);
    uint32_t px_size = lv_color_format_get_size(cf);
    size_t buf_size = w * h * px_size;
    log_v("alloc rotation buffer to: %u bytes", buf_size);
    void *rotation_buffer = heap_caps_malloc(buf_size, LVGL_BUFFER_MALLOC_FLAGS);
    assert(rotation_buffer != NULL);

    uint32_t w_stride = lv_draw_buf_width_to_stride(w, cf);
    uint32_t h_stride = lv_draw_buf_width_to_stride(h, cf);

    switch (rotation)
    {
    case LV_DISPLAY_ROTATION_90:
        lv_draw_sw_rotate(px_map, rotation_buffer, w, h, w_stride, h_stride, rotation, cf);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, area->y1, display->ver_res - area->x1 - w, area->y1 + h, display->ver_res - area->x1, rotation_buffer));
        break;
    case LV_DISPLAY_ROTATION_180:
        lv_draw_sw_rotate(px_map, rotation_buffer, w, h, w_stride, w_stride, rotation, cf);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, display->hor_res - area->x1 - w, display->ver_res - area->y1 - h, display->hor_res - area->x1, display->ver_res - area->y1, rotation_buffer));
        break;
    case LV_DISPLAY_ROTATION_270:
        lv_draw_sw_rotate(px_map, rotation_buffer, w, h, w_stride, h_stride, rotation, cf);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, display->hor_res - area->y2 - 1, area->x1, display->hor_res - area->y2 - 1 + h, area->x2 + 1, rotation_buffer));
        break;
    default:
        assert(false);
        break;
    }

    free(rotation_buffer);
};

static inline void lv_flush_oled(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    const esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(display);

    // To use LV_COLOR_FORMAT_I1, we need an extra buffer to hold the converted data
    static uint8_t oled_buffer[LVGL_HOR_RES * LVGL_VER_RES / 8];

    //  This is necessary because LVGL reserves 2 x 4 bytes in the buffer, as these are assumed to be used as a palette. Skip the palette here
    //  More information about the monochrome, please refer to https://docs.lvgl.io/9.2/porting/display.html#monochrome-displays
    px_map += 8;

    const size_t px_buf_size = lv_display_get_draw_buf_size(display);
    lv_draw_sw_i1_convert_to_vtiled(px_map, px_buf_size, LVGL_HOR_RES, LVGL_VER_RES, oled_buffer, sizeof(oled_buffer), false);

    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(panel_handle, 0,0, LVGL_HOR_RES, LVGL_VER_RES, oled_buffer);
}

static inline void lvgl_setup_panel(esp_lcd_panel_handle_t panel_handle)
{
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
#ifdef DISPLAY_INVERT
    // If LCD is IPS invert the colors
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif
#if (DISPLAY_SWAP_XY)
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, DISPLAY_SWAP_XY));
#endif
#if (DISPLAY_MIRROR_X || DISPLAY_MIRROR_Y)
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
#endif
#if (DISPLAY_GAP_X || DISPLAY_GAP_Y)
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, DISPLAY_GAP_X, DISPLAY_GAP_Y));
#endif
    // Turn display on
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
};