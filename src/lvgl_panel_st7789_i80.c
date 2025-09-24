#ifdef DISPLAY_ST7789_I80

#include <esp32_smartdisplay.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <lvgl_panel_common.h>

lv_display_t *lvgl_lcd_init(uint32_t hor_res, uint32_t ver_res)
{
    lv_display_t *display = lvgl_create_display();
    log_v("display:%p", display);

    pinMode(ST7789_RD, OUTPUT);
    digitalWrite(ST7789_RD, HIGH);

    const esp_lcd_i80_bus_config_t i80_bus_config = {
        .clk_src = ST7789_I80_BUS_CONFIG_CLK_SRC,
        .dc_gpio_num = ST7789_I80_BUS_CONFIG_DC,
        .wr_gpio_num = ST7789_I80_BUS_CONFIG_WR,
        .data_gpio_nums = {
            ST7789_I80_BUS_CONFIG_DATA_GPIO_D8,
            ST7789_I80_BUS_CONFIG_DATA_GPIO_D9,
            ST7789_I80_BUS_CONFIG_DATA_GPIO_D10,
            ST7789_I80_BUS_CONFIG_DATA_GPIO_D11,
            ST7789_I80_BUS_CONFIG_DATA_GPIO_D12,
            ST7789_I80_BUS_CONFIG_DATA_GPIO_D13,
            ST7789_I80_BUS_CONFIG_DATA_GPIO_D14,
            ST7789_I80_BUS_CONFIG_DATA_GPIO_D15},
        .bus_width = ST7789_I80_BUS_CONFIG_BUS_WIDTH,
        // transfer 100 lines of pixels (assume pixel is RGB565) at most in one transaction
        .max_transfer_bytes = ST7789_I80_BUS_CONFIG_MAX_TRANSFER_BYTES,
        .psram_trans_align = ST7789_I80_BUS_CONFIG_PSRAM_TRANS_ALIGN,
        .sram_trans_align = ST7789_I80_BUS_CONFIG_SRAM_TRANS_ALIGN};
    log_d("i80_bus_config: clk_src:%d, dc_gpio_num:%d, wr_gpio_num:%d, data_gpio_nums:[%d,%d,%d,%d,%d,%d,%d,%d], bus_width:%d, max_transfer_bytes:%d, psram_trans_align:%d, sram_trans_align:%d", i80_bus_config.clk_src, i80_bus_config.dc_gpio_num, i80_bus_config.wr_gpio_num, i80_bus_config.data_gpio_nums[0], i80_bus_config.data_gpio_nums[1], i80_bus_config.data_gpio_nums[2], i80_bus_config.data_gpio_nums[3], i80_bus_config.data_gpio_nums[4], i80_bus_config.data_gpio_nums[5], i80_bus_config.data_gpio_nums[6], i80_bus_config.data_gpio_nums[7], i80_bus_config.bus_width, i80_bus_config.max_transfer_bytes, i80_bus_config.psram_trans_align, i80_bus_config.sram_trans_align);
    esp_lcd_i80_bus_handle_t i80_bus;
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&i80_bus_config, &i80_bus));

    // Create direct_io panel handle
    esp_lcd_panel_io_i80_config_t io_i80_config = {
        .cs_gpio_num = ST7789_IO_I80_CONFIG_CS,
        .pclk_hz = ST7789_IO_I80_CONFIG_PCLK_HZ,
        .on_color_trans_done = lvgl_panel_color_trans_done,
        .user_ctx = display,
        .trans_queue_depth = ST7789_IO_I80_CONFIG_TRANS_QUEUE_DEPTH,
        .lcd_cmd_bits = ST7789_IO_I80_CONFIG_LCD_CMD_BITS,
        .lcd_param_bits = ST7789_IO_I80_CONFIG_LCD_PARAM_BITS,
        .dc_levels = {
            .dc_idle_level = ST7789_IO_I80_CONFIG_DC_LEVELS_DC_IDLE_LEVEL,
            .dc_cmd_level = ST7789_IO_I80_CONFIG_DC_LEVELS_DC_CMD_LEVEL,
            .dc_dummy_level = ST7789_IO_I80_CONFIG_DC_LEVELS_DC_DUMMY_LEVEL,
            .dc_data_level = ST7789_IO_I80_CONFIG_DC_LEVELS_DC_DATA_LEVEL},
        .flags = {.cs_active_high = ST7789_IO_I80_CONFIG_FLAGS_CS_ACTIVE_HIGH, .reverse_color_bits = ST7789_IO_I80_CONFIG_FLAGS_REVERSE_COLOR_BITS, .swap_color_bytes = ST7789_IO_I80_CONFIG_FLAGS_SWAP_COLOR_BYTES, .pclk_active_neg = ST7789_IO_I80_CONFIG_FLAGS_PCLK_ACTIVE_NEG, .pclk_idle_low = ST7789_IO_I80_CONFIG_FLAGS_PCLK_IDLE_LOW}};
    log_d("io_i80_config: cs_gpio_num:%d, pclk_hz:%d, on_color_trans_done:%p, user_ctx:%p, trans_queue_depth:%d, lcd_cmd_bits:%d, lcd_param_bits:%d, dc_levels:{dc_idle_level:%d, dc_cmd_level:%d, dc_dummy_level:%d, dc_data_level:%d}, flags:{cs_active_high:%d, reverse_color_bits:%d, swap_color_bytes:%d, pclk_active_neg:%d, pclk_idle_low:%d}", io_i80_config.cs_gpio_num, io_i80_config.pclk_hz, io_i80_config.on_color_trans_done, io_i80_config.user_ctx, io_i80_config.trans_queue_depth, io_i80_config.lcd_cmd_bits, io_i80_config.lcd_param_bits, io_i80_config.dc_levels.dc_idle_level, io_i80_config.dc_levels.dc_cmd_level, io_i80_config.dc_levels.dc_dummy_level, io_i80_config.dc_levels.dc_data_level, io_i80_config.flags.cs_active_high, io_i80_config.flags.reverse_color_bits, io_i80_config.flags.swap_color_bytes, io_i80_config.flags.pclk_active_neg, io_i80_config.flags.pclk_idle_low);
    esp_lcd_panel_io_handle_t io_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_i80_config, &io_handle));

    // Create ST7789 panel handle
    const esp_lcd_panel_dev_config_t panel_dev_config = {
        .reset_gpio_num = ST7789_DEV_CONFIG_RESET,
        .color_space = ST7789_DEV_CONFIG_COLOR_SPACE,
        .bits_per_pixel = ST7789_DEV_CONFIG_BITS_PER_PIXEL,
        .flags = {
            .reset_active_high = ST7789_DEV_CONFIG_FLAGS_RESET_ACTIVE_HIGH},
        .vendor_config = ST7789_DEV_CONFIG_VENDOR_CONFIG};
    log_d("panel_dev_config: reset_gpio_num:%d, color_space:%d, bits_per_pixel:%d, flags:{reset_active_high:%d}, vendor_config:%p", panel_dev_config.reset_gpio_num, panel_dev_config.color_space, panel_dev_config.bits_per_pixel, panel_dev_config.flags.reset_active_high, panel_dev_config.vendor_config);
    esp_lcd_panel_handle_t panel_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_dev_config, &panel_handle));

    lvgl_setup_panel(panel_handle);
    display->user_data = panel_handle;
    display->flush_cb = lv_flush_hardware;
    return display;
}

#endif