#include "sound.h"

//start sound from built in speaker
static void sound_start(uint32_t freq) {
	uint32_t divi, tmp;
	
	//set PIT to desired frequency
	divi = 1193180 / freq;
	outb(0x43, 0xB6);
	outb(0x42, (uint8_t)(divi));
	outb(0x42, (uint8_t)(divi >> 8));

	//play sound
	tmp = inb(0x61);
	if (tmp != (tmp | 3)) {
		outb(0x61, tmp | 3);
	}
}

//stop sound
static void sound_stop(void) {
	uint8_t tmp = (inb(0x61) & 0xFC);
	outb(0x61, tmp);
}

//play a sound for ms milliseconds at a given frequency
void sound_play(int freq, int ms) {
	sound_start(freq);
	sleep(ms);
	sound_stop();
}
