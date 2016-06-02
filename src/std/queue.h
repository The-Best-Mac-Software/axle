#ifndef QUEUE_H
#define QUEUE_H

#include "mutable_array.h"

typedef struct queue_t {
	uint16_t size;
	mutable_array_t array;
} Queue;

//returns first item in queue
type_t queue_head(Queue* queue);
//returns last item in queue
type_t queue_tail(Queue* queue);
//add item to queue
void queue_add(Queue* queue, type_t item);
//pop tail from queue
type_t queue_pop(Queue* queue);
//remove item at index from queue
type_t queue_remove_index(Queue* queue, int index);

#endif
