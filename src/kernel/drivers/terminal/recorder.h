#ifndef RECORDER_H
#define RECORDER_H

#include "terminal.h"

typedef struct term_cell_color {
	term_color fg;
	term_color bg;
} term_cell_color;

void term_record_char(char ch);
void term_record_backspace();
void term_record_color(term_cell_color col);

#endif
