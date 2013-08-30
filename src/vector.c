#include "vector.h"
#include "vbyte.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>


struct vector* vector_new(unsigned int initsize) 
{
    struct vector* v = malloc(sizeof(*v));

    if (v && (v->vector = malloc(initsize))) {
        v->size = initsize;
        v->pos = v->len = 0;
        return v;
    } else if (v) {
        free(v);
    }

    return NULL;
}

bool vector_expand(struct vector* v, unsigned int size) 
{
    /* grow the buffer (+ 1 is to step around case where size is 0) */
    void* newmem = realloc(v->vector, size);

    if (newmem) {
        v->size = size;
        v->vector = newmem;
        return true;
    } else {
        return false;
    }
}

/*
struct vector* vector_resize(struct vector* v, unsigned long int initsize) 
{
    void* newmem;

    if (!v) {
        if ((v = malloc(sizeof(*v))) 
          && (v->vector = malloc(initsize))) {
            v->size = initsize;
            v->pos = v->len = 0;
            v->err = 0;
        } else if (v) {
            free(v);
            return NULL;
        } else {
            return NULL;
        }
    } else {
        if ((newmem = realloc(v->vector, initsize))) {
            v->size = initsize;
            v->vector = newmem;
            v->pos = v->len = 0;
        } else {
            return NULL;
        }
    }

    return v;
}
*/

void vector_free(struct vector* v) 
{
    free(v->vector);
    free(v);
}

void vector_reset_pos(struct vector* v) 
{
    v->pos = 0;
}

/*
unsigned int vector_append(struct vector* dst, const struct vector* src) 
{
    unsigned int size = src->len - src->pos;

    while ((dst->size < dst->pos + size) && vector_expand(dst, dst->pos + size)) ;

    if (!dst->err && (dst != src)) {
        memcpy(dst->vector + dst->pos, src->vector + src->pos, size);
        dst->pos += size;
        dst->len = dst->pos;
        return size;
    } else {
        return 0;
    }
}
*/


bool vector_put_uint(struct vector *v,unsigned int d)
{
	if(v->pos+sizeof(unsigned int) > v->size && !vector_expand(v,2*v->size + 1))
	{
		return false;
	}
	*(unsigned int*)(v->vector+v->pos) = d;
	v->pos += sizeof(unsigned int);
	if(v->pos > v->len)
		v->len = v->pos;
	return true;
}

bool vector_get_uint(struct vector *v,unsigned int *d)
{
	if(v->pos+sizeof(unsigned int) > v->len)
		return false;
	*d = *(unsigned int*)(v->vector+v->pos);
	v->pos += sizeof(unsigned int);
	return true;
}

bool vector_put_ulint(struct vector *v,unsigned long int d)
{
	if(v->pos+sizeof(unsigned long int) > v->size && !vector_expand(v,2*v->size + 1))
	{
		return false;
	}
	*(unsigned long int*)(v->vector+v->pos) = d;
	v->pos += sizeof(unsigned long int);
	if(v->pos > v->len)
		v->len = v->pos;
	return true;
}

bool vector_get_ulint(struct vector *v,unsigned long int *d)
{
	if(v->pos+sizeof(unsigned long int) > v->len)
		return false;
	*d = *(unsigned long int*)(v->vector+v->pos);
	v->pos += sizeof(unsigned long int);
	return true;
}

bool vector_put_double(struct vector *v,double w)
{
	if(v->pos+sizeof(double) > v->size && !vector_expand(v,2*v->size + 1))
	{
		return false;
	}
	(*(double*)(v->vector+v->pos)) = w;
	v->pos += sizeof(double);
	if(v->pos > v->len)
		v->len = v->pos;
	return true;
}

bool vector_put_float(struct vector *v,float w)
{
	if(v->pos+sizeof(float) > v->size && !vector_expand(v,2*v->size + 1))
	{
		return false;
	}
	(*(float*)(v->vector+v->pos)) = w;
	v->pos += sizeof(float);
	if(v->pos > v->len)
		v->len = v->pos;
	return true;
}

bool vector_get_double(struct vector *v,double *w)
{
	if(v->pos+sizeof(double) > v->len)
		return false;
	*w = *(double*)(v->vector+v->pos);
	v->pos += sizeof(double);
	return true;
}

bool vector_get_float(struct vector *v,float *w)
{
	if(v->pos+sizeof(float) > v->len)
		return false;
	*w = *(float*)(v->vector+v->pos);
	v->pos += sizeof(float);
	return true;
}


bool vector_put_vbyte(struct vector *v,unsigned int n)
{
	int len;
	len = vbyte_compress(v->vector+v->pos,v->vector+v->size,n);
	if(len == 0)
	{
		if(!vector_expand(v,2*v->size+1))
			return false;
		return vector_put_vbyte(v,n);
	}
	v->pos += len;
	if(v->pos > v->len)
		v->len = v->pos;
	return true;
}

bool vector_get_vbyte(struct vector *v,unsigned int *n)
{
	int len;
	len = vbyte_decompress(v->vector+v->pos,v->vector+v->len,n);
	if(len == 0)
		return false;
	v->pos += len;
	return true;
}

 /*
unsigned long int vector_scanvbyte(struct vector* v) 
{
    unsigned long int ret = 1;

    while ((v->pos < v->len)) {
        if (!(v->vector[v->pos++] & 0x80)) { 
            return ret;
        }
        ret++;
    }

    v->err = ENOSPC;
    return 0;
}
*/

bool vector_eof(const struct vector* v) {
    return (v->pos == v->len);
}

unsigned long int vector_position(const struct vector* v) 
{
    return v->pos;
}

unsigned long int vector_length(const struct vector* v) 
{
    return v->len;
}
/*
struct vec* vector_copy(const struct vec* v) 
{
    struct vec* nv = malloc(sizeof(*nv));

    if (nv && (nv->vector = malloc(v->len))) {
        nv->pos = v->pos;
        nv->len = v->len;
        nv->size = v->len;
        nv->err = 0;
        memcpy(nv->vector, v->vector, nv->size);
    } else if (nv) {
        free(nv);
        return NULL;
    }

    return nv;
}
*/

/*
struct vec* vec_read(int srcfd, unsigned int len) {
    struct vec* v;

    if ((v = malloc(sizeof(*v))) && vec_read_inplace(srcfd, v, len)) {
        return v;
    } else {
        if (v) {
            free(v);
        }
        return NULL;
    }
}

unsigned int vec_write(struct vec* src, int dstfd) {
    unsigned int wlen,
                 len = src->len - src->pos;
    char *srcbuf = src->vector;


    while (len && ((wlen = write(dstfd, srcbuf, len)) >= 0)) {
        len -= wlen;
        srcbuf += wlen;
    }

    if (!len) {
        return src->len - src->pos;
    } else {
        src->err = errno;
        return 0;
    }
}

int vec_cmp(const struct vec *one, const struct vec *two) {
    if (one->len != two->len) {
        return one->len - two->len;
    } else {
        return (memcmp(one->vector, two->vector, one->len));
    }
}
*/

#ifdef VECTOR_TEST

 /* this is written for unit test */

#include <stdio.h>
int main()
{
	struct vector *v;
	unsigned int lastval;
	unsigned int val;
	unsigned long w;

	v = vector_new(8);
	if(v == NULL)
		return -1;
	vector_put_uint(v,4);
	vector_put_vbyte(v,1);
	vector_put_vbyte(v,4357);
	vector_put_ulint(v,42UL);

	vector_reset_pos(v);
	vector_get_uint(v,&lastval);
	printf("%d ",lastval);
	vector_get_vbyte(v,&val);
	lastval += val;
	printf("%d ",lastval);
	vector_get_vbyte(v,&val);
	lastval += val;
	printf("%d\n",lastval);
	vector_get_ulint(v,&w);
	printf("%lu\n",w);

	vector_free(v);
	return 0;
}

#endif
