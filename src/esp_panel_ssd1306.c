#ifdef DISPLAY_SSD1306

#include <esp_panel_ssd1306.h>
#include <esp32-hal-log.h>
#include <esp_rom_gpio.h>
#include <esp_heap_caps.h>
#include <memory.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_panel_io.h>

typedef struct
{
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t panel_io_handle;
    esp_lcd_panel_dev_config_t panel_dev_config;
    // Data
    int x_gap;
    int y_gap;
    bool swap_xy;
} ssd1306_panel_t;

// SSD1306 commands
#define SSD1306_CMD_SET_MEMORY_ADDR_MODE 0x20
#define SSD1306_CMD_SET_COLUMN_RANGE 0x21
#define SSD1306_CMD_SET_PAGE_RANGE 0x22
#define SSD1306_CMD_ACTIVATE_SCROLL 0x2F
#define SSD1306_CMD_DEACTIVATE_SCROLL 0x2E
#define SSD1306_CMD_SET_VERTICAL_SCROLL_AREA 0xA3
#define SSD1306_CMD_RIGHT_HORIZONTAL_SCROLL 0x26
#define SSD1306_CMD_LEFT_HORIZONTAL_SCROLL 0x27
#define SSD1306_CMD_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_CMD_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A
#define SSD1306_CMD_SET_START_LINE 0x40
#define SSD1306_CMD_SET_CONTRAST 0x81
#define SSD1306_CMD_SET_CHARGE_PUMP 0x8D
#define SSD1306_CMD_MIRROR_X_OFF 0xA0
#define SSD1306_CMD_MIRROR_X_ON 0xA1
#define SSD1306_CMD_ALL_ON_RESUME 0xA4
#define SSD1306_CMD_ALL_ON 0xA5
#define SSD1306_CMD_INVERT_OFF 0xA6
#define SSD1306_CMD_INVERT_ON 0xA7
#define SSD1306_CMD_SET_MULTIPLEX 0xA8
#define SSD1306_CMD_DISP_OFF 0xAE
#define SSD1306_CMD_DISP_ON 0xAF
#define SSD1306_CMD_MIRROR_Y_OFF 0xC0
#define SSD1306_CMD_MIRROR_Y_ON 0xC8
#define SSD1306_CMD_SET_DISPLAY_OFFSET 0xD3
#define SSD1306_CMD_SET_CLOCK_DIV 0xD5
#define SSD1306_CMD_SET_PRECHARGE 0xD9
#define SSD1306_CMD_SET_COMPINS 0xDA
#define SSD1306_CMD_SET_VCOM_DESELECT_LEVEL 0xDB

static esp_err_t ssd1306_reset(esp_lcd_panel_t *panel)
{
    log_v("panel:0x%08x", panel);
    if (panel == NULL)
        return ESP_ERR_INVALID_ARG;

    const ssd1306_panel_t *ph = (ssd1306_panel_t *)panel;

    if (ph->panel_dev_config.reset_gpio_num != GPIO_NUM_NC)
    {
        // Hardware reset
        gpio_set_level(ph->panel_dev_config.reset_gpio_num, ph->panel_dev_config.flags.reset_active_high);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ph->panel_dev_config.reset_gpio_num, !ph->panel_dev_config.flags.reset_active_high);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return ESP_OK;
}

static esp_err_t ssd1306_init(esp_lcd_panel_t *panel)
{
    log_v("panel:0x%08x", panel);
    if (panel == NULL)
        return ESP_ERR_INVALID_ARG;

    const ssd1306_panel_t *ph = (ssd1306_panel_t *)panel;

    if (ph->panel_dev_config.bits_per_pixel != 1)
    {
        log_e("Invalid bits per pixel: %d. Only 1bpp is supported for SSD1306", ph->panel_dev_config.bits_per_pixel);
        return ESP_ERR_INVALID_ARG;
    }

    const esp_lcd_panel_io_handle_t io = ph->panel_io_handle;
    esp_err_t res;
    // Display OFF (sleep mode)
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_DISP_OFF, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_DISP_OFF failed");
        return res;
    }
    // Set display clock divide ratio/oscillator frequency
    if ((res = esp_lcd_panel_io_rx_param(io, SSD1306_CMD_SET_CLOCK_DIV, (uint8_t[]){0x80}, 1)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_CLOCK_DIV failed");
        return res;
    }
    // Set multiplex ratio(1 to 64)
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_MULTIPLEX, (uint8_t[]){DISPLAY_HEIGHT - 1}, 1)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_MULTIPLEX failed");
        return res;
    }
    // Set display offset. 00 = no offset
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_DISPLAY_OFFSET, (uint8_t[]){0x00}, 1)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_DISPLAY_OFFSET failed");
        return res;
    }
    // Set start line address
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_START_LINE, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_START_LINE failed");
        return res;
    }
    // Set Chargepump: 0x10 = external Vcc, 0x14 = DC-DC
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_CHARGE_PUMP, (uint8_t[]){0x14}, 1)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_CHARGE_PUMP failed");
        return res;
    }
    // Set memory addressing mode: 00=Horizontal Addressing Mode; 01=Vertical Addressing Mode; 10=Page Addressing Mode (RESET); 11=Invalid
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_MEMORY_ADDR_MODE, (uint8_t[]){0x00}, 1)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_MEMORY_ADDR_MODE failed");
        return res;
    }
    // Set Segment Re-map. A0=address mapped; A1=address 127 mapped. Use A1 for 128x64
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_MIRROR_X_OFF, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_MIRROR_X_OFF failed");
        return res;
    }
    // Set COM Output Scan Direction to increase
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_MIRROR_Y_OFF, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_MIRROR_Y_OFF failed");
        return res;
    }
    // Set com pins hardware configuration
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_COMPINS, (uint8_t[]){0x12}, 1)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_COMPINS failed");
        return res;
    }
    // Set contrast control register: external Vcc: 0x9F; internal Vcc: 0xCF
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_CONTRAST, (uint8_t[]){0xCF}, 1)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_CONTRAST failed");
        return res;
    }
    // Set pre-charge period: external Vcc: 0x22; internal Vcc: 0xF1
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_PRECHARGE, (uint8_t[]){0xF1}, 1)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_PRECHARGE failed");
        return res;
    }
    // Set VCOMH Deselect Level: 0x00=0.65xVcc; 0x20=0.77xVcc; 0x30=0.83xVcc
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_VCOM_DESELECT_LEVEL, (uint8_t[]){0x40}, 1)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_SET_VCOM_DESELECT_LEVEL failed");
        return res;
    }
    // Output RAM to Display: 0xA4=Output follows RAM content, 0xA5=Output ignores RAM content
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_ALL_ON_RESUME, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_ALL_ON_RESUME failed");
        return res;
    }
    // Set display mode. A6=Normal; A7=Inverse
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_INVERT_OFF, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_INVERT_OFF failed");
        return res;
    }
    // Disable scrolling
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_DEACTIVATE_SCROLL, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_DEACTIVATE_SCROLL failed");
        return res;
    }
    // Display ON in normal mode
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_DISP_ON, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_DISP_ON failed.");
        return res;
    }

    return ESP_OK;
}

static esp_err_t ssd1306_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    log_v("panel:0x%08x, x_start:%d, y_start:%d, x_end:%d, y_end:%d", panel, x_start, y_start, x_end, y_end);
    if (panel == NULL || color_data == NULL)
        return ESP_ERR_INVALID_ARG;

    const ssd1306_panel_t *ph = (ssd1306_panel_t *)panel;

    if (x_start >= x_end)
    {
        log_w("X-start greater than the x-end");
        return ESP_ERR_INVALID_ARG;
    }

    if (y_start >= y_end)
    {
        log_w("Y-start greater than the y-end");
        return ESP_ERR_INVALID_ARG;
    }

    // Correct for gap
    x_start += ph->x_gap;
    x_end += ph->x_gap;
    y_start += ph->y_gap;
    y_end += ph->y_gap;

    if (ph->swap_xy)
    {
        int temp = x_start;
        x_start = y_start;
        y_start = temp;
        temp = x_end;
        x_end = y_end;
        y_end = temp;
    }

    esp_lcd_panel_io_handle_t io = ph->panel_io_handle;
    esp_err_t res;

    // one page contains 8 rows (COMs)
    uint8_t page_start = y_start / 8;
    uint8_t page_end = (y_end - 1) / 8;
    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_COLUMN_RANGE, (uint8_t[]){(x_start & 0x7F), ((x_end - 1) & 0x7F)}, 2)) != ESP_OK)
    {
        log_e("SSD1306_CMD_SET_COLUMN_RANGE failed");
        return res;
    }

    if ((res = esp_lcd_panel_io_tx_param(io, SSD1306_CMD_SET_PAGE_RANGE, (uint8_t[]){(page_start & 0x07), (page_end & 0x07)}, 2)) != ESP_OK)
    {
        log_e("io tx param SSD1306_CMD_SET_PAGE_RANGE failed");
        return res;
    }

    // transfer frame buffer
    size_t len = (y_end - y_start) * (x_end - x_start) * ph->panel_dev_config.bits_per_pixel / 8;
    if ((res = esp_lcd_panel_io_tx_color(io, -1, color_data, len)) != ESP_OK)
    {
        log_e("Sending color data failed");
        return res;
    }

    return ESP_OK;
}

static esp_err_t ssd1306_invert_color(esp_lcd_panel_t *panel, bool invert)
{
    log_v("panel:0x%08x, invert:%d", panel, invert);
    if (panel == NULL)
        return ESP_ERR_INVALID_ARG;

    const ssd1306_panel_t *ph = (ssd1306_panel_t *)panel;

    esp_err_t res;
    if ((res = esp_lcd_panel_io_tx_param(ph->panel_io_handle, invert ? SSD1306_CMD_INVERT_ON : SSD1306_CMD_INVERT_OFF, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_INVON/SSD1306_CMD_INVOFF failed");
        return res;
    }

    return ESP_OK;
}

static esp_err_t ssd1306_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    log_v("panel:0x%08x, mirror_x:%d, mirror_y:%d", panel, mirror_x, mirror_y);
    if (panel == NULL)
        return ESP_ERR_INVALID_ARG;

    ssd1306_panel_t *ph = (ssd1306_panel_t *)panel;
    esp_err_t res;

    // Set mirror x
    if ((res = esp_lcd_panel_io_tx_param(ph->panel_io_handle, mirror_x ? SSD1306_CMD_MIRROR_X_ON : SSD1306_CMD_MIRROR_X_OFF, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_MIRROR_X_ON/SSD1306_CMD_MIRROR_X_OFF failed");
        return res;
    }
    // Set mirror y
    if ((res = esp_lcd_panel_io_tx_param(ph->panel_io_handle, mirror_y ? SSD1306_CMD_MIRROR_Y_ON : SSD1306_CMD_MIRROR_Y_OFF, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_MIRROR_Y_ON/SSD1306_CMD_MIRROR_Y_OFF failed");
        return res;
    }

    return ESP_OK;
}

static esp_err_t ssd1306_swap_xy(esp_lcd_panel_t *panel, bool swap_xy)
{
    log_v("panel:0x%08x, swap_xy:%d", panel, swap_xy);
    if (panel == NULL)
        return ESP_ERR_INVALID_ARG;

    ssd1306_panel_t *ph = (ssd1306_panel_t *)panel;
    ph->swap_xy = swap_xy;

    return ESP_OK;
}

static esp_err_t ssd1306_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    log_v("panel:0x%08x, x_gap:%d, y_gap:%d", panel, x_gap, y_gap);
    if (panel == NULL)
        return ESP_ERR_INVALID_ARG;

    ssd1306_panel_t *ph = (ssd1306_panel_t *)panel;

    ph->x_gap = x_gap;
    ph->y_gap = y_gap;

    return ESP_OK;
}

static esp_err_t ssd1306_disp_off(esp_lcd_panel_t *panel, bool off)
{
    log_v("panel:0x%08x, off:%d", panel, off);
    if (panel == NULL)
        return ESP_ERR_INVALID_ARG;

    const ssd1306_panel_t *ph = (ssd1306_panel_t *)panel;

    esp_err_t res;
    if ((res = esp_lcd_panel_io_tx_param(ph->panel_io_handle, off ? SSD1306_CMD_DISP_OFF : SSD1306_CMD_DISP_ON, NULL, 0)) != ESP_OK)
    {
        log_e("Sending SSD1306_CMD_DISP_OFF/SSD1306_CMD_DISP_ON failed");
        return res;
    }

    return ESP_OK;
}

static esp_err_t ssd1306_del(esp_lcd_panel_t *panel)
{
    log_v("panel:0x%08x", panel);
    if (panel == NULL)
        return ESP_ERR_INVALID_ARG;

    ssd1306_panel_t *ph = (ssd1306_panel_t *)panel;

    // Reset RESET
    if (ph->panel_dev_config.reset_gpio_num != GPIO_NUM_NC)
        gpio_reset_pin(ph->panel_dev_config.reset_gpio_num);

    free(ph);

    return ESP_OK;
}

esp_err_t esp_lcd_new_panel_ssd1306(const esp_lcd_panel_io_handle_t panel_io_handle, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *panel_handle)
{
    log_v("panel_io_handle:0x%08x, panel_dev_config:0x%08x, panel_handle:0x%08x", panel_io_handle, panel_dev_config, panel_handle);
    if (panel_io_handle == NULL || panel_dev_config == NULL || panel_handle == NULL)
        return ESP_ERR_INVALID_ARG;

    if (panel_dev_config->reset_gpio_num != GPIO_NUM_NC && !GPIO_IS_VALID_GPIO(panel_dev_config->reset_gpio_num))
    {
        log_e("Invalid GPIO RST pin: %d", panel_dev_config->reset_gpio_num);
        return ESP_ERR_INVALID_ARG;
    }

    if (panel_dev_config->color_space != ESP_LCD_COLOR_SPACE_MONOCHROME)
    {
        log_e("Invalid color space: %d. Only MONOCHROME is supported", panel_dev_config->color_space);
        return ESP_ERR_INVALID_ARG;
    }

    if (panel_dev_config->reset_gpio_num != GPIO_NUM_NC)
    {
        esp_err_t res;
        const gpio_config_t cfg = {
            .pin_bit_mask = BIT64(panel_dev_config->reset_gpio_num),
            .mode = GPIO_MODE_OUTPUT};
        if ((res = gpio_config(&cfg)) != ESP_OK)
        {
            log_e("Configuring GPIO for RST failed");
            return res;
        }
    }

    ssd1306_panel_t *ph = heap_caps_calloc(1, sizeof(ssd1306_panel_t), MALLOC_CAP_DEFAULT);
    if (ph == NULL)
    {
        log_e("No memory available for ssd1306_panel_t");
        return ESP_ERR_NO_MEM;
    }

    ph->panel_io_handle = panel_io_handle;
    memcpy(&ph->panel_dev_config, panel_dev_config, sizeof(esp_lcd_panel_dev_config_t));

    ph->base.del = ssd1306_del;
    ph->base.reset = ssd1306_reset;
    ph->base.init = ssd1306_init;
    ph->base.draw_bitmap = ssd1306_draw_bitmap;
    ph->base.invert_color = ssd1306_invert_color;
    ph->base.mirror = ssd1306_mirror;
    ph->base.swap_xy = ssd1306_swap_xy;
    ph->base.set_gap = ssd1306_set_gap;
    ph->base.disp_off = ssd1306_disp_off;

    log_d("panel_handle: 0x%08x", ph);
    *panel_handle = (esp_lcd_panel_handle_t)ph;

    return ESP_OK;
}

#endif