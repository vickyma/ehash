#include "freemap.h"
#include <fcntl.h>
#include <sys/resource.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct freemap *freemap_new()
{
	struct freemap *ret;

	ret = malloc(sizeof(struct freemap));
	if(ret == NULL)
		goto exit;
	ret->size = 1;
	ret->array = malloc(sizeof(struct freemap_vec));
	if(ret->array == NULL)
		goto free_freemap;
	memset(ret->array,0,sizeof(struct freemap_vec));
	ret->used = 0;

	ret->open_file_record = malloc(sizeof(struct open_file_record));
	if(ret->open_file_record == NULL)
		goto free_array;
	ret->record_size = 1;
	ret->record_used = 0;

	return ret;
free_array:
	free(ret->array);
free_freemap:
	free(ret);
exit:
	return NULL;
}

void freemap_free(struct freemap *freemap)
{
	
	if(freemap->array)
		free(freemap->array);
	if(freemap->open_file_record)
		free(freemap->open_file_record);
	free(freemap);
	freemap = NULL;
}

/* return the max size of a file on success, 0 on error */
static unsigned int getfilemaxsize()
{
	struct rlimit ret;
	unsigned int val;
	if(getrlimit(RLIMIT_FSIZE,&ret) != 0)
		return 0;
	val = ret.rlim_cur;
	return val;	
}

int freemap_add(struct freemap *freemap,char *filename,enum freemap_option opt)
{
	int fd;
	unsigned int filemaxsize;
	char tmpfilename[]="indexXXXXXX";

	assert(freemap);
	if(opt == FREEMAP_CREAT)
	{
		if(freemap->record_used >= freemap->record_size) /* ensure we have enough size for open_file_record */
		{
			freemap->open_file_record = realloc(freemap->open_file_record,2*freemap->record_size *sizeof(struct open_file_record));
			if(freemap->open_file_record == NULL)
				goto exit;
			freemap->record_size *= 2;
		}
		fd = mkstemp(tmpfilename);
		if(fd == -1)
			goto exit;
		freemap->open_file_record[freemap->record_used].fd = fd;
		strcpy(freemap->open_file_record[freemap->record_used].filename,tmpfilename);
		freemap->record_used++;

		freemap->array[freemap->used].fd = fd;
		freemap->array[freemap->used].offset = 0;
		filemaxsize = getfilemaxsize();
		printf("filemax size=0x%x\n", filemaxsize);
		if(filemaxsize == 0)
			goto free_open_file_record;
		freemap->array[freemap->used].size = filemaxsize;

		freemap->used++;
		return 0;
	}
	else if(opt == FREEMAP_LOAD)
	{
	}
	else
		return -1;
free_open_file_record:		/* FIXIT:need free open_file_record.filename's space */
	free(freemap->open_file_record);
exit:
	return -1;
}

/* the arguement "size" is a hack here.it's a input value,and it's also a return value
 * because if a piece is too small to spilte,we'll return the whole piece,so the size
 * maybe not the value declined */
int freemap_malloc(struct freemap *freemap,unsigned int *size,int *fd,off_t *offset)
{
	int i;
	assert(fd);
	assert(offset);

	for(i = 0;i < freemap->used; i++)
	{
		if(freemap->array[i].size > *size)
		{
			*fd = freemap->array[i].fd;
			*offset = freemap->array[i].offset;

			if((freemap->array[i].size - *size) < *size) /* alloc the whole piece */
			{
				*size = freemap->array[i].size;
				memmove(&freemap->array[i],&freemap->array[i+1],(freemap->used-i-1)*sizeof(struct freemap_vec));
			}
			else
			{
				freemap->array[i].size -= *size;
				freemap->array[i].offset += *size;
			}
			return 0;
		}
	}
	return -1;
}

int freemap_dump(struct freemap *freemap,FILE *f)
{
	if(fwrite(&freemap->record_used,sizeof(unsigned short),1,f) != 1)
		return -1;
	if(fwrite(freemap->open_file_record,sizeof(struct open_file_record),freemap->record_used,f) != freemap->record_used)
		return -1;

	if(fwrite(&freemap->used,sizeof(unsigned int),1,f) != 1)
		return -1;
	if(fwrite(freemap->array,sizeof(struct freemap_vec),freemap->used,f) != freemap->used)
		return -1;
	fflush(f);
	return 0;
}

struct freemap* freemap_load(FILE *f)
{
	struct freemap *freemap;
	freemap = malloc(sizeof(struct freemap));
	if(freemap == NULL)
		goto exit;
	if(fread(&freemap->record_used,sizeof(unsigned short),1,f) != 1)
		goto free;
	freemap->record_size = freemap->record_used;
	freemap->open_file_record = malloc(freemap->record_size*sizeof(struct open_file_record));
	if(freemap->open_file_record == NULL)
		goto free;
	if(fread(freemap->open_file_record,sizeof(struct open_file_record),freemap->record_used,f) != freemap->record_used)
		goto free_open_file_record;
	if(fread(&freemap->used,sizeof(unsigned int),1,f) != 1)
		goto free_open_file_record;
	freemap->size = freemap->used;
	freemap->array = malloc(freemap->size*sizeof(struct freemap_vec));
	if(freemap->array == NULL)
		goto free_open_file_record;
	if(fread(freemap->array,sizeof(struct freemap_vec),freemap->used,f) != freemap->used)
		goto free_array;
	return freemap;

free_array:
	free(freemap->array);
free_open_file_record:
	free(freemap->open_file_record);
free:
	free(freemap);
exit:
	return NULL;
}

#ifdef FREEMAP_TEST
/* this is written for unit test */

#include <stdio.h>

int main(int argc,char *argv[])
{
	struct freemap *fm;
	int fd;
	off_t offset;
	unsigned int size;
	FILE *f;
	struct freemap *newfm;

	fm = freemap_new();
	if(fm == NULL)
		return -1;
	if(freemap_add(fm,NULL,FREEMAP_CREAT) != 0)
		return -1;
	size = 4096;
	if(freemap_malloc(fm,&size,&fd,&offset) != 0)
		return -1;

	f = fopen("test","w+");
	if(freemap_dump(fm,f) != 0)
		return -1;
	if(fseek(f,SEEK_SET,0L) != 0)
		return -1;
	newfm = freemap_load(f);
	if(newfm == NULL)
		return -1;
	freemap_free(fm);
	freemap_free(newfm);
	return 0;
}
#endif
