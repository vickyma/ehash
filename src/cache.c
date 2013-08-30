#include "cache.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define LRU_MAX USHRT_MAX
#define LRU_DEFAULT ((LRU_MAX>>1)+(LRU_MAX>>2))

struct cache *cache_new()
{
	struct cache *ret;
	int i;

	ret = malloc(sizeof(struct cache));
	if(ret == NULL)
		return NULL;
	ret->used = 0;
	ret->lastaccess = -1;
	pthread_mutex_init(&ret->mutex,NULL);
	for(i=0; i<CACHE_PAGE; i++)
	{
		ret->cache[i].mem = NULL;
		pthread_mutex_init(&ret->cache[i].mutex,NULL);
	}
	return ret;
}

void cache_free(struct cache *cache)
{
	unsigned int i;
	struct page *page;
	char *buf;
	int wlen;
	int len = PAGE_SIZE;

	for(i=0; i<cache->used; i++)
	{
		page = &cache->cache[i];
		if(page->dirty)
		{
			buf = page->mem;
			if(lseek(page->fd,page->offset,SEEK_SET) == -1)
				continue;
			while(len && ((wlen = write(page->fd,buf,len))>=0))
			{
				len -= wlen;
				buf += wlen;
			}
		}
		free(page->mem);
	}
	free(cache);
	cache = NULL;
}

/* i assume that <fd,offset> won't be in cache because it's a new one.
 * if the assume is false,what will happen is undefined...maybe a disaster */
struct page* cache_newpage(struct cache *cache,int fd,off_t offset)
{
	unsigned int i,pageout;
	struct page *page;
	int len,wlen;
	char *buf;

	assert(cache);

#ifndef NDEBUG
	for(i=0; i<cache->used; i++)
	{
		if(cache->cache[i].fd==fd && cache->cache[i].offset==offset)
			return NULL;
	}
#endif
	
	if(cache->used < CACHE_PAGE) /* have page slots,so load a page */
	{
		page = &cache->cache[cache->used];
		page->fd = fd;
		page->offset = offset;
		page->dirty = false;
		page->lru_count = LRU_DEFAULT;
		page->mem = malloc(PAGE_SIZE);
		if(page->mem == NULL)
			goto free_page;
		memset(page->mem,0,PAGE_SIZE);

		cache->lastaccess = cache->used;
		cache->used++;
	}
	else			/* page slots is full,we must pageout before pagein */
	{
		pageout = 0;
		for(i=0; i<cache->used; i++)
		{
			if(cache->cache[i].lru_count < cache->cache[pageout].lru_count)
			{
				pageout = i;
			}
		}
		page = &cache->cache[pageout];
		if(page->dirty)
		{
			if(lseek(fd,page->offset,SEEK_SET) == -1)
				goto exit;
			len = PAGE_SIZE;
			buf = page->mem;
			while(len > 0)
			{
				wlen = write(fd,buf,len);
				if(wlen == -1)
					goto exit;
				len -= wlen;
				buf += wlen;
			}
		}
		memset(page->mem,0,PAGE_SIZE);
		page->fd = fd;
		page->offset = offset;
		page->dirty = false;
		page->lru_count = LRU_DEFAULT;

		cache->lastaccess = pageout;
	}
	return page;
free_page:
	free(page->mem);
exit:
	return NULL;
}

/* maybe implement as a doublelist-hash and using a NRU algorithm will imporve permormence
 * i saw linux-0.11 do that,but here i just do it in my own way */
struct page* cache_pagein(struct cache *cache,int fd,off_t offset)
{
	unsigned int i,pageout;
	struct page *page;
	struct page *find;
	int len,wlen;
	char *buf;

	assert(cache);
	find = NULL;

	pthread_mutex_lock(&cache->mutex);
	if(cache->lastaccess!= -1 && cache->cache[cache->lastaccess].fd==fd && cache->cache[cache->lastaccess].offset==offset)
	{
		find = &cache->cache[cache->lastaccess];
	}
	pthread_mutex_unlock(&cache->mutex);

	if(find)
		return find;
	pageout = 0;
	for(i=0; i<cache->used; i++) /* whether <fd,offset> already in cache */
	{
		page = &cache->cache[i];

		pthread_mutex_lock(&page->mutex);
		if(page->lru_count < cache->cache[pageout].lru_count)
		{
			pageout = i;
		}
		if(page->fd == fd && page->offset == offset)
		{
			page->lru_count = LRU_DEFAULT + (LRU_MAX - LRU_DEFAULT) * page->lru_count / LRU_MAX;
			find = page;

			pthread_mutex_lock(&cache->mutex);
			cache->lastaccess = i;
			pthread_mutex_unlock(&cache->mutex);

		}
		else
		{
			if(page->lru_count > 0)
				page->lru_count--;
		}
		pthread_mutex_unlock(&page->mutex);
	}
	if(find)
		return find;
	if(cache->used < CACHE_PAGE) /* have page slots,so load a page */
	{
		pthread_mutex_lock(&cache->mutex);
		page = &cache->cache[cache->used];
		cache->lastaccess = cache->used;
		cache->used++;
		pthread_mutex_unlock(&cache->mutex);

		pthread_mutex_lock(&page->mutex);
		page->fd = fd;
		page->offset = offset;
		page->dirty = false;
		page->lru_count = LRU_DEFAULT;
		page->mem = malloc(PAGE_SIZE);
		if(page->mem == NULL)
			goto exit;
		if(lseek(fd,offset,SEEK_SET) == -1)
			goto exit;
		buf = page->mem;
		len = PAGE_SIZE;
		while(len > 0)
		{
			wlen = read(fd,buf,len);
			if(wlen == -1)
				goto exit;
			len -= wlen;
			buf += wlen;
		}
		pthread_mutex_unlock(&page->mutex);
	}
	else			/* page slots is full,we must pageout before pagein */
	{
		page = &cache->cache[pageout];
		if(page->dirty)
		{
			if(lseek(fd,page->offset,SEEK_SET) == -1)
				goto exit;
			len = PAGE_SIZE;
			buf = page->mem;
			while(len > 0) /* system call "write" may be interruped. return > 0 DOESN'T mean success */
			{	       /* we must use while(write) to ensure it's success. see APUE */
				wlen = write(fd,buf,len);
				if(wlen == -1)
					goto exit;
				len -= wlen;
				buf += wlen;
			}
		}

		pthread_mutex_lock(&page->mutex);
		if(lseek(fd,offset,SEEK_SET) == -1)
			goto exit;

		buf = page->mem;
		len = PAGE_SIZE;
		while(len > 0)
		{
			wlen = read(fd,buf,len);
			if(wlen == -1)
				goto exit;
			len -= wlen;
			buf += wlen;
		}
		page->fd = fd;
		page->offset = offset;
		page->dirty = false;
		page->lru_count = LRU_DEFAULT;
		pthread_mutex_unlock(&page->mutex);

		pthread_mutex_lock(&cache->mutex);
		cache->lastaccess = pageout;
		pthread_mutex_unlock(&cache->mutex);
	}

	return page;
exit:
	return NULL;
}
