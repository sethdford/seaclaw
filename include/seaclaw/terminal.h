/* Terminal capability detection and color output */
#ifndef SC_TERMINAL_H
#define SC_TERMINAL_H

#include "design_tokens.h"

typedef enum {
    SC_COLOR_LEVEL_NONE = 0,
    SC_COLOR_LEVEL_BASIC = 1,   /* 16 colors */
    SC_COLOR_LEVEL_256 = 2,     /* 256 colors */
    SC_COLOR_LEVEL_TRUECOLOR = 3 /* 24-bit RGB */
} sc_color_level_t;

typedef enum {
    SC_THEME_DARK = 0,
    SC_THEME_LIGHT = 1
} sc_theme_t;

/**
 * Detect terminal color capabilities from environment.
 * Checks NO_COLOR, COLORTERM, TERM, TERM_PROGRAM.
 * Result is cached after first call.
 */
sc_color_level_t sc_terminal_color_level(void);

/**
 * Detect whether the terminal has a light or dark background.
 * Checks COLORFGBG environment variable.
 * Falls back to SC_THEME_DARK.
 */
sc_theme_t sc_terminal_theme(void);

/**
 * Return the appropriate foreground escape for a hex color
 * at the current terminal's capability level.
 * buf must be at least 24 bytes. Returns buf.
 */
const char *sc_color_fg(char *buf, unsigned int r, unsigned int g, unsigned int b);

/**
 * Return the appropriate background escape for a hex color
 * at the current terminal's capability level.
 * buf must be at least 24 bytes. Returns buf.
 */
const char *sc_color_bg(char *buf, unsigned int r, unsigned int g, unsigned int b);

#endif /* SC_TERMINAL_H */
