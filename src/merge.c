#include "merge.h"
#include "ehash.h"
#include "tmpindex.h"
#include "vector.h"
#include "cache.h"
#include "vbyte.h"
#include "bucket.h"
#include "freemap.h"
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*
 * struct of a invertedlist:
 * | docnum | lastdoc | docid1 | w1 |(docid2-docid1)| w2 | (docid3-docid2) |w3| ... |
 * while "docnum" and"lastdoc" are un-compressed,others are compressed.
 *
 * posting->invertedlist is almost the same as invertedlist:
 * | docid1 | w1 |(docid2-docid1)| w2 | (docid3-docid2) |w3| ... |
 * with all above compressed.
 */

static int merge_invertedlist_page(struct posting *p,struct bucket *bkt, struct cache *cache,struct freemap *freemap)
{
	unsigned int docnum;
	unsigned int lastdoc;
	unsigned int firstdoc;
	int len_firstdoc;
	int fd;
	off_t offset;
	unsigned int size;
	unsigned int invertedlen;
	unsigned int add_size;
	struct vector *v;
	struct bucket tmp;
	struct page *page;
	int page_data_size;
	int available_size;
	char *ptr;

	assert(p);
	v = p->invertedlist;
	len_firstdoc = vbyte_decompress(v->vector,v->vector+v->len,&firstdoc);
	if(len_firstdoc <= 0)
		goto exit;
	docnum = *((unsigned int*)bkt->record);
	lastdoc = *((unsigned int*)((char*)bkt->record+sizeof(unsigned int)));
	assert(firstdoc > lastdoc);
	page_data_size =  PAGE_SIZE - sizeof(struct HEAD);
	invertedlen = bkt->head->num; /* special use for such kind of page,bkt->head->num
				         no longer store the number of records, it stores
				         invertedlen instead. */ 
	add_size = v->len - len_firstdoc;
	bkt->head->num += (v->len-len_firstdoc+vbyte_len(firstdoc-lastdoc));
	*((unsigned int*)bkt->record) += p->count;
	((unsigned int*)bkt->record)[1] = p->lastdoc;

	/* switch to the last page */
	tmp = *bkt;
	while(tmp.head->next.fd != 0)
	{
		fd = tmp.head->next.fd;
		offset = tmp.head->next.offset;
		page = cache_pagein(cache,fd,offset);
		if(page == NULL)
			goto exit;
		bucket_load(&tmp,page);
	}
	/* add invertedlist in the page */
	if((char*)tmp.record+tmp.head->ptr + vbyte_len(firstdoc-lastdoc) < (char*)tmp.head + PAGE_SIZE)
		tmp.head->ptr += vbyte_compress((char*)tmp.record+tmp.head->ptr,(char*)tmp.head+PAGE_SIZE,firstdoc-lastdoc);
	else
	{
		size = PAGE_SIZE;
		if(freemap_malloc(freemap,&size,&fd,&offset) != 0)
			goto exit;
		page = cache_newpage(cache,fd,offset);
		if(page == NULL)
			goto exit;
		bucket_init(&tmp,page,0);
		tmp.head->next.fd = fd;
		tmp.head->next.offset = offset;
		tmp.head->ptr = 0;
		tmp.head->ptr += vbyte_compress((char*)tmp.record+tmp.head->ptr,(char*)tmp.head+PAGE_SIZE,firstdoc-lastdoc);
	}	
	ptr = v->vector+len_firstdoc;
	while(add_size > 0)
	{
		if((char*)tmp.record + tmp.head->ptr + add_size < (char*)tmp.head + PAGE_SIZE)
		{
			memcpy((char*)tmp.record+tmp.head->ptr,ptr,add_size);
			tmp.head->ptr += add_size;
			add_size = 0;
		}
		else
		{
			available_size = PAGE_SIZE-sizeof(struct HEAD)-tmp.head->ptr;
			memcpy((char*)tmp.record+tmp.head->ptr,ptr,available_size);
			ptr += available_size;
			tmp.head->ptr += available_size;
			add_size -= available_size;

			size = PAGE_SIZE;
			if(freemap_malloc(freemap,&size,&fd,&offset) != 0)
				goto exit;
			page = cache_newpage(cache,fd,offset);
			if(page == NULL)
				goto exit;
			tmp.head->next.fd = fd;
			tmp.head->next.offset = offset;
			bucket_init(&tmp,page,0);
			tmp.head->ptr = 0;
		}
	}
	return 0;
exit:
	return -1;
}

/* NOTE:the returned char* is malloced,so remember to free it to avoid memory leak */
static char* merge_invertedlist(struct posting *p,char* invertedlist,unsigned int invertedlen,int *len)
{
	struct vector *v;
	unsigned int docnum;
	unsigned int lastdoc;
	unsigned int firstdoc;
	int len_firstdoc;
	char *ret;
	int pos;

	v = p->invertedlist;
        /* merge p with "" */
	if(invertedlist == NULL) 
	{
		*len = v->len + 2*sizeof(unsigned int);
		ret = malloc(*len);
		if(ret == NULL)
			goto exit;
		*((unsigned int*)ret) = p->count;
		*((unsigned int*)(ret+sizeof(unsigned int))) = p->lastdoc;
		memcpy(ret+2*sizeof(unsigned int),v->vector,v->len);
		return ret;
	}

	docnum = *((int*)invertedlist);
	lastdoc = *((int*)(invertedlist+sizeof(unsigned int)));
	len_firstdoc = vbyte_decompress(v->vector,v->vector+v->len,&firstdoc);
	if(len_firstdoc <= 0)
		goto exit;
	assert(firstdoc > lastdoc);
	*len = invertedlen + v->len - len_firstdoc + vbyte_len(firstdoc-lastdoc);
	ret = malloc(*len);
	if(ret == NULL)
		goto exit;
	memcpy(ret,invertedlist,invertedlen);
	*((unsigned int*)ret) = p->count + docnum;
	*((unsigned int*)(ret+sizeof(unsigned int))) = p->lastdoc;

	pos = invertedlen;
	pos += vbyte_compress(ret+pos,ret+*len,firstdoc-lastdoc);
	if(pos == invertedlen)
		goto free;
	memcpy(ret+pos,v->vector+len_firstdoc,v->len-len_firstdoc);

	return ret;

	/* the first pass,compute the size */
/*	len_docnum = vbyte_decompress(invertedlist,invertedlist+invertedlen,&docnum);
	if(len_docnum == 0)
	goto exit;
	len_lastdoc = vbyte_decompress(invertedlist+len_docnum,invertedlist+invertedlen,&lastdoc);
	if(len_lastdoc == 0)
	goto exit;
	assert(lastdoc < p->lastdoc);

	*len = 0;
	*len += vbyte_len(docnum+p->count);
	*len += vbyte_len(p->lastdoc);
	size = invertedlen-len_docnum-len_lastdoc;
	*len += size;

	tmplen = vbyte_decompress(v->vector,v->vector+v->size,&tmp);
	if(tmplen == 0)
	goto exit;
	assert(tmp > lastdoc);

	*len += vbyte_len(tmp-lastdoc);
	size = v->len-tmplen;
	*len += size;

	ret = malloc(*len);
	if(ret == NULL)
	goto exit;
*/
	/* the second pass,fill the space */
/*	pos = vbyte_compress(ret,ret+*len,docnum+p->count);
	pos += vbyte_compress(ret+pos,ret+*len,p->lastdoc);
	size = invertedlen-len_docnum-len_lastdoc;
	memcpy(ret+pos,invertedlist+len_docnum+len_lastdoc,size);
	pos += size;
	pos += vbyte_compress(ret+pos,ret+*len,tmp-lastdoc);
	size = v->len-tmplen;
	memcpy(ret+pos,v->vector+tmplen,size);
*/
free:
	free(ret);
exit:
	return NULL;
}

int merge(struct tmpindex *tmpidx,struct ehash *idx)
{
	struct posting *p;
	unsigned int len;
	char *invertedlist;
	char *merged;
	int mergedlen;
	struct bucket bkt;
	struct page *page;

	p = tmpidx->list;
	while(p)
	{
		invertedlist = ehash_find(idx,p->c,&len);
		if(invertedlist!=NULL && len==USHRT_MAX)    /* invertedlist is bigger than a page! */
		{
			page = cache_pagein(idx->cache,*((int*)invertedlist),*((off_t*)(invertedlist+sizeof(int))));
			if(page == NULL)
				return -1;
			bucket_load(&bkt,page);
			if(merge_invertedlist_page(p,&bkt,idx->cache,idx->freemap) != 0)
				return -1;
		}
		else
		{
			merged = merge_invertedlist(p,invertedlist,len,&mergedlen);
			if(merged == NULL)
				return -1;
			if(ehash_insert(idx,p->c,merged,mergedlen) != 0)
				return -1;
			free(merged); /* merge_invertedlist's return value is malloced!!! */
		}

		merged = NULL;
		tmpidx->list = p->next;
		posting_free(p);
		tmpidx->count--;
		p = tmpidx->list;
	}
	tmpindex_clear(tmpidx);
	return 0;
}

#ifdef MERGE_TEST
/* this is written for simple unit test */

#include <stdio.h>
int posting_insert(struct posting *p,unsigned int d, unsigned w);
struct posting* posting_alloc(unsigned long c,unsigned int d, unsigned w);

void invertedlist_print(char *c,unsigned int len)
{
	unsigned int val;
	unsigned int lastdoc;
	unsigned int docnum;
	int l;
	int pos;
	int i;
	double w;

	docnum = *((unsigned int*)c);
	lastdoc = *((unsigned int*)(c+sizeof(unsigned int)));
	printf("275 number of docs: %d\n",docnum);
	printf("276 lastdoc: %d\n",lastdoc);

	pos = 0;
	pos += sizeof(unsigned int) *2;
	//l = vbyte_decompress(c,c+len,&docnum);
	//printf("number of docs: %d\n",docnum);
	//pos += l;
	//l = vbyte_decompress(c+pos,c+len,&lastdoc);
	//pos += l;
	//printf("lastdoc: %d\n",lastdoc);

	l = vbyte_decompress(c+pos,c+len,&lastdoc);
	pos += l;
	printf("<%d,%lu>",lastdoc,*(unsigned long*)(c+pos));
	pos += sizeof(unsigned long);

	for(i=1; i<docnum; i++)
	{
		l = vbyte_decompress(c+pos,c+len,&val);
		if(l == 0)
			return;
		pos += l;
		lastdoc += val;
		printf("<%d,%lu>",lastdoc,*(unsigned long*)(c+pos));
		pos += sizeof(unsigned long);
	}
	printf("\n");
}
int main()
{
	struct posting *p;
	struct posting *p2;
	char *ret;
	int retlen;
	char *ret1;
	int ret1len;
	unsigned long tmp = 0;

	tmp = jshash("test", 512);
	p = posting_alloc(tmp,4,10);
	if(p == NULL)
		return -1;
	posting_insert(p,5,20);
	ret = merge_invertedlist(p,NULL,0,&retlen);

	invertedlist_print(ret,retlen);

	p2 = posting_alloc(tmp,7,10);
	posting_insert(p2,8,50);
	ret1 = merge_invertedlist(p2,ret,retlen,&ret1len);

	invertedlist_print(ret1,ret1len);

	//free(p->c);
	vector_free(p->invertedlist);
	free(p);

	//free(p2->c);
	vector_free(p2->invertedlist);
	free(p2);
	free(ret);
	free(ret1);
	return 0;
}

#endif
