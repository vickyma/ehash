#include "index.h"
#include "ehash.h"
#include "tmpindex.h"
#include "merge.h"
#include "vector.h"
#include "vbyte.h"
#include "filemap.h"
#include "bucket.h"
#include "cache.h"
#include "hash.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>		/* for function log() defination */
#include <limits.h>		/* for USHRT_MAX defination */

#define TMPINDEXSLOTS 512
#define TMPINDEXLOADFACTOR 5
#define FILL_FACTOR 0.8
struct index
{
	struct tmpindex *tempIndex;
	struct ehash *extendibleHash;
	struct filemap *fileMap;
	FILE *indexFile;
};
struct index* indexNew(const char *filename)
{
	struct index *idx;
	idx = malloc(sizeof(struct index));
	if(idx == NULL)
		goto exit;
	idx->indexFile = fopen(filename,"w");
	if(idx->indexFile == NULL)
		goto free_index;
	idx->tempIndex = tmpindex_new(TMPINDEXSLOTS);
	if(idx->tempIndex == NULL)
		goto free_index;
	idx->extendibleHash = ehash_new();
	if(idx->extendibleHash == NULL)
		goto free_tmpindex;
	if(ehash_alloc(idx->extendibleHash) != 0)
		goto free_ehash;
	idx->fileMap = filemap_new();
	if(idx->fileMap == NULL)
		goto free_ehash;
	return idx;

free_ehash:
	ehash_free(idx->extendibleHash);
free_tmpindex:
	free(idx->tempIndex);
free_index:
	free(idx);
exit:
	return NULL;
}

/* indexFree just gurantee no memory leak...dump cache to disk is not it's duty */
void indexFree(struct index* idx)
{
	if(idx->extendibleHash)
		ehash_free(idx->extendibleHash);
	if(idx->tempIndex)
		tmpindex_free(idx->tempIndex);
	if(idx->fileMap)
		filemap_free(idx->fileMap);
	fclose(idx->indexFile);
	free(idx);
}

struct index* indexLoad(const char *filename)
{
	struct index *idx;
	char updated = 0;

	idx = malloc(sizeof(struct index));
	if(idx == NULL)
		goto exit;
	idx->indexFile = fopen(filename,"r+");
	if(idx->indexFile == NULL)
		goto free_index;
	if(fwrite(&updated,1,1,idx->indexFile) != 1) /* reserve 1 byte for file lock */
		goto free_index;
	idx->tempIndex = tmpindex_new(TMPINDEXSLOTS);
	if(idx->tempIndex == NULL)
		goto free_index;
	idx->extendibleHash = ehash_load(idx->indexFile);
	if(idx->extendibleHash == NULL)
		goto free_tmpindex;
	idx->fileMap = filemap_load(idx->indexFile);
	if(idx->fileMap == NULL)
		goto free_ehash;
	return idx;

free_ehash:
	ehash_free(idx->extendibleHash);
free_tmpindex:
	tmpindex_free(idx->tempIndex);
free_index:
	free(idx);
exit:
	return NULL;
}

bool indexDump(struct index *idx)
{
	if(merge(idx->tempIndex,idx->extendibleHash) != 0)
		return false;
	if(fseek(idx->indexFile,1L,SEEK_SET) != 0) /* always dump from the begin of the file,the first byte is
						    used for file lock*/
		return -1;
	if(ehash_dump(idx->extendibleHash,idx->indexFile) != 0)
		return false;
	if(filemap_dump(idx->fileMap,idx->indexFile) != 0)
		return false;
	return true;
}

bool indexInsert(struct index *idx,unsigned long t,unsigned int d, unsigned long tf)
{
	if(tmpindex_insert(idx->tempIndex,t,d,tf) != 0)
		return false;
	if(idx->tempIndex->largest > FILL_FACTOR * PAGE_SIZE)
	{
		merge(idx->tempIndex,idx->extendibleHash);
	}
/*	if(idx->tempIndex->count > idx->tempIndex->slots*TMPINDEXLOADFACTOR)
	{
		merge(idx->tempIndex,idx->extendibleHash);
	}
*/
	return true;
}

struct result* indexFind(struct index *idx,unsigned long t,unsigned int *num)
{
	struct posting *p;
	char *invertedlist;
	unsigned int invertedlen;
	struct result *result;

	unsigned int val;
	unsigned int docnum;
	unsigned int curdoc;
	int l;
	int pos;
	int i,j;
	unsigned long tf;
	struct page *page;
	struct bucket bkt;
	int fd;
	off_t offset;
	int remember_to_free_malloced_invertedlist = 0;

	result = NULL;
	*num = 0;
	invertedlist = ehash_find(idx->extendibleHash,t,&invertedlen);
	if(invertedlist)
	{
		if(invertedlen != USHRT_MAX)
		{
			docnum = *((unsigned int*)invertedlist);
			*num += docnum;
			pos = 2*sizeof(unsigned int);  /* skip docnum and lastdoc */
		}
		else
		{
			fd = *((int*)invertedlist);
			offset = *((off_t*)(invertedlist+sizeof(int)));
			page = cache_pagein(idx->extendibleHash->cache,fd,offset);
			if(page == NULL)
				goto exit;
			bucket_load(&bkt,page);
			docnum = *((unsigned int*)bkt.record);
			*num += docnum;
			invertedlen = bkt.head->num;
			invertedlist = malloc(invertedlen);
			if(invertedlist == NULL)
				goto exit;
			remember_to_free_malloced_invertedlist = 1;

			memcpy(invertedlist,(char*)bkt.record+2*sizeof(unsigned int),bkt.head->ptr-2*sizeof(unsigned int));
			pos = bkt.head->ptr-2*sizeof(unsigned int);

			while(bkt.head->next.fd != 0)
			{
				fd = bkt.head->next.fd;
				offset = bkt.head->next.offset;
				page = cache_pagein(idx->extendibleHash->cache,fd,offset);
				if(page == NULL)
					goto free_invertedlist;
				bucket_load(&bkt,page);
				memcpy(invertedlist+pos,(char*)bkt.record,bkt.head->ptr);
				pos += bkt.head->ptr;
			}
			assert(invertedlen == pos+2*sizeof(unsigned int));
			pos = 0;
		}
	}

	p = tmpindex_find(idx->tempIndex,t);
	if(p)
		*num += p->count;

	if(*num != 0)
	{
		i = 0;
		result = malloc((*num) * sizeof(struct result));
		if(result == NULL)
			goto exit;
		if(invertedlist)
		{

			l = vbyte_decompress(invertedlist+pos,invertedlist+invertedlen,&curdoc);
			pos += l;

			result[i].d = curdoc;
			tf = *(unsigned long*)(invertedlist+pos);
			result[i].w = tf;//* log(idx->fileMap->total/(*num));
			pos += sizeof(unsigned long);
			i++;

			for(j=1; j<docnum; j++)
			{
				l = vbyte_decompress(invertedlist+pos,invertedlist+invertedlen,&val);
				if(l == 0 || val==0)
					goto error;
				pos += l;
				curdoc += val;

				result[i].d = curdoc;
				tf = *(unsigned long*)(invertedlist+pos);
				result[i].w = tf;//* log(idx->fileMap->total/(*num));
				i++;

				pos += sizeof(unsigned long);
			}
		}
		if(remember_to_free_malloced_invertedlist)
			free(invertedlist);
		if(p)
		{
			vector_reset_pos(p->invertedlist);
			if(!vector_get_vbyte(p->invertedlist,&curdoc))
				goto error;
			result[i].d = curdoc;
			if(!vector_get_ulint(p->invertedlist,&tf))
				goto error;
			result[i].w = tf;// * log(idx->fileMap->total/(*num));
			i++;
			for(j=1; j<p->count; j++)
			{
				if(!vector_get_vbyte(p->invertedlist,&val))
					goto error;
				curdoc += val;
				result[i].d = curdoc;
				if(!vector_get_ulint(p->invertedlist,&tf))
					goto error;
				result[i].w = tf;// * log(idx->fileMap->total/(*num));
				i++;
			}
		}
	}
	return result;

free_invertedlist:
	if(remember_to_free_malloced_invertedlist)
		free(invertedlist);
error:
	free(result);
exit:
	return NULL;
}

bool indexAdd(struct index *idx,const char *filename,unsigned int *d)
{
	return (filemap_add(idx->fileMap,filename,d)==0) ? true:false;
}

const char* indexFilename(struct index *idx,unsigned int docid)
{
	struct filemap *fmp;
	fmp = idx->fileMap;
	if(docid >= fmp->total)
		return NULL;
	return fmp->filename[docid];
}

int indexFd(struct index *idx)
{
	return fileno(idx->indexFile);
}

#define INDEX_TEST
#ifdef INDEX_TEST

#include "vbyte.h"
#include <stdio.h>


int create_test()
{
	struct index *idx;
	FILE *f;
	char c[10];
	unsigned int d;
	double w;
	int i;
	unsigned int tmp;

	idx = indexNew("index.map");
	if(idx == NULL)
		return -1;
	f = fopen("testdata","r");
	if(f == NULL)
	{
		perror("fopen:");
		return -1;
	}

	i = 0;
	unsigned long tmp2 = 0;
	while(fscanf(f,"%s %d %lf\n",c,&tmp,&w) != EOF)
	{
		if(i == 0)
		{
			if(!indexAdd(idx,"asdf",&d))
				return -1;
		}
		tmp2 = jshash(c, idx->tempIndex->slots);
		printf("i=%d d=%d\n", i, d++);
		indexInsert(idx,tmp2,d,w); /* add data to index */
		i++;
		//if(i == 100)
			//i = 0;
	}

	if(!indexDump(idx))	/* index dump */
		return -1;
	indexFree(idx);

	return 0;
}

int invertedlist_bigger_than_page_test()
{
	struct index *idx;	
	unsigned int d = 1;
	int i;
	struct result *r;	
	unsigned int num;
	unsigned long tmp = 0;

	idx = indexNew("index.map");
	if(idx == NULL)
		return -1;

	tmp = jshash("test", idx->tempIndex->slots);
	for(i=0; i<123500; i++)
	{
		if(!indexInsert(idx,tmp,d++,i+1))
				return -1;
	}

	if(!indexDump(idx))	/* index dump */
		return -1;
	r = indexFind(idx,tmp,&num);
	if(r)
	{
		for(i=0; i<num; i++)
		{
			printf("<%d,%lu>\n",r[i].d,r[i].w);
		}
		free(r);
	}
	indexLoad("index.map");
	indexFree(idx);

	return 0;
}

int load_test()
{
	struct index *idx;
	struct result *result;
	unsigned int num;
	int i;
	unsigned long tmp=0;

	idx = indexLoad("index.map");
	if(idx == NULL)
		return -1;

	tmp = jshash("ddm", idx->tempIndex->slots);
	result = indexFind(idx,tmp,&num);
	if(result)
	{
		for(i=0; i<num; i++)
		{
			printf("<%d %lu>",result[i].d,result[i].w);
		}
		free(result);
	}
	getchar();
	if(!indexDump(idx))	/* index dump */
		return -1;
	indexFree(idx);
	return 0;
}

int main(int argc,char *argv[])			/* for unit test */
{
	if(argc != 2)
	{
		printf("usage:%s [c|l]",argv[0]);
		return -1;
	}
	switch(*argv[1])
	{
	case 'c':
		return create_test();
	case 'l':
		return load_test();
	case 'b':
		return invertedlist_bigger_than_page_test();
	default:
		printf("usage:%s [c|l]",argv[0]);
	}
	return 0;
}
#endif
