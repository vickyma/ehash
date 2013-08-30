/* This file is for bucket. The bucket have been carefully designed
 * it's on-disk struct is like this:
 * +-----------------------------------------+
 * |num|local_depth|<fd,offset>|             +  header 
 * +-----------------------------------------+
 * |offset1|termlen1|invertedlen1|           +
 * +-----------------------------------------+  fixed size record
 * |offset2|termlen2|invertedlen2|...        +
 * +-----------------------------------------+
 * |                                         |  free space
 * +-----------------------------------------+
 * |                 |term,xxxxx|term,xxxxxxx|  inverted list
 * +-----------------------------------------+
 *
 * i also use one bit in the record to indicate whether the inverted list
 * is compressed or not.
 * NOTE: the bucket on-disk is not the same as it in-memory.
 * the bucket struct is only a wraper used in memory for on-disk bucket
 * struct.
 * 
 * */

#include "hash.h"
#include "bucket.h"
#include "cache.h"
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define START_OF_INVERTEDLIST ((char*)bkt->head + bkt->head->ptr)

struct bucket *bucket_new(struct page *page)
{
	struct bucket *ret;

	assert(page);
	assert(page->mem);

	ret = malloc(sizeof(struct bucket));
	if(ret == NULL)
		return NULL;
	ret->page = page;
	ret->init = false;
	return ret;
}

void bucket_free(struct bucket *bkt)
{
	free(bkt);
	bkt = NULL;
}

/*
  #define BUCKET_MEM(bucket) (char*((bucket)->page->mem))
  #define BUCKET_NUM(bucket) (*((unsigned short*)BUCKET_MEM(bucket)))
  #define BUCKET_LOCAL_DEPTH(bucket)					\
  (*((unsigned int*)(BUCKET_MEM(bucket)+sizeof(unsigned short))))	
  #define BUCKET_NEXT_FD(bucket)					\
  (*((int*)(BUCKET_MEM(bucket)+sizeof(unsigned short)	\
  +sizeof(unsigned int))))			
  #define BUCKET_NEXT_OFFSET(bucket)				\
  (*((off_t*)(BUCKET_MEM(bucket)+sizeof(unsigned short)	\
  +sizeof(unsigned int)+sizeof(int))))			
  #define BUCKET_HEAD_SIZE (sizeof(unsigned short)			\
  +sizeof(unsigned int)+sizeof(int)+sizeof(off_t))
  #define BUCKET_ENTRY_SIZE (sizeof(unsigned char)+
  #define BUCKET_ENTRY(bucket,i)\


  static void *bucket_mem(struct bucket *bkt)
  {
  return bucket->page->mem;
  }
  static void bucket_set_num(struct bucket *bkt,unsigned short val)
  {
  BUCKET_NUM(bkt) = val;
  }
  static void bucket_set_local_depth(struct bucket *bkt,unsigned int val)
  {
  BUCKET_LOCAL_DEPTH = val;
  }
*/

void bucket_init(struct bucket *bkt,struct page *page,unsigned int local_depth)
{
	bkt->page = page;

	bkt->head = (struct HEAD*)(page->mem);
	bkt->record = (struct RECORD*)((char*)page->mem + sizeof(struct HEAD));

	bkt->head->num = 0;
	bkt->head->local_depth = local_depth;
	bkt->head->next.fd = 0;
	bkt->head->next.offset = 0;
	bkt->head->ptr = PAGE_SIZE;

	bkt->init = true;
	bkt->page->dirty = true;

}
void bucket_alloc(struct bucket *bkt,unsigned int local_depth)
{
	bkt->head = (struct HEAD*)(bkt->page->mem);
	bkt->record = (struct RECORD*)((char*)bkt->page->mem + sizeof(struct HEAD));

	bkt->head->num = 0;
	bkt->head->local_depth = local_depth;
	bkt->head->next.fd = 0;
	bkt->head->next.offset = 0;
	bkt->head->ptr = PAGE_SIZE;

	bkt->init = true;
	bkt->page->dirty = true;
}

void bucket_load(struct bucket *bkt,struct page *page)
{
	bkt->page = page;
	bkt->head = (struct HEAD*)page->mem;
	bkt->record = (struct RECORD*)(page->mem + sizeof(struct HEAD));
	bkt->init = true;
}

/* use binsearch to find term's inverted list.return 0 on success
 * and -1 on error. pos is the return value of binsearch*/
static int _bucket_find(struct bucket *bkt,unsigned long term,unsigned short *pos)
{
	unsigned short i,j,mid;
	//int termlen;
	int big;
	//char string[256];
	unsigned long string;

	*pos = 0;
	i = 0;
	j = bkt->head->num;	/* binsearch */
	if(j == 0)
		return -1;
	while(i < j)
	{
		mid = (i+j)/2;
		//termlen = bkt->record[mid].termlen;
		//memcpy(string,(char*)bkt->head + bkt->record[mid].offset,termlen);
		//string[termlen] = '\0';
		//big = strcmp(term,string);
		string = *(unsigned long *)((char *)bkt->head + bkt->record[mid].offset);
		big = term > string;
		if(big > 0)
			i = mid + 1;
		else if(big < 0)
			j = mid;
		else
			break;
	}
	*pos = mid;
	return (i<j)?0:-1;
}

char* bucket_find(struct bucket *bkt,unsigned long term,unsigned int *invertedlen)
{
	unsigned short pos;
	if(_bucket_find(bkt,term,&pos) != 0)
		return NULL;
	*invertedlen = bkt->record[pos].invertedlen;
	return (char*)bkt->head + bkt->record[pos].offset + bkt->record[pos].termlen;
}


int bucket_remove(struct bucket *bkt,unsigned long term)
{
	unsigned short pos;
	unsigned int len;
	char *posptr;
	int size;
	unsigned short i;
	unsigned short true_invertedlen;

	if(_bucket_find(bkt,term,&pos) != 0)
		return -1;
	/* remove inverted list */
	true_invertedlen = (bkt->record[pos].invertedlen==USHRT_MAX) ? (sizeof(int)+sizeof(off_t)):bkt->record[pos].invertedlen;
	len = bkt->record[pos].termlen + true_invertedlen;
	posptr = (char*)bkt->head+bkt->record[pos].offset;
	size = posptr - START_OF_INVERTEDLIST;
	memmove(START_OF_INVERTEDLIST+len,START_OF_INVERTEDLIST,size);
	bkt->head->ptr += len;

	/* remove entry in record */
	memmove(&bkt->record[pos],&bkt->record[pos+1],(bkt->head->num-pos-1)*sizeof(struct RECORD));
	bkt->head->num--;

	/* update entry's offset in record */
	for(i=0; i<bkt->head->num; i++)
	{
		if((char*)bkt->head+bkt->record[i].offset < posptr)
			bkt->record[i].offset += len;
	}
	bkt->page->dirty = true;
	return 0;
}
/* NOTE:assume the term isn't in bucket,otherwise you must remove it before insert!!! */
int bucket_insert(struct bucket *bkt,unsigned long term,void *invertedlist,unsigned short invertedlen)
{
	unsigned short pos;
	int termlen;
	int big;
	//char string[256];
	unsigned long string = 0;
	unsigned short true_invertedlen;

	true_invertedlen = (invertedlen==USHRT_MAX) ? (sizeof(int)+sizeof(off_t)):invertedlen;
	if(bkt->head->num == 0)
	{
		//termlen = strlen(term);
		termlen = sizeof(unsigned long);
		if((char*)&bkt->record[1] > START_OF_INVERTEDLIST-true_invertedlen-termlen)
			return -2;
		bkt->head->ptr = bkt->head->ptr-true_invertedlen-termlen;
		//memcpy(START_OF_INVERTEDLIST,term,termlen);
		*(unsigned long *)(START_OF_INVERTEDLIST) = term;
		memcpy(START_OF_INVERTEDLIST+termlen,invertedlist,true_invertedlen);

		bkt->record[0].offset = bkt->head->ptr;
		bkt->record[0].termlen = termlen;
		bkt->record[0].invertedlen = invertedlen;

		bkt->head->num++;
		bkt->page->dirty = true;
		return 0;
	}
	if(_bucket_find(bkt,term,&pos) == 0)
		return -1;

	termlen = bkt->record[pos].termlen;
	//memcpy(string,(char*)bkt->head+bkt->record[pos].offset,termlen);
	//string[termlen] = '\0';
	//termlen = strlen(term);
	string = *(unsigned long *)((char*)bkt->head+bkt->record[pos].offset);
	termlen = sizeof(unsigned long);
	if(START_OF_INVERTEDLIST-true_invertedlen-termlen < (char*)&bkt->record[bkt->head->num]+sizeof(struct RECORD)) /* not enough space */
		return -2;

	//big = strcmp(term,string);
	big = term > string;
	if(big < 0)		/* insert before pos */
	{
		/* modify for record */
		memmove(&bkt->record[pos+1],&bkt->record[pos],(bkt->head->num-pos)*sizeof(struct RECORD));
		bkt->record[pos].termlen = termlen;
		bkt->record[pos].invertedlen = invertedlen;
		bkt->record[pos].offset = START_OF_INVERTEDLIST-true_invertedlen-termlen-(char*)bkt->head;

		bkt->head->ptr = bkt->head->ptr-true_invertedlen-termlen;
		//memcpy(START_OF_INVERTEDLIST,term,termlen); /* modify inverted list */
		*(unsigned long *)(START_OF_INVERTEDLIST) = term;
		memcpy(START_OF_INVERTEDLIST+termlen,invertedlist,true_invertedlen);

		bkt->head->num++;
	}
	else			/* insert after pos */
	{
		if(pos+1 < bkt->head->num) /* modify record */
			memmove(&bkt->record[pos+2],&bkt->record[pos+1],(bkt->head->num-pos-1)*sizeof(struct RECORD));
		bkt->record[pos+1].termlen = termlen;
		bkt->record[pos+1].invertedlen = invertedlen;
		bkt->record[pos+1].offset = START_OF_INVERTEDLIST-true_invertedlen-termlen-(char*)bkt->head;

		bkt->head->ptr = bkt->head->ptr-true_invertedlen-termlen;
		//memcpy(START_OF_INVERTEDLIST,term,termlen); /* modify inverted list */
		*(unsigned long *)(START_OF_INVERTEDLIST) = term;
		memcpy(START_OF_INVERTEDLIST+termlen,invertedlist,true_invertedlen);
		bkt->head->num++;
	}
	bkt->page->dirty = true;
	assert(START_OF_INVERTEDLIST >= (char*)&bkt->record[bkt->head->num]);
	return 0;
}

int bucket_splite(struct bucket *old,struct bucket *new)
{
	int i;
	int termlen;
	//char term[256];
	unsigned long term;
#ifndef NDEBUG
	int first = 0;
	unsigned int mask;
#endif

	i = 0;
	while(i < old->head->num)
	{
		termlen = old->record[i].termlen;
		//memcpy(term,(char*)(old->head)+old->record[i].offset,termlen);
		//term[termlen] = '\0';
		term = *(unsigned long *)((char *)old->head+old->record[i].offset);
#ifndef NDEBUG
		if(first == 0)
		{
			mask = (term & 0x7fffffff) %(1<<old->head->local_depth);
			first = 1;
		}
		else if(old->head->local_depth != 0)
		{

			assert(mask == (term & 0x7fffffff) % (1<<old->head->local_depth));
		}
#endif

		if(( (term & 0x7fffffff) & (1<<old->head->local_depth)) != 0) /* the significant bit is set,
								   * So,this record should move to new bucket */
		{
			if(bucket_insert(new,term,(char*)old->head+old->record[i].offset+termlen,old->record[i].invertedlen) != 0)
				return -1;
			if(bucket_remove(old,term) != 0)
				return -1;
		}
		else		/* NOTE:bucket_insert changed bucket->head->num internal,so shouldn't i++ in the case */
			i++;
	}
	old->head->local_depth++;
	old->page->dirty = true;

	return 0;
}

#ifdef BUCKET_TEST

/* this is written for simple unit test */
#include <stdio.h>

void bucket_print(struct bucket *bkt)
{
	unsigned short i;
	unsigned char termlen;
	char string[256];

	for(i=0; i<bkt->head->num; i++)
	{
		printf("entry %d: ",i);
		termlen = bkt->record[i].termlen;
		memcpy(string,(char*)bkt->head+bkt->record[i].offset,termlen);
		string[termlen] = '\0';
		printf("%s %d ",string,hash(string));
		printf("%d\n",bkt->record[i].invertedlen);
	}
}
int main()
{
	struct page page;
	struct bucket bkt;
	char invertedlist[] = "a simple test";
	char *term2 = "test2";
	char *term = "test";
	unsigned short pos;
	struct page newpage;
	struct bucket newbkt;
	unsigned long tmp,tmp2;

	page.mem = malloc(PAGE_SIZE);
	if(page.mem == NULL)
		goto exit;

	bucket_init(&bkt,&page,0);
	tmp = jshash(term, 512);
	tmp2 = jshash(term2, 512);
	if(bucket_insert(&bkt,tmp,invertedlist,strlen(invertedlist)) != 0)
	{
		printf("error bucket_insert\n");
		goto free_bucket;
	}
	if(bucket_insert(&bkt,tmp2,invertedlist,strlen(invertedlist)) != 0)
	{
		printf("error bucket_insert\n");
		goto free_bucket;
	}
	if(_bucket_find(&bkt,tmp,&pos) != 0)
	{
		printf("error _bucket_find\n");
		goto free_bucket;
	}
	if(bucket_remove(&bkt,tmp) != 0)
	{
		printf("error remove\n");
		goto free_bucket;
	}
	if(bucket_insert(&bkt,jshash("wod",512),invertedlist,strlen(invertedlist)) != 0)
	{
		printf("error bucket_insert\n");
		goto free_bucket;
	}
	if(bucket_insert(&bkt,jshash("bbcnews",512),invertedlist,strlen(invertedlist)) != 0)
	{
		printf("error bucket_insert\n");
		goto free_bucket;
	}
	if(bucket_insert(&bkt,jshash("bews",512),invertedlist,strlen(invertedlist)) != 0)
	{
		printf("error bucket_insert\n");
		goto free_bucket;
	}
	if(bucket_insert(&bkt,jshash("omga",512),invertedlist,strlen(invertedlist)) != 0)
	{
		printf("error bucket_insert\n");
		goto free_bucket;
	}
	if(bucket_insert(&bkt,jshash("pnwh",512),invertedlist,strlen(invertedlist)) != 0)
	{
		printf("error bucket_insert\n");
		goto free_bucket;
	}
	if(bucket_insert(&bkt,jshash("cnews",512),invertedlist,strlen(invertedlist)) != 0)
	{
		printf("error bucket_insert\n");
		goto free_bucket;
	}
	bucket_print(&bkt);
	printf("--------------------\n");
	newpage.mem = malloc(PAGE_SIZE);
	if(newpage.mem == NULL)
	{
		printf("error malloc newpage");
		goto free_bucket;
	}
	bucket_init(&newbkt,&newpage,0);
	bucket_splite(&bkt,&newbkt);
	bucket_print(&bkt);
	printf("----------------\n");
	bucket_print(&newbkt);

	free(newpage.mem);
	free(page.mem);
	return 0;
//free_newpage:
//	free(newpage.mem);
free_bucket:
//free_page_mem:
	free(page.mem);
exit:
	return 0;
}

#endif
