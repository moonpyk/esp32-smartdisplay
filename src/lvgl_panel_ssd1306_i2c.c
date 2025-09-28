#ifdef PANEL_SSD1306

#include <esp32_smartdisplay.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>    // Include the header defining esp_lcd_panel_dev_config_t
#include <esp_lcd_panel_vendor.h> // Ensure this header is included for esp_lcd_panel_dev_config_t
#include <esp_lcd_panel_io_interface.h>
#include <driver/i2c.h>
#include <lvgl_panel_common.h>

lv_display_t *lvgl_lcd_init()
{
    lv_display_t *display = lvgl_create_display();
    log_v("display:%p", display);

    // Create I2C bus
    const i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SSD1306_I2C_CONFIG_SDA,
        .scl_io_num = SSD1306_I2C_CONFIG_SCL,
        .sda_pullup_en = SSD1306_I2C_CONFIG_SDA_PULLUP_EN,
        .scl_pullup_en = SSD1306_I2C_CONFIG_SCL_PULLUP_EN,
        .master = {
            .clk_speed = SSD1306_I2C_CONFIG_MASTER_CLK_SPEED},
        .clk_flags = SSD1306_I2C_CONFIG_CLK_FLAGS};
    log_d("i2c_config: mode:%d, sda_io_num:%d, scl_io_num:%d, sda_pullup_en:%d, scl_pullup_en:%d, master:{clk_speed:%d}, clk_flags:0x%04x", i2c_config.mode, i2c_config.sda_io_num, i2c_config.scl_io_num, i2c_config.sda_pullup_en, i2c_config.scl_pullup_en, i2c_config.master.clk_speed, i2c_config.clk_flags);
    ESP_ERROR_CHECK(i2c_param_config(SSD1306_I2C_HOST, &i2c_config));
    ESP_ERROR_CHECK(i2c_driver_install(SSD1306_I2C_HOST, i2c_config.mode, 0, 0, 0));

    // Create IO handle
    const esp_lcd_panel_io_i2c_config_t io_i2c_config = {
        .dev_addr = SSD1306_IO_I2C_CONFIG_DEV_ADDRESS,
        .on_color_trans_done = lvgl_panel_color_trans_done,
        .control_phase_bytes = SSD1306_IO_I2C_CONFIG_CONTROL_PHASE_BYTES,
        .user_ctx = display,
        .dc_bit_offset = SSD1306_IO_I2C_CONFIG_DC_BIT_OFFSET, // 6 for SSD1306, 0 for SH1107
        .lcd_cmd_bits = SSD1306_IO_I2C_CONFIG_LCD_CMD_BITS,
        .lcd_param_bits = SSD1306_IO_I2C_CONFIG_LCD_PARAM_BITS,
        .flags = {
            .dc_low_on_data = SSD1306_IO_I2C_CONFIG_FLAGS_DC_LOW_ON_DATA,
            .disable_control_phase = SSD1306_IO_I2C_CONFIG_FLAGS_DISABLE_CONTROL_PHASE}};
    log_d("io_i2c_config: dev_addr:0x%02x, on_color_trans_done:%p, control_phase_bytes:%d, user_ctx:%p, dc_bit_offset:%d, lcd_cmd_bits:%d, lcd_param_bits:%d, flags:{dc_low_on_data:%d, disable_control_phase:%d}", io_i2c_config.dev_addr, io_i2c_config.on_color_trans_done, io_i2c_config.control_phase_bytes, io_i2c_config.user_ctx, io_i2c_config.dc_bit_offset, io_i2c_config.lcd_cmd_bits, io_i2c_config.lcd_param_bits, io_i2c_config.flags.dc_low_on_data, io_i2c_config.flags.disable_control_phase);
    esp_lcd_panel_io_handle_t io_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)SSD1306_I2C_HOST, &io_i2c_config, &io_handle));

    const esp_lcd_panel_dev_config_t panel_dev_config = {
        .reset_gpio_num = SSD1306_DEV_CONFIG_RESET,
        .color_space = SSD1306_DEV_CONFIG_COLOR_SPACE,
        .bits_per_pixel = SSD1306_DEV_CONFIG_BITS_PER_PIXEL,
        .flags = {
            .reset_active_high = SSD1306_DEV_CONFIG_FLAGS_RESET_ACTIVE_HIGH}};
    log_d("panel_dev_config: reset_gpio_num:%d, color_space:%d, bits_per_pixel:%d, flags:{reset_active_high:%d}", panel_dev_config.reset_gpio_num, panel_dev_config.color_space, panel_dev_config.bits_per_pixel, panel_dev_config.flags.reset_active_high);
    esp_lcd_panel_handle_t panel_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_dev_config, &panel_handle));

    // SSD1306_SETCONTRAST (0x81), 0x8F is a reasonable default contrast value
    ESP_ERROR_CHECK(esp_lcd_panel_io_rx_param(io_handle, 0x81, (const uint8_t[]){0x8F}, 1));

    lvgl_setup_panel(panel_handle);
    lv_display_set_user_data(display, panel_handle);
    lv_display_set_flush_cb(display, lv_flush_oled);
    return display;
}

#endif