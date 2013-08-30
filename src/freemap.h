#ifndef FREEMAP_H
#define FREEMAP_H

#include <unistd.h>		/* for type off_t */
#include <stdio.h>		/* for type FILE* */

enum freemap_option
{
	FREEMAP_CREAT = 0,
	FREEMAP_LOAD = 1
};

struct open_file_record
{
	char filename[12];	/* filename is "indexXXXXXX" */
	int fd;
};
struct freemap_vec
{
	int fd;
	unsigned int offset;
	unsigned int size;
};
struct freemap
{
	struct freemap_vec *array;
	unsigned int size;	/* total size of array */
	unsigned int used;	/* used size of array */
	struct open_file_record *open_file_record; /* map filename to fd or otherwise */
	unsigned short record_size;		   /* total size of open_file_record */
	unsigned short record_used;		   /* used size of open_file_record */
};

struct freemap *freemap_new();

void freemap_free(struct freemap *freemap);

int freemap_add(struct freemap *freemap,char *filename,enum freemap_option opt);

int freemap_malloc(struct freemap *freemap,unsigned int *size,int *fd,off_t *offset);

int freemap_dump(struct freemap *freemap,FILE *f);
struct freemap* freemap_load(FILE *f);
#endif
