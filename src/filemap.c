#include "filemap.h"
#include <stdlib.h>
#include <string.h>


struct filemap* filemap_new()
{
	struct filemap *ret;
	ret = malloc(sizeof(struct filemap));
	if(ret == NULL)
		goto exit;
	ret->size = 8;
	ret->total = 0;
	ret->filename = malloc(8 * sizeof(char*));
	if(ret->filename == NULL)
		goto free_filemap;
	return ret;

free_filemap:
	free(ret);
exit:
	return NULL;
}

int filemap_add(struct filemap *filemap,const char *filename,unsigned int *docid)
{
	if(filemap->total >= filemap->size)
	{
		filemap->size *= 2;
		filemap->filename = realloc(filemap->filename,filemap->size * sizeof(char*));
		if(filename == NULL)
			return -1;
	}
	*docid = filemap->total;
	filemap->filename[*docid] = malloc(strlen(filename)+1);
	if(filemap->filename[*docid] == NULL)
		return -1;
	strcpy(filemap->filename[filemap->total],filename);
	filemap->total++;

	return 0;
}

int filemap_dump(struct filemap *filemap,FILE *f)
{
	unsigned int i;
	int size;

	if(fwrite(&filemap->total,sizeof(unsigned int),1,f) != 1)
		return -1;
	for(i=0; i<filemap->total; i++)
	{
		size = strlen(filemap->filename[i]);
		if(fwrite(&size,sizeof(int),1,f) != 1)
			return -1;
		if(fwrite(filemap->filename[i],size,1,f) != 1)
			return -1;
	}
	return 0;
}

struct filemap* filemap_load(FILE *f)
{
	struct filemap *ret;
	unsigned int i;
	int size;

	ret = malloc(sizeof(struct filemap));
	if(ret == NULL)
		goto exit;
	if(fread(&ret->total,sizeof(unsigned int),1,f) != 1)
		goto free_filemap;
	ret->size = ret->total;
	ret->filename = malloc(ret->size*sizeof(char*));
	if(ret->filename == NULL)
		goto free_filemap;
	for(i=0; i<ret->total; i++)
	{
		if(fread(&size,sizeof(int),1,f) != 1)
			goto free_filename;
		ret->filename[i] = malloc(size+1);
		if(ret->filename[i] == NULL)
			goto free_filename;
		if(fread(ret->filename[i],size,1,f) != 1)
			goto free_filename;
		ret->filename[i][size] = '\0';
	}
	return ret;

free_filename:
	free(ret->filename);
free_filemap:
	free(ret);
exit:
	return NULL;
}

void filemap_free(struct filemap *filemap)
{
	int i;
	for(i=0; i<filemap->total; i++)
	{
		free(filemap->filename[i]);
	}
	free(filemap->filename);
	free(filemap);
}
