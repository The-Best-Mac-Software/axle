#include "snake.h"
#include "kernel.h"
#include "kheap.h"

#define BOARD_SIZE VGA_WIDTH * VGA_HEIGHT

//u16int* screen_buffer[VGA_WIDTH][VGA_HEIGHT] = (u16int*)0x8B000;

void board_clear() {
	for (int y = 0; y < VGA_HEIGHT; y++) {
		for (int x = 0; x < VGA_WIDTH; x++) {
			terminal_putentryat(" ", 68, x, y);
		}
	}
}

void move_up(game_state_t* game_state) {
	snake_player_t* player = game_state->player;

	for (int i = 0; i < player->length; i++) {
		player->body[i].y = player->body[i].y - 1;
		terminal_putentryat("@", 66, player->body[i].x, player->body[i].y);
	}
}

void move_down(game_state_t* game_state) {
	snake_player_t* player = game_state->player;

	for (int i = 0; i < player->length; i++) {
		player->body[i].y = player->body[i].y + 1;
		terminal_putentryat("@", 66, player->body[i].x, player->body[i].y);
	}
}

void move_left(game_state_t* game_state) {
	snake_player_t* player = game_state->player;

	for (int i = 0; i < player->length; i++) {
		player->body[i].x = player->body[i].x - 1;
		terminal_putentryat("@", 66, player->body[i].x, player->body[i].y);
	}
}

void move_right(game_state_t* game_state) {
	printf_info("Moving right");

	snake_player_t* player = game_state->player;

	//for (int i = 0; i < player->length; i++) {
	//	printf_info("Drawing next block %d", i);
		player->head->x = player->head->x + 1;
		terminal_putentryat("@", 66, player->head->x, player->head->y);
	//}
}

void move_dispatch(game_state_t* game_state) {
	switch (game_state->last_move) {
		case 'w':
			move_up(game_state);
			break;
		case 'a':
			move_left(game_state);
			break;
		case 's':
			move_down(game_state);
			break;
		case 'd':
			move_right(game_state);
			break;
	}
}

void game_tick(game_state_t* game_state) {
	static int bufPos;

	if (game_state->player->is_alive != 0) {
		game_state->is_running = 1;

		board_clear();
	
		int y = bufPos / VGA_WIDTH;
		int x = bufPos - (y * VGA_WIDTH);

		move_dispatch(game_state);

		sleep(100); 
		if (haskey()) {
			char ch = getchar();
			if (ch == 'w' || ch == 'a' || ch == 's' || ch == 'd') {
				game_state->last_move = ch;
			}
		}

		bufPos++;
	}
	else {
		//end game
		game_state->is_running = 0;
	}
}

game_state_t* game_setup() {
	game_state_t* game_state = kmalloc(sizeof(game_state_t));
	snake_player_t* player = kmalloc(sizeof(snake_player_t));
	game_state->player->is_alive = 1;
	game_state->last_move = 'd';
	player->length = 1;
	player->head = kmalloc(sizeof(*player->head));
	player->head->x = 10;
	player->head->y = 10;
	player->head->direction = 3;
	player->body = kmalloc(sizeof(*player->body) * 256);
	player->body[0] = *player->head;

	return game_state;
}

void game_teardown(game_state_t* game_state) {
	//free(game_state->player);
	//free(game_state);
	
	printf_info("Thanks for playing!");
}

void play_snake() {
	terminal_clear();

	game_state_t* game_state = game_setup();	
	board_clear();

	//ensure game_tick runs at least once
	do {
		game_tick(game_state);
	} while (game_state->is_running != 0);
	
	game_teardown(game_state);
}
