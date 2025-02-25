#include "stdafx.h"

#include "lcd.h"

/* 
 * Differing interpretations of contrast require that
 * each model has its own base contrast level. 
 */
#define BASE_LEVEL_83P		24
#define BASE_LEVEL_82		30
#define TRUCOLOR(color, bits) ((color) * (0xFF / ((1 << (bits)) - 1)))

/* 
 * Column and Row Driver opcodes, opcode masks, and data masks
 */
typedef enum _CRD_COMMAND {
	// Display ON(1)/OFF(0)
	CRD_DPE 	= 0x02, CRD_DPE_MASK 	= 0xFE, CRD_DPE_DATA	= 0x01,
	// Word length: 8 bits(1)/6 bits(0)
	CRD_86E 	= 0x00, CRD_86E_MASK 	= 0xFE, CRD_86E_DATA	= 0x01,
	// Counter Select : bit1 Y(1)/X(0)  Mode Select bit0 UP(1)/DOWN(0)
	CRD_UDE 	= 0x04, CRD_UDE_MASK 	= 0xFC, CRD_UDE_DATA	= 0x03,
	// Test Mode Select
	CRD_CHE 	= 0x18, CRD_CHE_MASK 	= 0xF8, CRD_CHE_DATA	= 0x00,
	// Op-amp Power Control 1
	CRD_OPA1	= 0x10, CRD_OPA1_MASK	= 0xF8, CRD_OPA1_DATA	= 0x03,
	// Op-amp Power Control 2
	CRD_OPA2	= 0x08, CRD_OPA2_MASK	= 0xF8, CRD_OPA2_DATA	= 0x03,
	// Y-(Page) Address Set
	CRD_SYE		= 0x20, CRD_SYE_MASK	= 0xE0, CRD_SYE_DATA	= 0x1F,
	// Z-Address Set
	CRD_SZE		= 0x40, CRD_SZE_MASK	= 0xC0, CRD_SZE_DATA	= 0x3F,
	// X-Address Set
	CRD_SXE		= 0x80, CRD_SXE_MASK	= 0xC0, CRD_SXE_DATA	= 0x3F,
	// Contrast Set
	CRD_SCE		= 0xC0, CRD_SCE_MASK	= 0xC0, CRD_SCE_DATA	= 0x3F
} CRD_COMMAND;

/* 
 * This is a clever set of macros to simulate a switch-case
 * statement for Column and Row driver constants.
 */
#define TOKCAT(x,y) x##y
#define TOKCAT2(x,y) TOKCAT(x,y)
#define LINECAT(x) TOKCAT2(x, __LINE__ )	
#define CRD_SWITCH(zz)\
	int CRD_TEST = (zz); if (0)

#define CRD_DATA(zz) (CRD_TEST & CRD_##zz##_DATA)
#define CRD_CASE(zz) goto LINECAT(A);}\
while ((CRD_TEST & (CRD_##zz##_MASK))==CRD_##zz) {LINECAT(A)

/*
 * Macro to calculate the offset into LCD memory given x, y, and z
 */
#define LCD_OFFSET(x, y, z) (((((y)+(z))%LCD_HEIGHT)*LCD_MEM_WIDTH)+((x)%LCD_MEM_WIDTH))

/*
 * Prototypes
 */
static void LCD_advance_cursor(LCD_t *);
static void LCD_enqueue(CPU_t *cpu, LCD_t *lcd);
static void LCD_free(CPU_t *);
static void LCD_reset(CPU_t *);
unsigned char *LCD_update_image(LCD_t *lcd);
unsigned char* LCD_image(LCDBase_t *lcdBase);
static void LCD_command(CPU_t *cpu, device_t *dev);
static void LCD_data(CPU_t *cpu, device_t *dev);

#define NORMAL_DELAY 60		//tstates
#define MICROSECONDS(xx) (((cpu->timer_c->freq * 10 / MHZ_6) * NORMAL_DELAY) / 10) + (xx - NORMAL_DELAY)

void set_model_baselevel(LCD_t *lcd, int model) {
	switch (model) {
	case TI_82:
		lcd->base_level = BASE_LEVEL_82;
		break;
	case TI_83:
	case TI_73:
	case TI_83P:
	case TI_83PSE:
	case TI_84P:
	case TI_84PSE:
		lcd->base_level = BASE_LEVEL_83P;
		break;
		//v2 of the 81 will come in as an 82/83
	case TI_81:
	case TI_85:
	case TI_86:
	default:
		lcd->base_level = 0;
		break;
	}
}

/* 
 * Initialize LCD for a given CPU
 */
LCD_t* LCD_init(CPU_t* cpu, int model) {	
	LCD_t* lcd = (LCD_t *) malloc(sizeof(LCD_t));
	if (!lcd) {
		printf("Couldn't allocate memory for LCD\n");
		exit(1);
	}

	lcd->base.free = &LCD_free;
	lcd->base.reset = &LCD_reset;
	lcd->base.command = (devp) &LCD_command;
	lcd->base.data = (devp) &LCD_data;
	lcd->base.image = &LCD_image;
	lcd->base.bytes_per_pixel = 1;
	
	set_model_baselevel(lcd, model);

	lcd->base.height = 64;
	lcd->base.width = 128;
	if (model == TI_86 || model == TI_85) {
		lcd->base.display_width = 128;
	} else {
		lcd->base.display_width = 96;
	}
	
	// Set all values to the defaults
	lcd->shades = LCD_DEFAULT_SHADES;
	lcd->mode = MODE_PERFECT_GRAY;
	lcd->steady_frame = 1.0 / FPS;
	lcd->lcd_delay = NORMAL_DELAY;

	if (lcd->shades > LCD_MAX_SHADES) {
		lcd->shades = LCD_MAX_SHADES;
	} else if (lcd->shades == 0) {
		lcd->shades = LCD_DEFAULT_SHADES;
	}

	lcd->base.time = cpu->timer_c->elapsed;
	lcd->base.ufps_last = cpu->timer_c->elapsed;
	lcd->base.ufps = 0.0f;
	lcd->base.lastgifframe = cpu->timer_c->elapsed;
	lcd->base.lastaviframe = cpu->timer_c->elapsed;
	lcd->base.write_avg = 0.0f;
	lcd->base.write_last = cpu->timer_c->elapsed;
	return lcd;
}

/* 
 * Simulates the state of the LCD after a power reset
 */
static void LCD_reset(CPU_t *cpu) {
	LCD_t *lcd = (LCD_t *) cpu->pio.lcd;
	lcd->base.active = FALSE;
	lcd->word_len = 8;
	lcd->base.cursor_mode = Y_UP;
	lcd->base.x = 0;
	lcd->base.y = 0;
	lcd->base.z = 0;
	lcd->base.contrast = 32;
	lcd->last_read = 0;
	
	memset(lcd->display, 0, DISPLAY_SIZE);
	
	lcd->front = 0;
	memset(lcd->queue, 0, sizeof(lcd->queue[0]) * LCD_MAX_SHADES);
}

/* 
 * Free space belonging to lcd
 */
static void LCD_free(CPU_t *cpu) {
	free(cpu->pio.lcd);
}

/* 
 * Device code for LCD commands 
 */
static void LCD_command(CPU_t *cpu, device_t *dev) {
	LCD_t *lcd = (LCD_t *) dev->aux;

	if (cpu->pio.model >= TI_83P && lcd->lcd_delay > (cpu->timer_c->tstates - lcd->base.last_tstate)) {
		cpu->output = FALSE;
		if (cpu->input) {
			cpu->input = FALSE;
			// this is set so that the sign flag will be properly 
			// set to indicate an error
			cpu->bus = 0x80;
		}
	}

	if (cpu->output) {
		lcd->base.last_tstate = cpu->timer_c->tstates;
		// Test the bus to determine which command to run

	CRD_SWITCH(cpu->bus) {
		CRD_CASE(DPE):
				lcd->base.active = CRD_DATA(DPE);
				LCD_enqueue(cpu, lcd);
				break;
			CRD_CASE(86E):
				lcd->word_len = CRD_DATA(86E);
				break;
			CRD_CASE(UDE):
				lcd->base.cursor_mode = (LCD_CURSOR_MODE)CRD_DATA(UDE);
				break;
			CRD_CASE(CHE):
			CRD_CASE(OPA1):
			CRD_CASE(OPA2):
				break;
			CRD_CASE(SYE):
				lcd->base.y = CRD_DATA(SYE);
				break;
			CRD_CASE(SZE):
				lcd->base.z = CRD_DATA(SZE);
				LCD_enqueue(cpu, lcd);
				break;
			CRD_CASE(SXE):
				lcd->base.x = CRD_DATA(SXE);
				break;
			CRD_CASE(SCE):
				lcd->base.contrast = CRD_DATA(SCE) - lcd->base_level;
				break;
		}
		cpu->output = FALSE;
	} else if (cpu->input) {
		cpu->bus = (unsigned char)((lcd->word_len << 6) | (lcd->base.active << 5) | lcd->base.cursor_mode);
		cpu->input = FALSE;
	}
}

/* 
 * Output to the LCD data port.
 * Also manage user FPS and grayscale 
 */
static void LCD_data(CPU_t *cpu, device_t *dev) {
	LCD_t *lcd = (LCD_t *) dev->aux;

	//int min_wait = MICROSECONDS(lcd->lcd_delay);
	if (cpu->pio.model >= TI_83P && 
		lcd->lcd_delay > (cpu->timer_c->tstates - lcd->base.last_tstate) &&
		(cpu->input || cpu->output))
	{
		cpu->output = FALSE;
		cpu->input = FALSE;
		return;
	}

	// Get a pointer to the byte referenced by the CRD cursor
	unsigned int shift = 0;
	unsigned char *cursor;
	if (lcd->word_len) {
		int temp = LCD_OFFSET(lcd->base.y, lcd->base.x, 0);
		cursor = &lcd->display[temp];
	} else {
		unsigned int new_y = lcd->base.y * 6;
		shift = 10 - (new_y % 8);
		
		cursor = &lcd->display[LCD_OFFSET(new_y / 8, lcd->base.x, 0)];
	}

	if (cpu->output) {
		// Run some sanity checks on the write vars
		if (lcd->base.write_last > cpu->timer_c->elapsed)
			lcd->base.write_last = cpu->timer_c->elapsed;

		double write_delay = cpu->timer_c->elapsed - lcd->base.write_last;
		if (lcd->base.write_avg == 0.0) lcd->base.write_avg = write_delay;
		lcd->base.write_last = cpu->timer_c->elapsed;
		lcd->base.last_tstate = cpu->timer_c->tstates;
	
		// If there is a delay that is significantly longer than the
		// average write delay, we can assume a frame has just terminated
		// and you can push this complete frame towards generating the
		// final image.
		
		// If you are in steady mode, then this simply serves as a
		// FPS calculator
		if (write_delay < lcd->base.write_avg * 100.0) {
			lcd->base.write_avg = (lcd->base.write_avg * 0.90) + (write_delay * 0.10);
		} else {
			double ufps_length = cpu->timer_c->elapsed - lcd->base.ufps_last;
			lcd->base.ufps = 1.0 / ufps_length;
			lcd->base.ufps_last = cpu->timer_c->elapsed;
			
			if (lcd->mode == MODE_PERFECT_GRAY) {
				LCD_enqueue(cpu, lcd);
				lcd->base.time = cpu->timer_c->elapsed;
			}
		}
		
		if (lcd->mode == MODE_GAME_GRAY) {
			if ((lcd->base.x == 0) && (lcd->base.y == 0)) {
				LCD_enqueue(cpu, lcd);
				lcd->base.time = cpu->timer_c->elapsed;
			}
		}

		// Set the cursor based on the word mode
		if (lcd->word_len) {
			cursor[0] = cpu->bus;
		} else {
			unsigned short	data = cpu->bus << shift,
					mask = ~(0x003F << shift);

			cursor[0] = (cursor[0] & (mask >> 8)) | (data >> 8);
			cursor[1] = (cursor[1] & (mask & 0xFF)) | (data & 0xFF);	
		}
		
		LCD_advance_cursor(lcd);
		cpu->output = FALSE;
	} else if (cpu->input) {
		cpu->bus = (unsigned char)(lcd->last_read);

		if (lcd->word_len) {
			lcd->last_read = cursor[0];
		} else {
			lcd->last_read = (cursor[0] << 8) | cursor[1];
			lcd->last_read >>= shift;
			lcd->last_read &= 0x3F;
		}
		
		LCD_advance_cursor(lcd);
		cpu->input = FALSE;
	}
	
	// Make sure timers are valid
	if (lcd->base.time > cpu->timer_c->elapsed)
		lcd->base.time = cpu->timer_c->elapsed;
	
	else if (cpu->timer_c->elapsed - lcd->base.time > (2.0 / STEADY_FREQ_MIN))
		lcd->base.time = cpu->timer_c->elapsed - (2.0 / STEADY_FREQ_MIN);
	
	// Perfect gray mode should time out too in case the screen update rate is too slow for
	// proper grayscale (essentially a fallback on steady freq)
	if (lcd->mode == MODE_PERFECT_GRAY || lcd->mode == MODE_GAME_GRAY) {	
		if ((cpu->timer_c->elapsed - lcd->base.time) >= (1.0 / STEADY_FREQ_MIN)) {
			LCD_enqueue(cpu, lcd);
			lcd->base.time += (1.0 / STEADY_FREQ_MIN);
		}
	} else if (lcd->mode == MODE_STEADY) {
		if ((cpu->timer_c->elapsed - lcd->base.time) >= lcd->steady_frame) {
			LCD_enqueue(cpu, lcd);
			lcd->base.time += lcd->steady_frame;
		}
	}
}

/* 
 * Moves the CRD cursor to the next position, according
 * to the increment/decrement mode set by lcd->cursor_mode
 */
static void LCD_advance_cursor(LCD_t *lcd) {
	switch (lcd->base.cursor_mode) {
		case X_UP:
			lcd->base.x = (lcd->base.x + 1) % LCD_HEIGHT;
			break;
		case X_DOWN:
			lcd->base.x = (lcd->base.x - 1) % LCD_HEIGHT;
			break;
		case Y_UP:
			{
					 lcd->base.y++;
				unsigned int bound = lcd->word_len ? 15 : 19;
				if (((unsigned int)lcd->base.y) >= bound) lcd->base.y = 0;
				break;
			}
		case Y_DOWN:
			if (lcd->base.y <= 0) {
				lcd->base.y = lcd->word_len ? 14 : 18;
			} else
				lcd->base.y--;
			break;
		case MODE_NONE:
			break;
	}
}

/* 
 * Add a black and white LCD image to the LCD grayscale queue
 */
static void LCD_enqueue(CPU_t *cpu, LCD_t *lcd) {

	if (lcd->front == 0) lcd->front = lcd->shades;
	lcd->front--;
	
	for (int i = 0; i < LCD_HEIGHT; i++)
		for (int j = 0; j < LCD_MEM_WIDTH; j++)
			lcd->queue[lcd->front][LCD_OFFSET(j, i, LCD_HEIGHT - lcd->base.z)] = lcd->display[LCD_OFFSET(j, i, 0)];

	if (cpu->lcd_enqueue_callback != NULL) {
		cpu->lcd_enqueue_callback(cpu);
	}
	//7/8/11 BuckeyeDude: does not work with z-addressing properly
	//we now copy to display assuming z offset is 0 always here 
	//when we enqueue the lcd->z property is taken into account
	//this means if you are using lcd->display directly which you shouldn't
	//be then you will no z-addressing
	//memcpy(lcd->queue[lcd->front], lcd->display, DISPLAY_SIZE);
}


/*
 * Clear the LCD's display, including all grayscale buffers
 */
void LCD_clear(LCD_t *lcd) {
	int i;
	for (i = 0; i < LCD_MAX_SHADES; i++) 
		memset(lcd->queue[i], 0x00, DISPLAY_SIZE);
}

//Neil - reuse screen instead of calling malloc every frame
unsigned char* screen = NULL;
bool screen_allocated = false;

unsigned char *LCD_update_image(LCD_t *lcd) {
	if (!screen_allocated)
	{
		screen = (unsigned char*)malloc(GRAY_DISPLAY_SIZE);
		ZeroMemory(screen, GRAY_DISPLAY_SIZE);
		screen_allocated = true;
	}
	

	int bits = 0;
	int n = lcd->shades;
	while (n > 0) {
		bits++;
		n >>= 1;
	}

	int alpha;
	int contrast_color = 0xFF;
	if (lcd->base.contrast < LCD_MID_CONTRAST) {
		alpha = 98 - ((lcd->base.contrast % LCD_MID_CONTRAST) * 100 / LCD_MID_CONTRAST);
		contrast_color = 0x00;
	} else {
		alpha = (lcd->base.contrast % LCD_MID_CONTRAST);
		alpha = alpha * alpha / 3;
		if (alpha > 100) {
			alpha = 100;
		}
	}

	int alpha_overlay = (alpha * contrast_color / 100);
	int inverse_alpha = 100 - alpha;

	unsigned int row, col;
	for (row = 0; row < LCD_HEIGHT; row++) {
		for (col = 0; col < LCD_MEM_WIDTH; col++) {
			unsigned int p0 = 0, p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0, p7 = 0;
			unsigned int i;
			
			for (i = 0; i < lcd->shades; i++) {
				unsigned int u = lcd->queue[i][row * LCD_MEM_WIDTH + col];
				p7 += u & 1; u >>= 1;
				p6 += u & 1; u >>= 1;
				p5 += u & 1; u >>= 1;
				p4 += u & 1; u >>= 1;
				p3 += u & 1; u >>= 1;
				p2 += u & 1; u >>= 1;
				p1 += u & 1; u >>= 1;
				p0 += u;
			}
			
			unsigned char *scol = &screen[row * LCD_WIDTH + col * 8];
			scol[0] = (unsigned char)(alpha_overlay + TRUCOLOR(p0, bits) * inverse_alpha / 100);
			scol[1] = (unsigned char)(alpha_overlay + TRUCOLOR(p1, bits) * inverse_alpha / 100);
			scol[2] = (unsigned char)(alpha_overlay + TRUCOLOR(p2, bits) * inverse_alpha / 100);
			scol[3] = (unsigned char)(alpha_overlay + TRUCOLOR(p3, bits) * inverse_alpha / 100);
			scol[4] = (unsigned char)(alpha_overlay + TRUCOLOR(p4, bits) * inverse_alpha / 100);
			scol[5] = (unsigned char)(alpha_overlay + TRUCOLOR(p5, bits) * inverse_alpha / 100);
			scol[6] = (unsigned char)(alpha_overlay + TRUCOLOR(p6, bits) * inverse_alpha / 100);
			scol[7] = (unsigned char)(alpha_overlay + TRUCOLOR(p7, bits) * inverse_alpha / 100);
		}
	}

	return screen;
}

/* 
 * Generate a grayscale image from the black and white images
 * pushed to the queue.  If there are no images in the queue,
 * the generated image will be blank
 */
unsigned char* LCD_image(LCDBase_t *lcdBase) {
	LCD_t *lcd = (LCD_t *)lcdBase;
	if (lcdBase->active == FALSE) {
		unsigned char *screen = (unsigned char *)malloc(GRAY_DISPLAY_SIZE);
		ZeroMemory(screen, GRAY_DISPLAY_SIZE);
		return screen;
	}

	return LCD_update_image(lcd);
}