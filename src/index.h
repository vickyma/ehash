#ifndef INDEX_H
#define INDEX_H


#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

	struct result
	{
		unsigned int d;
		unsigned long w;
	};
	struct index;

	struct index* indexNew(const char *filename);
	void indexFree(struct index* idx);
	bool indexAdd(struct index *idx,const char *filename,unsigned int *d);
	bool indexInsert(struct index *idx,unsigned long t,unsigned int d, unsigned long tf);
	/* Note: returned value is malloced,should free it to avoid memory leak */
	struct result* indexFind(struct index *idx,unsigned long t,unsigned int *num);
	bool indexDump(struct index *idx);
	struct index* indexLoad(const char *filename);
	const char* indexFilename(struct index *idx,unsigned int docid);
	int indexFd(struct index *idx);

#ifdef __cplusplus
}
#endif

#endif
