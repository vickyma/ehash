#ifndef VECTOR_H
#define VECTOR_H

#include <stdbool.h>

struct vector 
{
    char *vector;                /* the actual bytes to hold the vector */
    unsigned long int size;      /* the capacity of vector */
    unsigned long int pos;       /* where we are in vector */
    unsigned long int len;       /* the current length of vector */
};

struct vector* vector_new(unsigned int initsize);
void vector_free(struct vector*);
bool vector_put_uint(struct vector *v,unsigned int d);
bool vector_put_ulint(struct vector *v,unsigned long int d);
bool vector_put_double(struct vector *v,double w);
bool vector_put_float(struct vector *v,float w);
bool vector_put_vbyte(struct vector* v, unsigned int d);

bool vector_get_uint(struct vector *v,unsigned int *d);
bool vector_get_ulint(struct vector *v,unsigned long int *d);
bool vector_get_double(struct vector *v,double *w);
bool vector_get_float(struct vector *v,float *w);
bool vector_get_vbyte(struct vector *v,unsigned int *d);

void vector_reset_pos(struct vector* v);
bool vector_expand(struct vector* v, unsigned int size);

#endif
