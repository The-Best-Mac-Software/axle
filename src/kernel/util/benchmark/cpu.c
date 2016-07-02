#include "cpu.h"
#include <std/timer.h>

#define TEST_LENGTH_MS 10

static bool time_up;

void timer_end() {
	time_up = true;
}

double cpu_speed() {
	time_up = false;
	//install a timer callback for a known time ahead
	add_callback(timer_end, TEST_LENGTH_MS, false, 0);

	//while the timer hasn't completed, iterate
	uint32_t iterations = 0;
	while (!time_up) {
		iterations++;
	}
	
	//TODO implement method to determine how many cycles an iteration takes
	uint64_t cycles_per_iteration = 40000000;
	double speed_mhz = (iterations * cycles_per_iteration) / 1000;
	return speed_mhz;
}
