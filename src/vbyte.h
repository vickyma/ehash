/* vbyte.h declares functions to read and write variable-byte encoded
 * integers to files
 */

#ifndef VBYTE_H
#define VBYTE_H

#ifdef _cplusplus
extern "C" {
#endif

	int vbyte_len(unsigned int n);

	int vbyte_compress(char *src,char *bound,unsigned int n);
	int vbyte_decompress(char *src,char *bound,unsigned int *n);


#ifdef _cplusplus
}
#endif

#endif

