#include "array_o.h"

#include "std.h"

int8_t standard_lessthan_predicate(type_t a, type_t b) {
	return a < b;
}

array_o* array_o_create(uint32_t max_size, lessthan_predicate_t less_than) {
	array_o* ret = kmalloc(sizeof(array_o));
	ret->array = kmalloc(max_size * sizeof(void*));
	ret->size = 0;
	ret->max_size = max_size;
	ret->less_than = less_than;
	return ret;
}

array_o* array_o_place(void* addr, uint32_t max_size, lessthan_predicate_t less_than) {
	array_o* ret = (array_o*)kmalloc(sizeof(array_o));
	ret->array = addr;
	memset(ret->array, 0, max_size * sizeof(void*));
	ret->size = 0;
	ret->max_size = max_size;
	ret->less_than = less_than;
	return ret;
}

void array_o_destroy(array_o* array) {
	kfree(array);
	kfree(array->array);
}

void array_o_insert(array_o* array, type_t item) {
	ASSERT(array->less_than, "ordered array didn't have a less-than predicate!");
	ASSERT(array->size < array->max_size - 1, "array_o would exceed max_size (%d)", array->max_size);

	uint32_t iterator = 0;
	while (iterator < array->size && array->less_than(array->array[iterator], item)) {
		iterator++;
	}
	//just add item to end of array
	if (iterator == array->size) {
		array->array[array->size++] = item;
	}
	else {
		type_t tmp = array->array[iterator];
		array->array[iterator] = item;
		while (iterator < array->size) {
			iterator++;
			type_t tmp2 = array->array[iterator];
			array->array[iterator] = tmp;
			tmp = tmp2;
		}
		array->size++;
	}
}

type_t array_o_lookup(array_o* array, uint32_t i) {
	ASSERT(i < array->size, "array_o: %d exceeded bounds %d", i, array->size);
	return array->array[i];
}

uint16_t array_o_index(array_o* array, type_t item) {
	for (int i = 0; i < array->size; i++) {
		if (array->array[i] == item) {
			return i;
		}
	}
	//return ARR_NOT_FOUND;
	return -1;
}

void array_o_remove(array_o* array, uint32_t i) {
	while (i < array->size) {
		array->array[i] = array->array[i + 1];
		i++;
	}
	array->size--;
}

