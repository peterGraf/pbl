/*
 pbl.c - Basic library functions

 Copyright (C) 2002 - 2007   Peter Graf

 This file is part of PBL - The Program Base Library.
 PBL is free software.

	MIT License

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.

 For more information on the Program Base Library or Peter Graf,
 please see: https://www.mission-base.com/.

   $Log: pbl.c,v $
   Revision 1.22  2021/06/23 14:32:49  peter
   Switch to MIT license

   Revision 1.21  2021/06/23 13:39:52  peter

*/
/*
 * Make sure "strings <exe> | grep Id | sort -u" shows the source file versions
 */
char* pbl_c_id = "$Id: pbl.c,v 1.22 2021/06/23 14:32:49 peter Exp $";

#include <stdio.h>
#include <string.h>
#include <memory.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#include <time.h>

#include "pbl.h"

static char pbl_errbuf[PBL_ERRSTR_LEN + 1];

int    pbl_errno;
char* pbl_errstr = pbl_errbuf;

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/

/**
  * Replacement for malloc().
  *
  * @return  void * retptr == NULL: OUT OF MEMORY
  * @return  void * retptr != NULL: pointer to buffer allocated
  */
void* pbl_malloc(
	char* tag,           /** tag used for memory leak detection */
	size_t   size        /** number of bytes to allocate        */
)
{
	if (!tag)
	{
		tag = "pbl_malloc";
	}

	void* ptr = malloc(size);
	if (!ptr)
	{
#ifdef _WIN32
#pragma warning(disable: 4996)
#endif
		snprintf(pbl_errstr, PBL_ERRSTR_LEN,
			"%s: failed to malloc %d bytes\n", tag, (int)size);
		pbl_errno = PBL_ERROR_OUT_OF_MEMORY;
	}
	return ptr;
}

/**
  * Replacement for malloc(), initializes the memory to 0.
  *
  * @return  void * retptr == NULL: OUT OF MEMORY
  * @return  void * retptr != NULL: pointer to buffer allocated
  */
void* pbl_malloc0(
	char* tag,           /** tag used for memory leak detection */
	size_t   size        /** number of bytes to allocate        */
)
{
	if (!tag)
	{
		tag = "pbl_malloc0";
	}

	void* ptr = calloc((size_t)1, size);
	if (!ptr)
	{
#ifdef _WIN32
#pragma warning(disable: 4996)
#endif
		snprintf(pbl_errstr, PBL_ERRSTR_LEN,
			"%s: failed to calloc %d bytes\n", tag, (int)size);
		pbl_errno = PBL_ERROR_OUT_OF_MEMORY;
		return ptr;
	}
	return ptr;
}


/**
  * Duplicate a buffer, similar to strdup().
  *
  * @return  void * retptr == NULL: OUT OF MEMORY
  * @return  void * retptr != NULL: pointer to buffer allocated
  */
void* pbl_memdup(
	char* tag,         /** tag used for memory leak detection */
	void* data,        /** buffer to duplicate                */
	size_t size        /** size of that buffer                */
)
{
	if (!tag)
	{
		tag = "pbl_memdup";
	}

	void* ptr = pbl_malloc(tag, size);
	if (!ptr)
	{
		return ptr;
	}
	return memcpy(ptr, data, size);
}

/**
  * Duplicate a string, similar to strdup().
  *
  * @return  void * retptr == NULL: OUT OF MEMORY
  * @return  void * retptr != NULL: pointer to buffer allocated
  */
void* pbl_strdup(
	char* tag,        /** tag used for memory leak detection */
	char* data        /** string to duplicate                */
)
{
	if (!tag)
	{
		tag = "pbl_strdup";
	}
	return pbl_memdup(tag, data, strlen(data) + 1);
}


/**
  * Duplicate and concatenate two memory buffers.
  *
  * @return  void * retptr == NULL: OUT OF MEMORY
  * @return  void * retptr != NULL: pointer to new buffer allocated
  */
void* pbl_mem2dup(
	char* tag,         /** tag used for memory leak detection */
	void* mem1,        /** first buffer to duplicate          */
	size_t len1,       /** length of first buffer             */
	void* mem2,        /** second buffer to duplicate         */
	size_t len2        /** length of second buffer            */
)
{
	if (!tag)
	{
		tag = "pbl_mem2dup";
	}

	void* ptr = pbl_malloc(tag, len1 + len2);
	if (!ptr)
	{
		return ptr;
	}
	if (len1)
	{
		memcpy(ptr, mem1, len1);
	}
	if (len2)
	{
		memcpy(((char*)ptr) + len1, mem2, len2);
	}
	return ptr;
}

/**
 * Replacement for memcpy with target length check.
 *
 * @return   size_t rc: number of bytes copied
 */
size_t pbl_memlcpy(
	void* to,           /** target buffer to copy to                             */
	size_t tolen,       /** number of bytes in the target buffer                 */
	void* from,         /** source to copy from                                  */
	size_t n            /** length of source                                     */
)
{
	size_t l = n > tolen ? tolen : n;

	memcpy(to, from, l);
	return l;
}

/**
 * Find out how many starting bytes of two buffers are equal.
 *
 * @return   int rc: number of equal bytes
 */
int pbl_memcmplen(
	void* left,     /** first buffer for compare               */
	size_t llen,    /** length of that buffer                  */
	void* right,    /** second buffer for compare              */
	size_t rlen     /** length of that buffer                  */
)
{
	unsigned int i;
	unsigned char* l = (unsigned char*)left;
	unsigned char* r = (unsigned char*)right;

	if (llen > rlen)
	{
		llen = rlen;
	}

	for (i = 0; i < llen; i++)
	{
		if (*l++ != *r++)
		{
			break;
		}
	}

	return i;
}

/**
 * Compare two memory buffers, similar to memcmp.
 *
 * @return   int rc  < 0: left is smaller than right
 * @return   int rc == 0: left and right are equal
 * @return   int rc  > 0: left is bigger than right
 */
int pbl_memcmp(
	void* left,     /** first buffer for compare               */
	size_t llen,    /** length of that buffer                  */
	void* right,    /** second buffer for compare              */
	size_t rlen     /** length of that buffer                  */
)
{
	size_t len;

	/*
	 * a buffer with a length 0 is logically smaller than any other buffer
	 */
	if (!llen)
	{
		if (!rlen)
		{
			return 0;
		}
		return -1;
	}
	if (!rlen)
	{
		return 1;
	}

	/*
	 * use the shorter of the two buffer lengths for the memcmp
	 */
	if (llen <= rlen)
	{
		len = llen;
	}
	else
	{
		len = rlen;
	}

	/*
	 * memcmp is used, therefore the ordering is ascii
	 */
	int rc = memcmp(left, right, len);
	if (rc)
	{
		return rc;
	}

	/*
	 * if the two buffers are equal in the first len bytes, but don't have
	 * the same lengths, the longer one is logically bigger
	 */
	return (int)((int)llen - ((int)rlen));
}

/**
 * Copy a two byte short to a two byte buffer.
 */
void pbl_ShortToBuf(
	unsigned char* buf,        /** buffer to copy to                 */
	int s                       /** short value to copy               */
)
{
	*buf++ = (unsigned char)(s >> 8);
	*buf = (unsigned char)(s);
}

/**
 * Read a two byte short from a two byte buffer.
 *
 * @return int rc: the short value read
 */
int pbl_BufToShort(
	unsigned char* buf            /** buffer to read from      */
)
{
	unsigned int s = ((unsigned int)(*buf++)) << 8;

	s |= *buf;
	return s;
}

#define PBL_BUFTOSHORT( PTR ) ((( 0 | PTR[ 0 ]) << 8) | PTR[ 1 ] )

/**
 * Copy a four byte long to a buffer as hex string like "0f0f0f0f".
 */
void pbl_LongToHexString(
	unsigned char* buf,         /** buffer to copy to                 */
	unsigned long l             /** long value to copy                */
)
{
	int c;
	int i;

	buf[8] = 0;

	for (i = 8; i > 0; )
	{
		if (!l)
		{
			memcpy(buf, "00000000", i);
			return;
		}

		c = l & 0xf;
		l = l >> 4;

		if (c <= 9)
		{
			buf[--i] = '0' + c;
		}
		else
		{
			buf[--i] = 'a' + (c - 10);
		}
	}
}

/**
 * Copy a four byte long to a four byte buffer.
 */
void pbl_LongToBuf(
	unsigned char* buf,         /** buffer to copy to                 */
	long l                      /** long value to copy                */
)
{
	*buf++ = (unsigned char)((l >> 24));
	*buf++ = (unsigned char)((l >> 16));
	*buf++ = (unsigned char)((l >> 8));
	*buf = (unsigned char)(l);
}

/**
 * Read a four byte long from a four byte buffer.
 *
 * @return long rc: the long value read
 */
long pbl_BufToLong(
	unsigned char* buf        /** the buffer to read from   */
)
{
	unsigned long l = (((unsigned long)(*buf++))) << 24;

	l |= (((unsigned long)(*buf++))) << 16;
	l |= (((unsigned long)(*buf++))) << 8;
	return l | *buf;
}

/**
 * Copy a four byte long to a variable length buffer.
 *
 * @return int rc: the number of bytes used in the buffer
 */
int pbl_LongToVarBuf(unsigned char* buffer, unsigned long value)
{
	if (value <= 0x7f)
	{
		*buffer = (unsigned char)value;
		return 1;
	}
	if (value <= 0x3fff)
	{
		*buffer++ = (unsigned char)(value / 0x100) | 0x80;
		*buffer = (unsigned char)value & 0xff;
		return 2;
	}
	if (value <= 0x1fffff)
	{
		*buffer++ = (unsigned char)(value / 0x10000) | 0x80 | 0x40;
		*buffer++ = (unsigned char)(value / 0x100);
		*buffer = (unsigned char)value & 0xff;
		return 3;
	}
	if (value <= 0x0fffffff)
	{
		*buffer++ = (unsigned char)(value / 0x1000000) | 0x80 | 0x40 | 0x20;
		*buffer++ = (unsigned char)(value / 0x10000);
		*buffer++ = (unsigned char)(value / 0x100);
		*buffer = (unsigned char)value & 0xff;
		return 4;
	}
	*buffer++ = (unsigned char)0xf0;
	pbl_LongToBuf(buffer, value);
	return 5;
}

/**
 * Read a four byte long from a variable length buffer.
 *
 * @return int rc: the number of bytes used in the buffer
 */
int pbl_VarBufToLong(
	unsigned char* buffer,    /** buffer to read from                 */
	unsigned long* value      /** long to read to                     */
)
{
	int c = 0xff & *buffer++;
	int val;

	if (!(c & 0x80))
	{
		*value = c;
		return 1;
	}
	if (!(c & 0x40))
	{
		*value = (c & 0x3f) * 0x100 + (*buffer & 0xff);
		return 2;
	}
	if (!(c & 0x20))
	{
		val = (c & 0x1f) * 0x10000;
		val += ((*buffer++) & 0xff) * 0x100;
		*value = val + ((*buffer) & 0xff);
		return 3;
	}
	if (!(c & 0x10))
	{
		val = (c & 0x0f) * 0x1000000;
		val += ((*buffer++) & 0xff) * 0x10000;
		val += ((*buffer++) & 0xff) * 0x100;
		*value = val + ((*buffer) & 0xff);
		return 4;
	}

	*value = pbl_BufToLong(buffer);
	return 5;
}

/**
 * Find out how many bytes a four byte long would use in a buffer.
 *
 * @return int rc: number of bytes used in buffer
 */
int pbl_LongSize(
	unsigned long value               /** value to check          */
)
{
	if (value <= 0x7f)
	{
		return 1;
	}
	if (value <= 0x3fff)
	{
		return 2;
	}
	if (value <= 0x1fffff)
	{
		return 3;
	}
	if (value <= 0x0fffffff)
	{
		return 4;
	}
	return 5;
}

/**
 * Find out how many bytes a four byte long uses in a buffer.
 *
 * @return int rc: number of bytes used in buffer
 */
int pbl_VarBufSize(
	unsigned char* buffer   /** buffer to check                  */
)
{
	int c = 0xff & *buffer;

	if (!(c & 0x80))
	{
		return 1;
	}
	if (!(c & 0x40))
	{
		return 2;
	}
	if (!(c & 0x20))
	{
		return 3;
	}
	if (!(c & 0x10))
	{
		return 4;
	}
	return 5;
}
