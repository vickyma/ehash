#ifndef FILEMAP_H
#define FILEMAP_H

#include <stdio.h>		/* for type FILE */

struct filemap
{
	char **filename;
	unsigned int size;	/* size of filename array */
	unsigned int total;	/* current docid */
};

struct filemap* filemap_new();
int filemap_add(struct filemap *filemap,const char *filename,unsigned int *docid);
int filemap_dump(struct filemap *filemap,FILE *f);
struct filemap* filemap_load(FILE *f);
void filemap_free(struct filemap *filemap);

#endif
