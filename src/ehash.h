#ifndef EHASH_H
#define EHASH_H

#include <stdio.h>		/* for type FILE* */
struct ehash
{
	struct bucket_entry *directory;
	unsigned int global_depth;
	struct bucket *bucket;	/* current bucket we're procesing */

	struct freemap *freemap;
	struct cache *cache;
};
struct ehash* ehash_new();
void ehash_free(struct ehash *h);
int ehash_alloc(struct ehash *h);
char* ehash_find(struct ehash *h,unsigned long term,unsigned int *len);
int ehash_insert(struct ehash *h,unsigned long term,char *invertedlist,unsigned short len);
int ehash_dump(struct ehash *h,FILE *f);
struct ehash* ehash_load(FILE *f);

#define FILL_FACTOR 0.8

#endif
