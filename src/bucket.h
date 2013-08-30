#ifndef BUCKET_H
#define BUCKET_H

#include <unistd.h>		/* for off_t */
#include <stdbool.h>		/* for bool */
/* to avoid the use of boring to death Macro,i use some tricky skills here.
 * i use struct instead of unstructed void* .this may cause more use of space
 * for aligned reason. but it's neat and readible to coder.
 */
struct bucket
{
	struct HEAD
	{
		unsigned int num;
		unsigned int local_depth;
		unsigned int ptr; /* record the first inverted list's position.for fast adding */
		struct
		{
			int fd;
			off_t offset;
		}next;
	}*head;
	struct RECORD
	{
		unsigned int offset;
		unsigned char termlen;
		unsigned short invertedlen; 
	}*record;
	struct page *page;
	bool init;
};
struct page;
struct bucket *bucket_new(struct page *page);
void bucket_alloc(struct bucket *bkt,unsigned int local_depth);
void bucket_init(struct bucket *bkt,struct page *page,unsigned int local_depth);
char* bucket_find(struct bucket *bkt,unsigned long term,unsigned int *invertedlen);
void bucket_load(struct bucket *bkt,struct page *page);
int bucket_splite(struct bucket *old,struct bucket *buck);
int bucket_remove(struct bucket *bkt,unsigned long term); 
int bucket_insert(struct bucket *bkt,unsigned long term,void *invertedlist,unsigned short invertedlen);
void bucket_free(struct bucket *bkt);
#endif
