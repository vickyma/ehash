#include "hash.h"
#include "ehash.h"
#include "cache.h"
#include "freemap.h"
#include "bucket.h"

#include <assert.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

struct bucket_entry
{
	int fd;
	off_t offset;
};
struct ehash* ehash_new()
{
	struct ehash *h;
	h = malloc(sizeof(struct ehash));
	if(h == NULL)
		goto exit;
	h->global_depth = 1;
	h->directory = NULL;
	h->cache = NULL;
	h->freemap = NULL;
	h->bucket = NULL;
	return h;
exit:
	return NULL;
}
/* ehash_free just gurantee no memory leak .
 * dump data to disk isn't it's duty */
void ehash_free(struct ehash *h) 
{
	assert(h);
	h->bucket = NULL;
	if(h->cache)
		cache_free(h->cache);
	if(h->freemap)
		freemap_free(h->freemap);
	if(h->directory)
		free(h->directory);
	if(h->bucket)
		free(h->bucket);
	h->directory = NULL;
	free(h);
	h = NULL;
}
int ehash_alloc(struct ehash *h)
{
	unsigned int pagesize;
	int fd;
	off_t offset;
	struct page *page;


	h->directory = (struct bucket_entry*)malloc(2 * sizeof(struct bucket_entry));
	if(h->directory == NULL)
		goto exit;
	h->freemap = freemap_new(); /* init freemap */
	if(h->freemap == NULL)
		goto free_directory;
	if(freemap_add(h->freemap,NULL,FREEMAP_CREAT) != 0)
		goto free_freemap;
	pagesize = PAGE_SIZE;
	if(freemap_malloc(h->freemap,&pagesize,&fd,&offset) != 0)
		goto free_freemap;

	h->cache = cache_new();	/* init cache */
	if(h->cache == NULL)	
		goto free_freemap;
	page = cache_newpage(h->cache,fd,offset); /* FIXME:not really need to pagein it now */
	if(page == NULL)
		goto free_cache;

	h->directory[0].fd = fd;
	h->directory[0].offset = offset;
	h->directory[1].fd = fd;
	h->directory[1].offset = offset;

	h->bucket = malloc(sizeof(struct bucket));
	if(h->bucket == NULL)
		goto free_cache;
	bucket_init(h->bucket,page,0);
/*
  h->bucket = bucket_new(page);
  if(h->bucket == NULL)
  goto free_cache;
  bucket_alloc(h->bucket,0);
*/
	return 0;
free_cache:
	cache_free(h->cache);
free_freemap:
	freemap_free(h->freemap);
free_directory:
	free(h->directory);
exit:
	return -1;
}

static int ehash_expand(struct ehash *h)
{
	unsigned int i;
	unsigned int buddy;
	struct bucket_entry *tmp;
	unsigned int num;

	num = 1<<(1+h->global_depth);
	tmp = realloc(h->directory,num*sizeof(struct bucket_entry));
	if(tmp == NULL)
	{
		return -1;
	}
	h->directory = tmp;
	for(i=(1<<h->global_depth); i<(1<<(1+h->global_depth)); i++)
	{
		buddy = (i & ((1<<h->global_depth) -1));
		h->directory[i] = h->directory[buddy];
	}
	h->global_depth++;
	return 0;
}

char *ehash_find(struct ehash *h,unsigned long c,unsigned int *len)
{
	unsigned int entry;
	struct page *page;
	unsigned int hashval;
	int fd;
	off_t offset;
	struct bucket bkt;

	//hashval = hash(c);
	hashval = c & 0x7fffffff;
	entry = hashval & ((1 << h->global_depth)-1);
	fd = h->directory[entry].fd;
	offset = h->directory[entry].offset;

	page = cache_pagein(h->cache, fd,offset);

/*	assert(h->bucket);
	bucket_load(h->bucket,page);
*/
	bucket_load(&bkt,page); /* special process for thread-safe */
	*h->bucket = bkt;
	return bucket_find(&bkt,c,len);
}

/* NOTE:this function will remove the previous invertedlist before insert the new one */
int ehash_insert(struct ehash* h,unsigned long c,char *invertedlist,unsigned short invertedlen)
{
	unsigned int entry;
	unsigned int hashval;
	struct page *page;
	unsigned int size;
	int ret;
	int fd;
	off_t offset;
	struct bucket bkt;
	unsigned int local_mask;
	int i;
	char buf[sizeof(int)+sizeof(off_t)];

	//hashval = hash(c);
	hashval = c & 0x7fffffff;
	entry = hashval & ((1 << (h->global_depth)) - 1);
	if(h->bucket->page->fd!=h->directory[entry].fd || h->bucket->page->offset!=h->directory[entry].offset)
	{
		page = cache_pagein(h->cache,h->directory[entry].fd,h->directory[entry].offset); 
		assert(h->bucket);
		bucket_load(h->bucket,page);
	}
	bucket_remove(h->bucket,c);

	/* preprocess for invertedlist,if a record is too large,store <fd,offset>
	   which point to a new page,instead of the data itself */
	if(invertedlen!=USHRT_MAX && invertedlen>FILL_FACTOR*PAGE_SIZE) 
	{
		size = PAGE_SIZE;
		if(freemap_malloc(h->freemap,&size,&fd,&offset) != 0)
			goto exit;
		page = cache_newpage(h->cache,fd,offset);
		if(page == NULL)
			goto free_freemap;
		bucket_init(&bkt,page,0);
		memcpy(bkt.record,invertedlist,invertedlen);
		bkt.head->num = invertedlen;
		bkt.head->ptr = invertedlen;
		*((int*)buf) = fd;
		*((off_t*)(buf+sizeof(int))) = offset;
		ret = bucket_insert(h->bucket,c,buf,USHRT_MAX);
	}
	else
		ret = bucket_insert(h->bucket,c,invertedlist,invertedlen);

	if(ret == 0)		/* success */
		return 0;	
	else if(ret == -1)	/* a error occured */
		return -1;
	else			/* bucket is full,need splite */
	{
		if(h->bucket->head->local_depth < h->global_depth) /* splite bucket */
		{
			size = PAGE_SIZE;
			if(freemap_malloc(h->freemap,&size,&fd,&offset) != 0)
				goto exit;
			page = cache_newpage(h->cache,fd,offset);
			if(page == NULL)
				goto free_freemap;
			bucket_init(&bkt,page,h->bucket->head->local_depth+1);
			if(bucket_splite(h->bucket,&bkt) != 0)
				goto free_freemap;

			/* repair directory entry */
			local_mask = hashval & ((1<<(bkt.head->local_depth-1))-1);
			for(i=0; i<(1<<(h->global_depth - bkt.head->local_depth)); i++)
			{
				entry = (i<<bkt.head->local_depth)+(1<<(bkt.head->local_depth-1))+local_mask;
				h->directory[entry].fd = bkt.page->fd;
				h->directory[entry].offset = bkt.page->offset;
			}

			/* insert the bucket */
			if((hashval & (1<<(bkt.head->local_depth-1))) != 0)
			{
				ret = bucket_insert(&bkt,c,invertedlist,invertedlen);
				switch(ret)
				{
					case 0:
						*h->bucket = bkt;
						return 0;
					case -2:
						return ehash_insert(h,c,invertedlist,invertedlen); /* fuck! should this happen? */
					case -1:
						return -1;
				}
			}
			else
			{
				ret = bucket_insert(h->bucket,c,invertedlist,invertedlen);
				switch(ret)
				{
					case 0:
						return 0;
					case -2:
						return ehash_insert(h,c,invertedlist,invertedlen);
					case -1:
						return -1;
				}
			}
		}
		else		/* expand directory */
		{
			if(ehash_expand(h) != 0)
				return -1;
			return ehash_insert(h,c,invertedlist,invertedlen);
		}
	}

free_freemap:			/* FIXME:should freemap_free the page */
exit:
	return -1;
}

int ehash_dump(struct ehash *h,FILE *f)
{
	unsigned int num;
	unsigned int i;
	struct cache* cache;
	struct page* page;
	char *buf;
	int len;
	int wlen;

	cache = h->cache;
	num = cache->used;
	for(i=0; i<num; i++)	/* flush cache */
	{
		page = &cache->cache[i];
		if(page->dirty)
		{
			if(lseek(page->fd,page->offset,SEEK_SET) == -1)
				return -1;
			buf = page->mem;
			len = PAGE_SIZE;
			while(len > 0)
			{
				wlen = write(page->fd,buf,len);
				if(wlen == -1)
					return -1;
				buf += wlen;
				len -= wlen;
			}	
		}
	}

	if(fwrite(&h->global_depth,sizeof(unsigned int),1,f) != 1) /* write global_depth */
		return -1;
	num = (1<<h->global_depth);
	if(fwrite(h->directory,sizeof(struct bucket_entry),num,f) != num) /* write directory */
		return -1;

	if(freemap_dump(h->freemap,f) != 0) /* write freemap info */
		return -1;
	fflush(f);
	return 0;
}

struct ehash* ehash_load(FILE *f)
{
	struct ehash *h;
	unsigned int num;
	int i;
	int fd;
	int oldfd;
	unsigned int j;
	struct page* page;

	h = malloc(sizeof(struct ehash));
	if(h == NULL)
		goto exit;
	if(fread(&h->global_depth,sizeof(unsigned int),1,f) != 1) /* load global_depth */
		goto free_idx;

	num = (1<<h->global_depth);	
	h->directory = malloc(num*sizeof(struct bucket_entry));
	if(h->directory == NULL)
		goto free_idx;
	if(fread(h->directory,sizeof(struct bucket_entry),num,f) != num) /* load directory */
		goto free_directory;
	h->freemap = freemap_load(f); /* load freemap */
	if(h->freemap == NULL)
		goto free_directory;

	/* open file by filename to get new fd,and fix fd in freemap and directory */
	for(i=0; i<h->freemap->record_used; i++)
	{
		fd = open(h->freemap->open_file_record[i].filename,O_RDWR);
		if(fd == -1)
			goto free_freemap;
		oldfd = h->freemap->open_file_record[i].fd;
		h->freemap->open_file_record[i].fd = fd; /* update fd in freemap */
		for(j=0; j<h->freemap->used; j++)
		{
			if(h->freemap->array[j].fd == oldfd)
			{
				h->freemap->array[j].fd = fd;
			}
		}
		for(j=0; j<num; j++)			 /* update fd in directory */
		{
			if(h->directory[j].fd == oldfd)
			{
				h->directory[j].fd = fd;
			}
		}
	}

	h->cache = cache_new();	/* init cache */
	if(h->cache == NULL)	
		goto free_freemap;
	page = cache_pagein(h->cache,h->directory[0].fd,h->directory[0].offset); /* FIXME:not really need to pagein it now */
	if(page == NULL)
		goto free_cache;

	h->bucket = bucket_new(page);
	if(h->bucket == NULL)
		goto free_cache;
	bucket_load(h->bucket,page);
	return h;

free_cache:
	cache_free(h->cache);
free_freemap:
	freemap_free(h->freemap);
free_directory:
	free(h->directory);
free_idx:
	free(h);
exit:
	return NULL;
}

#ifdef EHASH_TEST

/* this is written for simple unit test */
#include <stdio.h>
int main()
{
	struct ehash *h;
	unsigned int len;
	char *invertedlist;

	h = ehash_new();
	if(h == NULL)
		return -1;
	if(ehash_alloc(h) != 0)
		ehash_free(h);
	ehash_insert(h,"test","abdefghijklmnopqrstuvwxyz",24);
	ehash_insert(h,"test1","abdefghijklmnopqrstuvwxyz0123456789",34);
	invertedlist = ehash_find(h,"test",&len);
	printf("%s len =%d\n", invertedlist, len);
	ehash_free(h);
	return 0;
}

#endif
