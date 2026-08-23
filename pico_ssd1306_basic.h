/**
 * @file ssd1306_mini.h
 * @brief Minimal SSD1306 SSD1306 OLED library for Pico C/C++ SDK.
 *
 * @author Machina Speculatrix https://medium.com/machina-speculatrix
 *
 * This is just an interim version of this library. For the definitive
 * version see: https://github.com/mspeculatrix/pico_oled
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

#define DISP_WIDTH 128
#define DISP_HEIGHT 64
#define LINE_HEIGHT 8
#define NUM_LINES 8

#define CHAR_WIDTH 5 	// Width in pixels of characters (normal size)
#define CHAR_HEIGHT 7 	// Height in pixels of characters (normal size)

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
	 * @brief Clear a line's worth of pixel from buffer
	 * @param disp Pointer to the initialized display context structure.
	 * @param line Line number (0-7)
	 */
	void ssd1306_clearln(ssd1306_t* disp, int line);

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

	/**
	 * @brief Print a line of text starting at X position 0
	 * @param disp Pointer to display context.
	 * @param line Line number (0-7)
	 * @param str Null-terminated string to draw.
	 */
	void ssd1306_println(ssd1306_t* disp, uint8_t line, const char* str);

	/* Wrapper functions to ssd1306_printstr_scaled */
	void ssd1306_printstr(ssd1306_t* disp, int x, int y, const char* str);
	void ssd1306_printstr_double(ssd1306_t* disp, int x, int y,
		const char* str);

	/**
	 * @brief Prints text using 5x7 font with selectable size scaling.
	 *
	 * @param disp Pointer to display context.
	 * @param x Horizontal pixel position (0 to 127).
	 * @param y Vertical pixel position (0 to 63).
	 * @param str Null-terminated string to print.
	 * @param scale Scaling factor (1 = standard 5x7 glyphs, 2 = double-size 10x14).
	 */
	void ssd1306_printstr_scaled(ssd1306_t* disp, int x, int y,
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
