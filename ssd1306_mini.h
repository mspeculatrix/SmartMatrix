/**
 * @file ssd1306_mini.h
 * @brief Minimal SSD1306 SSD1306 OLED library for Pico C/C++ SDK.
 */

#ifndef __SSD1306_MINI_H__
#define __SSD1306_MINI_H__

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

 /** @brief Standard 0.96" SSD1306 OLED I2C bus address */
#define SSD1306_I2C_ADDR 0x3C
#define SSD1306_FONT_NORMAL 1
#define SSD1306_FONT_DOUBLE 2

/**
 * @brief Display instance holding peripheral bindings and memory buffer.
 */
typedef struct {
	i2c_inst_t* i2c;      /**< Pointer to initialized Pico I2C hardware instance */
	uint8_t buffer[1024]; /**< 128x64 display frame buffer (8 pages x 128 bytes) */
} ssd1306_t;

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @brief Initializes the display hardware controller over I2C.
	 *
	 * Executes the required initialization sequence to set panel resolution,
	 * charge pump state, orientation, and contrast levels.
	 *
	 * @param disp Pointer to ssd1306_t context structure to populate.
	 * @param i2c Ref to configured Pico hardware I2C instance (i2c0 or i2c1).
	 */
	void ssd1306_init(ssd1306_t* disp, i2c_inst_t* i2c);

	/**
	 * @brief Clears the 1,024-byte RAM buffer (does not flush to physical screen).
	 * @param disp Pointer to display context.
	 */
	void ssd1306_clear(ssd1306_t* disp);

	/**
	 * @brief Sends a single control command byte to the SSD1306 controller via I2C.
	 *
	 * @param disp Pointer to the initialized display context structure.
	 * @param cmd The command byte to transmit.
	 */
	static void ssd1306_cmd(ssd1306_t* disp, uint8_t cmd);

	/**
	 * @brief Draws individual pixels directly into the display RAM frame buffer.
	 *
	 * @param disp Pointer to display context.
	 * @param x Pixel X coordinate (0 to 127).
	 * @param y Pixel Y coordinate (0 to 63).
	 * @param turn_on true to set pixel high (white), false to clear pixel (black).
	 */
	static inline void ssd1306_draw_pixel(ssd1306_t* disp, int x, int y,
		bool turn_on);

	/* Wrapper functions to ssd1306_draw_string_scaled */
	void ssd1306_draw_string(ssd1306_t* disp, int x, int y, const char* str);
	void ssd1306_draw_string_double(ssd1306_t* disp, int x, int y,
		const char* str);

	/**
	 * @brief Draws ASCII text using full 5x7 font with selectable size scaling.
	 *
	 * @param disp Pointer to display context.
	 * @param x Horizontal pixel position (0 to 127).
	 * @param y Vertical pixel position (0 to 63).
	 * @param str Null-terminated string to draw.
	 * @param scale Scaling factor (1 = standard 5x7 glyphs, 2 = double-size 10x14).
	 */
	void ssd1306_draw_string_scaled(ssd1306_t* disp, int x, int y,
		const char* str, uint8_t scale);

	/**
	 * @brief Transmits frame buffer over I2C to render onto physical panel.
	 *
	 * @param disp Pointer to display context.
	 */
	void ssd1306_show(ssd1306_t* disp);

#ifdef __cplusplus
}
#endif

#endif // __SSD1306_MINI_H__
