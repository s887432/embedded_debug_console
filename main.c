/* ----------------------------------------------------------------------------
 *         SAM Software Package License
 * ----------------------------------------------------------------------------
 * Copyright (c) 2016, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

/**
 *  \page lcd LCD Example
 *
 *  \section Purpose
 *
 *  This example demonstrates how to configure the LCD Controller (LCDC)
 *  to use the LCD on the board.
 *
 *  \section Requirements
 *
 *  This package can be used with SAMA5D4x Xplained board.
 *
 *  \section Description
 *
 *  The example configures the LCDC for LCD to display and then draw test
 *  patterns on LCD.
 *
 *  4 layers are displayed:
 *  - Base: The layer at bottom, show test pattern with color blocks.
 *  - OVR1: The layer over base, used as canvas to draw shapes.
 *  - HEO:  The next layer, showed scaled ('F') which flips or rotates once
 *          for a while.
 *
 *  \section Usage
 *
 *  -# Build the program and download it inside the evaluation board. Please
 *     refer to the
 *     <a href="http://www.atmel.com/dyn/resources/prod_documents/6421B.pdf">
 *     SAM-BA User Guide</a>, the
 *     <a href="http://www.atmel.com/dyn/resources/prod_documents/doc6310.pdf">
 *     GNU-Based Software Development</a>
 *     application note or to the
 *     <a href="ftp://ftp.iar.se/WWWfiles/arm/Guides/EWARM_UserGuide.ENU.pdf">
 *     IAR EWARM User Guide</a>,
 *     depending on your chosen solution.
 *  -# On the computer, open and configure a terminal application
 *     (e.g. HyperTerminal on Microsoft Windows) with these settings:
 *    - 115200 bauds
 *    - 8 bits of data
 *    - No parity
 *    - 1 stop bit
 *    - No flow control
 *  -# Start the application.
 *  -# In the terminal window, the
 *     following text should appear (values depend on the board and chip used):
 *     \code
 *      -- LCD Example xxx --
 *      -- SAMxxxxx-xx
 *      -- Compiled: xxx xx xxxx xx:xx:xx --
 *     \endcode
 *  -# Test pattern images should be displayed on the LCD.
 *
 *  \section References
 */
/**
 * \file
 *
 * This file contains all the specific code for the ISI example.
 */

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/
#include "board.h"
#include "chip.h"

#include "display/lcdc.h"
#include "peripherals/pmc.h"
#include "gpio/pio.h"

#include "mm/cache.h"
#include "serial/console.h"
#include "led/led.h"

#include "lcd_draw.h"
#include "lcd_font.h"
#include "lcd_color.h"
#include "font.h"
#include "timer.h"
#include "trace.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*----------------------------------------------------------------------------
 *        Local definitions
 *----------------------------------------------------------------------------*/
/** Size of base image buffer */
#define SIZE_LCD_BUFFER_BASE (BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * 4)
/** Size of Overlay 1 buffer */
#define SIZE_LCD_BUFFER_OVR1 (BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * 3)

/** Background color for OVR1 */
#define OVR1_BG      0xFFFFFF

#define START_POS_X		5
#define START_POS_Y		5
#define LINE_SPACE		5

#define MAX_LINE_CHAR_COUNT			66
#define MAX_FRAME_LINE_COUNT		25
/*----------------------------------------------------------------------------
 *        Local variables
 *----------------------------------------------------------------------------*/

/** LCD BASE buffer */
CACHE_ALIGNED_DDR static uint8_t _base_buffer[BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * 3];

/** Overlay 1 buffer */
CACHE_ALIGNED_DDR static uint8_t _ovr1_buffer[SIZE_LCD_BUFFER_OVR1];

/** Backlight value */
static uint8_t bBackLight = 0xF0;

static uint8_t fontWidth;
static uint8_t fontHeight;
//static int linePos = START_POS_Y;

static struct _pin pio_input = { PIO_GROUP_D, PIO_PD18, PIO_INPUT, PIO_DEFAULT };
static uint8_t gKeyPressed;

#if 0
static char gFrameBuffer[MAX_FRAME_LINE_COUNT][MAX_LINE_CHAR_COUNT+1] = {
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$"},
	{"******************************************************************"},
};
#else
static char gFrameBuffer[MAX_FRAME_LINE_COUNT][MAX_LINE_CHAR_COUNT+1];
#endif

/*----------------------------------------------------------------------------
 *        Functions
 *----------------------------------------------------------------------------*/
static void fill_color(uint8_t *lcd_base)
{
	uint16_t v_max  = BOARD_LCD_HEIGHT;
	uint16_t h_max  = BOARD_LCD_WIDTH;
	uint16_t v, h;
	uint8_t *pix = (uint8_t *)lcd_base;
	
	for (v = 0; v < v_max; ++v) {
		for (h = 0; h < h_max; ++h) {
			*pix++ = (COLOR_BLACK&0x0000FF) >>  0;
			*pix++ = (COLOR_BLACK&0x0000FF) >>  0;
			*pix++ = (COLOR_BLACK&0x0000FF) >>  0;		
		}
	}
}

/**
 * Turn ON LCD, show base .
 */
static void _LcdOn(void)
{
	//test_pattern_24RGB(_base_buffer);
	fill_color(_base_buffer);
	cache_clean_region(_base_buffer, sizeof(_base_buffer));

	lcdc_on();

	lcdc_set_backlight(bBackLight);
	
	/* Display base layer */
	// background
	lcdc_show_base(_base_buffer, 24, 0);

	lcdc_create_canvas(LCDC_OVR1, _ovr1_buffer, 24, 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
	lcd_fill(COLOR_BLACK);
	cache_clean_region(_ovr1_buffer, sizeof(_ovr1_buffer));
	
	lcd_select_font(FONT10x14);
	fontWidth = 10;
	fontHeight = 14;
	
	printf("- LCD ON\r\n");
}

static int frameLineRead = 0;
static int frameLineWrite = 0;
static int lineBufferPos = 0;
static int bufferEmpty = 1;
static int bufferFirstFull = 0;

static void screen_update(void)
{
	int i;
	int y = START_POS_Y;
	int end_line;

	printf("[%d,%d]\n\r", frameLineRead, frameLineWrite);
	if( frameLineWrite == frameLineRead && (bufferEmpty == 0) ) {
		// buffer full
		if( bufferFirstFull == 0 ) {
			bufferFirstFull = 1;
		} else {
			lcd_fill(COLOR_BLACK);
		}
		
		end_line = MAX_FRAME_LINE_COUNT;

	} else {
		end_line = frameLineWrite;
	}
	
	for(i=frameLineRead; i<end_line; i++) {
		lcd_draw_string(START_POS_X, y, gFrameBuffer[i], COLOR_WHITE);
		y += (fontHeight + LINE_SPACE);
	}
	
	if( (frameLineWrite == frameLineRead) ) {
		for(i=0; i<frameLineRead; i++) {
			lcd_draw_string(START_POS_X, y, gFrameBuffer[i], COLOR_WHITE);
			y += (fontHeight + LINE_SPACE);
		}
	}
}

static void line_add(char ch)
{
	if( lineBufferPos < MAX_LINE_CHAR_COUNT ) {
		gFrameBuffer[frameLineWrite][lineBufferPos++] = ch;
	} else {
		// line buffer is full
		gFrameBuffer[frameLineWrite][MAX_LINE_CHAR_COUNT] = 0;
		lineBufferPos = MAX_LINE_CHAR_COUNT;
	}
	
	if( ch == 0 ) {
		// end of line
		lineBufferPos = 0;
		
		if( (frameLineWrite == frameLineRead) && (bufferEmpty == 0) ) {
			frameLineRead++;
			if( frameLineRead >= MAX_FRAME_LINE_COUNT ) {
				frameLineRead = 0;
			}
		}
		
		frameLineWrite++;
		bufferEmpty = 0;
		
		if( frameLineWrite >= MAX_FRAME_LINE_COUNT ) {
			frameLineWrite = 0;			
		}
	}
}

static void line_del(void)
{
	if( lineBufferPos > 0 ) {
		lineBufferPos--;
	}
}

static void screen_clean(void)
{
	lcd_fill(COLOR_BLACK);
	frameLineWrite = 0;
	lineBufferPos = 0;
	frameLineRead = 0;
	bufferEmpty = 1;
	bufferFirstFull = 0;

	
	for(int i=0; i<MAX_FRAME_LINE_COUNT; i++) {
		gFrameBuffer[i][0] = 0;
	}
}

static void dbg_events(void)
{
	uint8_t key;

	if (console_is_rx_ready()){
		key = console_get_char();
		
		if( key >= 0x20 ) {
			line_add(key);
		} else if( key == '\n' ) {
			line_add(0);
			
			screen_update();
		} else if( key == 0x08 ) {
			line_del();
		}
		else {
			printf("[%02X]", key);
		}
	}
}

static void pio_handler(uint32_t group, uint32_t status, void* user_arg)
{
	/* unused */
	(void)group;
	(void)user_arg;

	if (group == pio_input.group && (status & pio_input.mask)) {
		gKeyPressed = 1;
	}
}


/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/

/**
 *  \brief LCD Exmple Application entry point.
 *
 *  \return Unused (ANSI-C compatibility).
 */
extern int main(void)
{
	gKeyPressed = 0;
	
	/* Output example information */
	console_example_info("LCD Example");

	/* Configure PIO for input acquisition */
	pio_configure(&pio_input, 1);
	pio_set_debounce_filter(100);

	/* Initialize pios interrupt with its handlers, see
	 * PIO definition in board.h. */
	pio_add_handler_to_group(pio_input.group, pio_input.mask, pio_handler, NULL);
	
	pio_input.attribute |= PIO_IT_FALL_EDGE;
	pio_enable_it(&pio_input);
	
	/* Configure LCD */
	_LcdOn();

	//screen_update();
	//lcd_fill(COLOR_BLACK);
	
	printf("Width = %d, Height=%d\r\n", BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);

	while(1) {
		dbg_events();
		if( gKeyPressed ) {
			printf("key pressed\n\r");
			gKeyPressed = 0;

			screen_clean();
		}
	}
}
