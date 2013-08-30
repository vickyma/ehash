#ifndef MERGE_H
#define MERGE_H

struct ehash;
struct tmpindex;
struct posting;
int merge(struct tmpindex *tmpidx,struct ehash *idx);
//char* merge_invertedlist(struct posting *p,char* invertedlist,unsigned int invertedlen,int *len);
#endif
