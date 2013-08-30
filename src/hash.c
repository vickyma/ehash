#include "hash.h"

static int seed = 73802;

unsigned int jshash(const char *word, int size) 
{
	char c;
	unsigned int h;

	for (h = seed; (c = *word) !='\0'; word++) {
		h ^= ((h << 5) + c + (h >> 2));
	}

	return ((unsigned int) ((h & 0x7fffffff) % size));
}

unsigned int hash(const char *str)
{
	unsigned int hash = 0;
	while(*str)
	{
		hash = (hash<<5) - hash + (*str++);
	}
	return (hash & 0x7fffffff);
}
