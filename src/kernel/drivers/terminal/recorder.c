#include "recorder.h"
#include <stdbool.h>
#include <std/std.h>

extern bool is_scroll_redraw;
extern array_m* term_history;

void term_record_char(char ch) {
	if (is_scroll_redraw) return;

	//add this character to line buffer
	/*
	char* current = (char*)array_m_lookup(term_history, term_history->size - 1);
	strccat(current, ch);
	*/
}

void term_record_backspace() {
	if (is_scroll_redraw) return;

	/*
	// remove backspaced character
	// remove space rendered in place of backspaced character
	char* current = (char*)array_m_lookup(term_history, term_history->size - 1);
	current[strlen(current) - 2] = '\0';
	*/
}

void term_record_color(term_cell_color col) {
	if (is_scroll_redraw) return;

	/*
	//append color format to line history
	char* current = (char*)array_m_lookup(term_history, term_history->size - 1);
	strcat(current, "\e[");

	//convert color code to string
	char buf[3];
	itoa(col.fg, buf);
	buf[2] = '\0';

	strcat(current, buf);
	strcat(current, ";");
	*/
}

