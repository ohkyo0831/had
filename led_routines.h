/*
 * Copyright (C) 2008 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __LED_ROUTINES_H__
#define __LED_ROUTINES_H__

#include <inttypes.h>

/** \defgroup 1Basic Basic
 */
/*@{*/

#define COLOR_RED 0
#define COLOR_GREEN 1
#define COLOR_AMBER 2

#define LINE_LENGTH 512
#define LED_MAX_STACK 10

/* holding the content for each different line to be shown */
struct _ledLine
{
	uint16_t *column_red;
	uint16_t *column_green;
	uint16_t *column_red_output;
	uint16_t *column_green_output;
	int x;
	int y;
	int shift_position;
};

/** 
 * Checks if the thread for the led-matrix is running
 *
 * @returns returns 0 on stopped, 1 on running
 */
extern int ledIsRunning(void);
extern void ledMatrixThread(void);
extern void stopLedMatrixThread(void);

/* send data over tcp/ip */
extern void updateDisplay();

extern void allocateLedLine(struct _ledLine *ledLine, int line_length);
extern void freeLedLine(struct _ledLine ledLine);

/** Draw a character
 *
 * @param c character to draw
 * @param color can be COLOR_RED, COLOR_GREEN or COLOR_AMBER
 * @param *ledLine pointer to the struct where the character should be drawn
 * 
 * @return 0 on failure (LINE_LENGTH reached), 1 on success
 */
extern int putChar(char c, uint8_t color, struct _ledLine *ledLine);

extern void putString(char *string, uint8_t color, struct _ledLine *ledLine);

/* clears the screen. doesn't send to display, call updateDisplay() afterwards */
extern void clearScreen(struct _ledLine *ledLine);

/* shifts all columns to the left */
extern int shiftLeft(struct _ledLine *ledLine);

extern void ledPushToStack(char *string, int color, int shift, int lifetime);

/*@}*/

#endif

