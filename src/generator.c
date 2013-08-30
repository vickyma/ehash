#include "hash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define TEXT_NUM 400		/* number of texts */
#define WORD_NUM 400		/* number of words in a text */
#define WORD_LEN 10		/* max len of a word */
#define BLOOMFILTER_SIZE (2*WORD_NUM)
#define FILENAME "testdata"

const char* table="abcdefghijklmnopqrstuvwxyz";

static char bloomfilter[BLOOMFILTER_SIZE];

void generator(char *string)	/* generator a random string */
{
	int tablelen = strlen(table);
	unsigned int strlen;
	int i;

	strlen = rand() % WORD_LEN;
	while(strlen < 2)
		strlen = rand() % WORD_LEN;
	for(i=0; i<strlen; i++)
	{
		string[i] = table[rand() % tablelen];
	}
	string[strlen] = '\0';
}
int main()
{
	char string[WORD_LEN];
	int i,j;
	int pos;
	FILE *f;

	srand(time(NULL));
	f = fopen(FILENAME,"w");
	for(i=1; i<=TEXT_NUM; i++)
	{
		memset(bloomfilter,0,WORD_NUM);
		for(j=0; j<WORD_NUM; j++)
		{
			while(1)
			{
				generator(string);
				pos = hash(string)%BLOOMFILTER_SIZE;
				if(bloomfilter[pos] != 0)
					continue;
				bloomfilter[pos] = 1;
				break;
			}
			fprintf(f,"%s %d %f\n",string,i,(double)rand());
		}
	}
	return 0;
}
