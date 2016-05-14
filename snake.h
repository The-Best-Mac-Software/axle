#ifndef SNAKE_H
#define SNAKE_H

#include "common.h"

typedef struct coordinate {
	int x;
	int y;
	int direction;
} coordinate_t;

typedef struct snake_player {
	u8int length;
	int is_alive;
	coordinate_t* head;
	coordinate_t* body;
} snake_player_t;

typedef struct game_state {
	snake_player_t* player;
	char last_move;
	int is_running;
} game_state_t;

void play_snake();

#endif
