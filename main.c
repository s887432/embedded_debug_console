#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "callback.h"
#include "chip.h"
#include "cpuidle.h"
#include "irq/irq.h"
#include "gpio/pio.h"
#include "mm/cache.h"
#include "mutex.h"
#include "peripherals/pmc.h"
#include "serial/console.h"
#include "serial/usart.h"
#include "serial/usartd.h"

#include "display/lcdc.h"

#include "lcd_draw.h"
#include "lcd_font.h"
#include "lcd_color.h"
#include "font.h"
#include "timer.h"
#include "trace.h"

/*----------------------------------------------------------------------------
 *        Local definitions
 *----------------------------------------------------------------------------*/
 
#define ENABLE_MBUS_UART
#define ENABLE_DISPLAY
#define ENABLE_KEYINPUT

#ifdef ENABLE_MBUS_UART
#define USART_ADDR FLEXUSART5
#define USART_PINS PINS_FLEXCOM5_USART_HS_IOS1
#endif // end of ENABLE_MBUS_UART

#ifdef ENABLE_DISPLAY
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
#endif // end of ENABLE_DISPLAY
/*----------------------------------------------------------------------------
 *        Local variables
 *----------------------------------------------------------------------------*/

#ifdef ENABLE_MBUS_UART
static struct _pin pio_output = { PIO_GROUP_A, PIO_PA29, PIO_OUTPUT_0, PIO_DEFAULT };
static const struct _pin usart_pins[] = USART_PINS;

static const uint8_t test_patten[] = "abcdefghijklmnopqrstuvwxyz0123456789\n\r";

static struct _usart_desc usart_desc = {
	.addr           = USART_ADDR,
	.baudrate       = 115200,
	.mode           = US_MR_CHMODE_NORMAL | US_MR_PAR_NO | US_MR_CHRL_8_BIT,
	.transfer_mode  = USARTD_MODE_POLLING,
	.timeout        = 0, // unit: ms
};
#endif // end of ENABLE_MBUS_UART

#ifdef ENABLE_DISPLAY
/** LCD BASE buffer */
CACHE_ALIGNED_DDR static uint8_t _base_buffer[BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * 3];

/** Overlay 1 buffer */
CACHE_ALIGNED_DDR static uint8_t _ovr1_buffer[SIZE_LCD_BUFFER_OVR1];

/** Backlight value */
static uint8_t bBackLight = 0xF0;

static uint8_t fontWidth;
static uint8_t fontHeight;

static char gFrameBuffer[MAX_FRAME_LINE_COUNT][MAX_LINE_CHAR_COUNT+1];
#endif // end of ENABLE_DISPLAY

#ifdef ENABLE_KEYINPUT
static struct _pin pio_input = { PIO_GROUP_D, PIO_PD18, PIO_INPUT, PIO_DEFAULT };
static uint8_t gKeyPressed;
#endif // end of ENABLE_KEYINPUT
/*----------------------------------------------------------------------------
 *        Functions
 *----------------------------------------------------------------------------*/

#ifdef ENABLE_KEYINPUT
static void pio_handler(uint32_t group, uint32_t status, void* user_arg)
{
	/* unused */
	(void)group;
	(void)user_arg;

	if (group == pio_input.group && (status & pio_input.mask)) {
		gKeyPressed = 1;
	}
}
#endif // end of ENABLE_KEYINPUT

#ifdef ENABLE_DISPLAY
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
#endif // end of ENABLE_DISPLAY

#ifdef ENABLE_MBUS_UART
static int _usart_finish_tx_transfer_callback(void* arg, void* arg2)
{
	return 0;
}

static void _usart_write_buffer(uint8_t *buffer, int size)
{
	struct _buffer tx = {
		.data = (unsigned char*)buffer,
		.size = size,
		.attr = USARTD_BUF_ATTR_WRITE,
	};
		
	struct _callback _cb_tx = {
		.method = _usart_finish_tx_transfer_callback,
		.arg = 0,
	};
		
	usartd_transfer(0, &tx, &_cb_tx);
	usartd_wait_tx_transfer(0);
}

static void _usart_irq_handler(uint32_t source, void* user_arg)
{
	if (usart_is_rx_ready(USART_ADDR)) {
		uint8_t key = usart_get_char(USART_ADDR);
		
#ifdef ENABLE_DISPLAY
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
#endif // end of ENABLE_DISPLAY

		printf("%c", key);
	}
}
#endif // end of ENABLE_MBUS_UART


/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/
int main (void)
{
#ifdef ENABLE_DISPLAY
	gKeyPressed = 0;
#endif // end of ENABLE_DISPLAY

	/* Output example information */
	console_example_info("USART Example");

#ifdef ENABLE_MBUS_UART
	// UART pin select
	pio_configure(&pio_output, 1);
	pio_clear(&pio_output);
	
	uint32_t id = get_usart_id_from_addr(USART_ADDR);
	pio_configure(usart_pins, ARRAY_SIZE(usart_pins));
	usartd_configure(0, &usart_desc);
	irq_add_handler(id, _usart_irq_handler, NULL);
	usart_enable_it(USART_ADDR, US_IER_RXRDY);
	irq_enable(id);
	
	_usart_write_buffer((uint8_t *)test_patten, sizeof(test_patten));
#endif // end of ENABLE_MBUS_UART

#ifdef ENABLE_KEYINPUT
	/* Configure PIO for input acquisition */
	pio_configure(&pio_input, 1);
	pio_set_debounce_filter(100);

	/* Initialize pios interrupt with its handlers, see
	 * PIO definition in board.h. */
	pio_add_handler_to_group(pio_input.group, pio_input.mask, pio_handler, NULL);
	
	pio_input.attribute |= PIO_IT_FALL_EDGE;
	pio_enable_it(&pio_input);
#endif // end of ENABLE_DISPLAY

#ifdef ENABLE_DISPLAY	
	/* Configure LCD */
	_LcdOn();

	printf("Width = %d, Height=%d\r\n", BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
#endif // end of ENABLE_DISPLAY

	while (1) {
		cpu_idle();
#ifdef ENABLE_KEYINPUT
		if( gKeyPressed ) {
			printf("key pressed\n\r");
			gKeyPressed = 0;
#ifdef ENABLE_DISPLAY
			screen_clean();
#endif // end of ENABLE_DISPLAY

		}
#endif //  end of ENABLE_KEYINPUT

	}
}
