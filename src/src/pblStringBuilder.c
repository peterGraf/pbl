/*
 pblStringBuilder.c - C implementation of a mutable sequence of characters.

 Copyright (C) 2010, 2015   Peter Graf

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

 $Log: pblStringBuilder.c,v $
 Revision 1.9  2021/06/23 14:32:49  peter
 Switch to MIT license

 Revision 1.8  2021/06/12 11:18:27  peter

 */
 /*
  * Make sure "strings <exe> | grep Id | sort -u" shows the source file versions
  */
char* pblStringBuilder_c_id = "$Id: pblStringBuilder.c,v 1.9 2021/06/23 14:32:49 peter Exp $";

#include <stdio.h>
#include <memory.h>
#include <stdarg.h>
#include <errno.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#include <stdlib.h>

#include "pbl.h"

/**
 * Creates a new string builder.
 *
 * This function has a time complexity of O(1).
 *
 * @return PblStringBuilder * retPtr != NULL: A pointer to the new string builder.
 * @return PblStringBuilder * retPtr == NULL: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
PblStringBuilder* pblStringBuilderNew(void)
{
	PblStringBuilder* stringBuilder = (PblStringBuilder*)pbl_malloc0("pblStringBuilderNew",
		sizeof(PblStringBuilder));
	if (!stringBuilder)
	{
		return NULL;
	}

	stringBuilder->list = pblListNewArrayList();
	if (!stringBuilder->list)
	{
		PBL_FREE(stringBuilder);
		return NULL;
	}

	return stringBuilder;
}

/**
 * Removes all of the elements from the string builder.
 *
 * This function has a time complexity of O(N),
 * with N being the number of elements in the string builder.
 *
 * @return void
 */
void pblStringBuilderClear(    /*                                   */
	PblStringBuilder* stringBuilder /** The string builder to clear */
)
{
	while (pblListSize(stringBuilder->list) > 0)
	{
		void* element = pblListRemoveLast(stringBuilder->list);
		if (element != NULL && element != (void*)-1)
		{
			PBL_FREE(element);
		}
	}
	stringBuilder->length = 0;
}

/**
 * Frees the string builder's memory from heap.
 *
 * This function has a time complexity of O(N),
 * with N being the number of elements in the string builder.
 *
 * @return void
 */
void pblStringBuilderFree(    /*                                   */
	PblStringBuilder* stringBuilder /** The string builder to free */
)
{
	pblStringBuilderClear(stringBuilder);
	pblListFree(stringBuilder->list);
	PBL_FREE(stringBuilder);
}

/**
 * Returns the number of elements in the string builder.
 *
 * This function has a time complexity of O(1).
 *
 * @return int rc: The number of elements in the string builder.
 */
int pblStringBuilderSize(    /*                                   */
	PblStringBuilder* stringBuilder /** The string builder to use */
)
{
	return pblListSize(stringBuilder->list);
}

/**
 * Returns the number of characters in the string builder.
 *
 * This function has a time complexity of O(1).
 *
 * @return size_t rc: The number of characters in the string builder.
 */
size_t pblStringBuilderLength(    /*                              */
	PblStringBuilder* stringBuilder /** The string builder to use */
)
{
	return stringBuilder->length;
}

/**
 * Appends a '\0' terminated string to the
 * end of the data of the string builder.
 *
 * This function has a time complexity of O(N),
 * with N being the length of the string to append.
 *
 * @return size_t rc >= 0: The length of the string builder.
 * @return size_t rc == -1: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
size_t pblStringBuilderAppendStr(    /*                             */
	PblStringBuilder* stringBuilder, /** The string builder to use  */
	const char* data /** The data to be added to the string builder */
)
{
	size_t length = 0;
	char* element = NULL;

	if (data)
	{
		length = strlen(data);
		element = (char*)pbl_memdup("pblStringBuilderAppendStr", (void*)data, length + 1);
		if (!element)
		{
			return (size_t)-1;
		}
	}

	if (pblListAdd(stringBuilder->list, element) < 0)
	{
		PBL_FREE(element);
		return (size_t)-1;
	}

	return stringBuilder->length += length;
}

/**
 * Appends at most n bytes of a string to the
 * end of the data of the string builder.
 *
 * This function has a time complexity of O(N),
 * with N being the number of characters to append.
 *
 * @return size_t rc >= 0: The length of the string builder.
 * @return size_t rc == -1: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
size_t pblStringBuilderAppendStrN(    /*                            */
	PblStringBuilder* stringBuilder, /** The string builder to use  */
	size_t n,        /** The maximum number of bytes to append      */
	const char* data /** The data to be added to the string builder */
)
{
	size_t length = 0;
	char* element = NULL;

	if (data && n > 0)
	{
		element = (char*)pbl_malloc("pblStringBuilderAppendStrN", n + 1);
		if (!element)
		{
			return -1;
		}

#ifdef WIN32
		strncpy_s(element, n + 1, data, n);
#else
		strncpy(element, data, n);
#endif
		element[n] = '\0';
		length = strlen(element);
	}

	if (pblListAdd(stringBuilder->list, element) < 0)
	{
		PBL_FREE(element);
		return -1;
	}

	return stringBuilder->length += length;
}

#define _PBL_BUFFER_ON_STACK_SIZE_MAX  4096

/**
 * Appends a variable string defined by the format parameter to the
 * end of the data of the string builder.
 *
 * This function will append at most 4096 bytes of data to the string buffer.
 * If you need to append more bytes, use the function pblStringBuilderAppendN.
 *
 * For the possible values of the format parameter,
 * see the documentation of the printf(3) family of functions.
 *
 * This function has a time complexity of O(N),
 * with N being the length of the string being appended.
 *
 * @return size_t rc >= 0: The length of the string builder.
 * @return size_t rc == -1: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 * <BR>PBL_ERROR_PARAM_FORMAT  - The parameter format is not valid.
 */
size_t pblStringBuilderAppend( /*                                   */
	PblStringBuilder* stringBuilder, /** The string builder to use  */
	const char* format, /** The format of the arguments to append   */
	... /** The variable arguments to append                        */
)
{
	char* tag = "pblStringBuilderAppend";
	size_t size = _PBL_BUFFER_ON_STACK_SIZE_MAX;
	char buffer[_PBL_BUFFER_ON_STACK_SIZE_MAX];
	int rc;
	va_list args;

	if (!format)
	{
		return pblStringBuilderAppendStr(stringBuilder, NULL);
	}

	va_start(args, format);
	rc = vsnprintf(buffer, size, format, args);
	va_end(args);

	if (rc < 0)
	{
#ifdef WIN32
		_snprintf_s(pbl_errstr, PBL_ERRSTR_LEN, PBL_ERRSTR_LEN,
			"%s: vsnprintf of format '%s' failed with errno %d\n", tag, format, errno);
#else
		snprintf(pbl_errstr, PBL_ERRSTR_LEN, "%s: vsnprintf of format '%s' failed with errno %d\n", tag, format, errno);
#endif
		pbl_errno = PBL_ERROR_PARAM_FORMAT;

		return (size_t)-1;
	}

	return pblStringBuilderAppendStr(stringBuilder, buffer);
}

/**
 * Appends at most n bytes of a variable string defined by the format parameter to the
 * end of the data of the string builder.
 *
 * For the possible values of the format parameter,
 * see the documentation of the printf(3) family of functions.
 *
 * This function has a time complexity of O(N),
 * with N being the length of the string being appended.
 *
 * @return size_t rc >= 0: The length of the string builder.
 * @return size_t rc == -1: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 * <BR>PBL_ERROR_PARAM_FORMAT  - The parameter format is not valid.
 */
size_t pblStringBuilderAppendN( /*                                  */
	PblStringBuilder* stringBuilder, /** The string builder to use  */
	size_t n, /** The maximum number of bytes to append             */
	const char* format, /** The format of the arguments to append   */
	... /** The variable arguments to append                        */
)
{
	char* tag = "pblStringBuilderAppendN";
	char* data = NULL;
	int rc;
	size_t size_t_rc;
	va_list args;

	if (!format)
	{
		return pblStringBuilderAppendStr(stringBuilder, NULL);
	}

	if (n < 1)
	{
		return pblStringBuilderAppendStr(stringBuilder, "");
	}

	data = (char*)pbl_malloc(tag, n + 1);
	if (!data)
	{
		return (size_t)-1;
	}

	va_start(args, format);
	rc = vsnprintf(data, n + 1, format, args);
	va_end(args);

	if (rc < 0)
	{
		PBL_FREE(data);

#ifdef WIN32
		_snprintf_s(pbl_errstr, PBL_ERRSTR_LEN, PBL_ERRSTR_LEN,
			"%s: vsnprintf of format '%s' failed with errno %d\n", tag, format, errno);
#else
		snprintf(pbl_errstr, PBL_ERRSTR_LEN, "%s: vsnprintf of format '%s' failed with errno %d\n", tag, format, errno);
#endif
		pbl_errno = PBL_ERROR_PARAM_FORMAT;

		return (size_t)-1;
	}

	data[n] = '\0';
	size_t_rc = pblStringBuilderAppendStr(stringBuilder, data);
	PBL_FREE(data);
	return size_t_rc;
}

/**
 * Converts the string builder's data to a '\0' terminated string.
 *
 * This function has a time complexity of O(N),
 * with N being the length of the data of the string builder.
 *
 * Note: The memory for the data returned is malloced,
 * it is the caller's responsibility to free that memory!
 *
 * @return char * rc != NULL: The data.
 * @return char * rc == NULL: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
char* pblStringBuilderToString( /*                                */
	PblStringBuilder* stringBuilder /** The string builder to use */
)
{
	char* tag = "pblStringBuilderToString";
	int hasNext;
	char* data = NULL;
	char* ptr;
	PblIterator iteratorBuffer;
	PblIterator* iterator = &iteratorBuffer;

	if (stringBuilder->length == 0)
	{
		return (char*)pbl_strdup(tag, "");
	}

	data = (char*)pbl_malloc(tag, stringBuilder->length + 1);
	if (!data)
	{
		return (char*)NULL;
	}
	ptr = data;

	if (pblIteratorInit((PblCollection*)stringBuilder->list, iterator) < 0)
	{
		PBL_FREE(data);
		return (char*)NULL;
	}

	while ((hasNext = pblIteratorHasNext(iterator)) > 0)
	{
		char* element = (char*)pblIteratorNext(iterator);
		if (element == (void*)-1)
		{
			// Concurrent modification
			//
			PBL_FREE(data);
			return (char*)NULL;
		}
		if (element)
		{
			size_t length = strlen(element);
			memcpy(ptr, element, length + 1);
			ptr += length;
		}
	}
	if (hasNext < 0)
	{
		// Concurrent modification
		//
		PBL_FREE(data);
		return (char*)NULL;
	}

	return data;
}
