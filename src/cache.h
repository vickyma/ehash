#ifndef CACHE_H
#define CACHE_H

#include <unistd.h>		/* for type off_t */
#include <stdbool.h>		/* for type bool */
#include <pthread.h>

#define CACHE_PAGE 1000
#define PAGE_SIZE  4096
struct page 
{
	int fd;			/* file that this page lives at */
	off_t offset;		/* offset that this page lives at */
	unsigned short lru_count;	/* count for clock LRU approx or 
					 * UCHAR_MAX if unremovable */
	bool dirty;			/* whether this page is dirty */
	pthread_mutex_t mutex;		/* protect this page */

	char *mem;
};

struct cache
{
	struct page cache[CACHE_PAGE];
	unsigned int used;	/* how many page used in the cache */
	int lastaccess;
	pthread_mutex_t mutex;
};

struct cache *cache_new();

void cache_free(struct cache *cache);

struct page* cache_newpage(struct cache *cache,int fd,off_t offset);
struct page* cache_pagein(struct cache *cache,int fd,off_t offset);
void cache_freepage(struct cache *cache,int fd,off_t offset); /* FIXME:implement it */

#endif
