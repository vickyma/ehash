/* vbyte.c implements variable byte encoding and decoding */

#include "vbyte.h"
#include <stdbool.h>

int vbyte_len(unsigned int n) {
    unsigned int ret = 1;
    while (n >= 128) {
        n >>= 7;
        ret++;
    }
    return ret;
}

/* compress n into src,if violate the bound,return 0,else return compressed size */
int vbyte_compress(char *src,char *bound,unsigned int n)
{
	int len = 0;

	while(n >= 128)
	{
		if(src+len < bound)
		{
			*(src+len) = (char)((n & 0xff)|0x80);
			n >>= 7;
			len++;
		}
		else
		{
			return 0;
		}
	}
	if(src+len < bound)
	{
		*(src+len) = (char)n;
		len++;
	}
	return len;
}

int vbyte_decompress(char *src,char *bound,unsigned int *n)
{
	int len = 0;

	*n = 0;
	while((src+len<bound) && (*(src+len) & 0x80))
	{
		*n += (*(src+len)&0x7f)<<(7*len);
		len++;
	}
	if(src+len == bound)
	{
		return 0;
	}
	*n += (*(src+len)&0x7f)<<(7*len);
	len++;
	return len;
}
