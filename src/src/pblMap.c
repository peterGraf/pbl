/*
 pblMap.c - C implementation of a Map similar to the Java Map.

 Copyright (C) 2010   Peter Graf

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

 $Log: pblMap.c,v $
 Revision 1.18  2021/06/23 14:32:48  peter
 Switch to MIT license

 Revision 1.17  2021/06/12 11:18:26  peter

 */

/*
 * Make sure "strings <exe> | grep Id | sort -u" shows the source file versions
 */
char* pblMap_c_id = "$Id: pblMap.c,v 1.18 2021/06/23 14:32:48 peter Exp $";

#include <stdio.h>
#include <memory.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#include <stdlib.h>

#include "pbl.h"

/*****************************************************************************/
/* #defines                                                                  */
/*****************************************************************************/
#define PBL_MAP_ENTRY_TAG               1
#define PBL_MAP_KEY_TAG                 2

/*
 * Hash value function used for map entries.
 *
 * @return int rc: The hash value of the entry that was passed.
 */
static int pblMapEntryHashValue( /*                                          */
const void *element /**                 Element to calculate hash value for  */
)
{
	PblMapEntry * entry = (PblMapEntry*) element;

	if (!entry || 0 == entry->keyLength)
	{
		return 0;
	}

	if (entry->tag == PBL_MAP_ENTRY_TAG)
	{
		return pblHtHashValue((unsigned char *) entry->buffer, entry->keyLength);
	}
	else
	{
		PblMapKey * key = (PblMapKey*) element;
		return pblHtHashValue((unsigned char *) key->key, key->keyLength);
	}
}

/*
 * Compares two map entries.
 *
 * Used as compare function for maps.
 *
 * @return int rc  < 0: left is smaller than right
 * @return int rc == 0: left and right are equal
 * @return int rc  > 0: left is greater than right
 */
static int pblMapEntryCompareFunction( /*                                    */
const void * left, /*                     The left value for the comparison  */
const void * right /*                     The right value for the comparison */
)
{
	PblMapEntry * leftEntry = *(PblMapEntry**) left;
	PblMapEntry * rightEntry = *(PblMapEntry**) right;

	if (!leftEntry)
	{
		if (rightEntry)
		{
			return -1;
		}
		return 0;
	}
	if (!rightEntry)
	{
		return 1;
	}
	if (leftEntry->tag == PBL_MAP_ENTRY_TAG)
	{
		if (rightEntry->tag == PBL_MAP_ENTRY_TAG)
		{
			return pbl_memcmp(leftEntry->buffer, leftEntry->keyLength, rightEntry->buffer, rightEntry->keyLength);
		}
		else
		{
			PblMapKey * rightKey = *(PblMapKey**) right;

			return pbl_memcmp(leftEntry->buffer, leftEntry->keyLength, rightKey->key, rightKey->keyLength);
		}
	}
	else
	{
		PblMapKey * leftKey = *(PblMapKey**) left;

		if (rightEntry->tag == PBL_MAP_ENTRY_TAG)
		{
			return pbl_memcmp(leftKey->key, leftKey->keyLength, rightEntry->buffer, rightEntry->keyLength);
		}
		else
		{
			PblMapKey * rightKey = *(PblMapKey**) right;

			return pbl_memcmp(leftKey->key, leftKey->keyLength, rightKey->key, rightKey->keyLength);
		}
	}
}

/**
 * Creates a new tree map.
 *
 * This method has a time complexity of O(1).
 *
 * @return pblMap * retPtr != NULL: A pointer to the new map.
 * @return pblMap * retPtr == NULL: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
PblMap * pblMapNewTreeMap(void)
{
	PblMap * pblMap = (PblMap *) pblSetNewTreeSet();
	if (!pblMap)
	{
		return NULL;
	}

	pblSetSetCompareFunction((PblSet *) pblMap, pblMapEntryCompareFunction);
	pblSetSetHashValueFunction((PblSet *) pblMap, pblMapEntryHashValue);

	return pblMap;
}

/**
 * Creates a new hash map.
 *
 * This method has a time complexity of O(1).
 *
 * @return PblMap * retPtr != NULL: A pointer to the new map.
 * @return PblMap * retPtr == NULL: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
PblMap * pblMapNewHashMap(void)
{
	PblMap * pblMap = (PblMap *) pblSetNewHashSet();
	if (!pblMap)
	{
		return NULL;
	}

	pblSetSetCompareFunction((PblSet *) pblMap, pblMapEntryCompareFunction);
	pblSetSetHashValueFunction((PblSet *) pblMap, pblMapEntryHashValue);

	return pblMap;
}

/**
 * Removes all of the mappings from this map. The map will be empty after this call returns.
 *
 * <B>Note:</B> The memory of the entries cleared is freed.
 *
 * For hash maps this method has a time complexity of O(N).
 * For tree maps this method has a time complexity of O(N * Log N).
 *
 * @return void
 */
void pblMapClear( /*                                               */
PblMap * map /**                                  The map to clear */
)
{
	while (pblSetSize((PblSet *) map) > 0)
	{
		void * ptr = pblSetRemove((PblSet *) map);
		PBL_FREE(ptr);
	}
}

/**
 * Removes all of the mappings from this map and frees the map's memory from heap.
 *
 * For hash maps this method has a time complexity of O(N).
 * For tree maps this method has a time complexity of O(N * Log N).
 *
 * @return void
 */
void pblMapFree( /*                                              */
PblMap * map /**                                 The map to free */
)
{
	pblMapClear(map);
	PBL_FREE(map);
}

/**
 * Returns true if this map contains a mapping for the specified string key.
 *
 * More formally, returns true if and only if this map contains a mapping for a key k such that
 * (key==null ? k==null : memcmp( key, k, keyLength ) == 0. (There can be at most one such mapping.)
 *
 * For hash maps his method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return int rc >  0: The map contains a mapping for the specified key.
 * @return int rc == 0: The map did not contain a mapping for the key.
 */
int pblMapContainsKeyStr( /*                                                 */
PblMap * map, /**             The map to check                               */
char * key /**                Key whose presence in this map is to be tested */
)
{
	return pblMapContainsKey(map, key, key ? 1 + strlen(key) : 0);
}

/**
 * Returns true if this map contains a mapping for the specified key.
 *
 * More formally, returns true if and only if this map contains a mapping for a key k such that
 * (key==null ? k==null : memcmp( key, k, keyLength ) == 0. (There can be at most one such mapping.)
 *
 * For hash maps his method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return int rc >  0: The map contains a mapping for the specified key.
 * @return int rc == 0: The map did not contain a mapping for the key.
 */
int pblMapContainsKey( /*                                                    */
PblMap * map, /**             The map to check                               */
void * key, /**               Key whose presence in this map is to be tested */
size_t keyLength /**          Length of the key                              */
)
{
	PblMapKey mapKey;

	mapKey.tag = PBL_MAP_KEY_TAG;
	mapKey.keyLength = keyLength;
	mapKey.key = key;

	return pblSetContains((PblSet *) map, &mapKey);
}

/**
 * Returns true if this map contains a mapping for the specified string value.
 *
 * This method has a time complexity of O(N).
 *
 * @return int rc >  0: The map contains a mapping for the specified value.
 * @return int rc == 0: The map did not contain a mapping for the value.
 * @return int rc <  0:  An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_CONCURRENT_MODIFICATION - The underlying collection was modified concurrently.
 */
int pblMapContainsValueStr( /*                                               */
PblMap * map, /**           The map to check                                 */
char * value /**            Value whose presence in this map is to be tested */
)
{
	return pblMapContainsValue(map, value, value ? 1 + strlen(value) : 0);
}

/**
 * Returns true if this map contains a mapping for the specified value.
 *
 * This method has a time complexity of O(N).
 *
 * @return int rc >  0: The map contains a mapping for the specified value.
 * @return int rc == 0: The map did not contain a mapping for the value.
 * @return int rc <  0:  An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_CONCURRENT_MODIFICATION - The underlying collection was modified concurrently.
 */
int pblMapContainsValue( /*                                                  */
PblMap * map, /**           The map to check                                 */
void * value, /**           Value whose presence in this map is to be tested */
size_t valueLength /**      Length of the value                              */
)
{
	int hasNext;
	void * element;
	PblMapEntry * entry;

	PblIterator iterator;
	pblIteratorInit((PblSet *) map, &iterator);

	while ((hasNext = pblIteratorHasNext(&iterator)) > 0)
	{
		element = pblIteratorNext(&iterator);
		if (element == (void*) -1)
		{
			// Concurrent modification
			//
			return -1;
		}

		entry = (PblMapEntry *) element;
		if (!entry)
		{
			continue;
		}

		if (entry->valueLength != valueLength)
		{
			continue;
		}

		if (0 == valueLength)
		{
			return 1;
		}

		if (0 == memcmp(value, entry->buffer + entry->keyLength, valueLength))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Returns the value to which the specified string key is mapped,
 * or null if this map contains no mapping for the key.
 *
 * More formally, if this map contains a mapping from a key k to a value v such that
 * (key==null ? k==null : memcmp( key, k, keyLength ) == 0,
 * then this method returns v; otherwise it returns null.
 * (There can be at most one such mapping.)
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return void * retptr != NULL: The associated value.
 * @return void * retptr == NULL: There is no associated value.
 */
void * pblMapGetStr( /*                                                   */
PblMap * map, /**            The map to check                             */
char * key /**               Key whose associated value is to be returned */
)
{
	return pblMapGet(map, key, key ? 1 + strlen(key) : 0, NULL);
}

/**
 * Returns the value to which the specified key is mapped,
 * or null if this map contains no mapping for the key.
 *
 * More formally, if this map contains a mapping from a key k to a value v such that
 * (key==null ? k==null : memcmp( key, k, keyLength ) == 0,
 * then this method returns v; otherwise it returns null.
 * (There can be at most one such mapping.)
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return void * retptr != NULL: The associated value.
 * @return void * retptr == NULL: There is no associated value.
 */
void * pblMapGet( /*                                                      */
PblMap * map, /**            The map to check                             */
void * key, /**              Key whose associated value is to be returned */
size_t keyLength, /**        Length of the key                            */
size_t * valueLengthPtr /**  Out: Length of the value returned            */
)
{
	PblMapEntry * mapEntry;
	PblMapKey mapKey;

	mapKey.tag = PBL_MAP_KEY_TAG;
	mapKey.keyLength = keyLength;
	mapKey.key = key;

	mapEntry = (PblMapEntry *) pblSetGetElement((PblSet *) map, &mapKey);
	if (!mapEntry)
	{
		if (valueLengthPtr)
		{
			*valueLengthPtr = 0;
		}
		return NULL;
	}

	if (valueLengthPtr)
	{
		*valueLengthPtr = mapEntry->valueLength;
	}

	return mapEntry->buffer + mapEntry->keyLength;
}

/**
 * Associates the specified string value with the specified string key in this map.
 *
 * If the map previously contained a mapping for the key, the old value is replaced by the specified value.
 * (A map m is said to contain a mapping for a key k if and only if pblMapContainsKey(k) would return true.)
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return int rc >  0: The map did not already contain a mapping for the key.
 * @return int rc == 0: The map did already contain a mapping for the key.
 * @return int rc <  0: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 * <BR>PBL_ERROR_OUT_OF_BOUNDS - Maximum capacity of the hash set exceeded.
 */
int pblMapAddStrStr( /*                                       */
PblMap * map, /**                   The map to add to         */
char * key, /**                     Key to add a mapping for  */
char * value /**                    Value of the new mapping  */
)
{
	return pblMapAdd(map, key, key ? 1 + strlen(key) : 0, value, value ? 1 + strlen(value) : 0);
}

/*
 * Creates a new map entry
 *
 * @return void * retptr != NULL: The new entry.
 * @return void * retptr == NULL: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
static PblMapEntry * pblMapEntryNew(void * key, /** Key to add a mapping for  */
size_t keyLength, /**                               Length of the key         */
void * value, /**                                   Value of the new mapping  */
size_t valueLength /**                              Length of the value       */
)
{
	PblMapEntry * newEntry = (PblMapEntry *) pbl_malloc("pblMapEntryNew",
			sizeof(PblMapEntry) + keyLength + valueLength);
	if (!newEntry)
	{
		return NULL;
	}

	newEntry->tag = PBL_MAP_ENTRY_TAG;
	newEntry->keyLength = keyLength;
	if (keyLength > 0)
	{
		memcpy(newEntry->buffer, key, keyLength);
	}
	newEntry->valueLength = valueLength;
	if (valueLength > 0)
	{
		memcpy(newEntry->buffer + keyLength, value, valueLength);
	}
	return newEntry;
}

/**
 * Associates the specified value with the specified key in this map.
 *
 * If the map previously contained a mapping for the key, the old value is replaced by the specified value.
 * (A map m is said to contain a mapping for a key k if and only if pblMapContainsKey(k) would return true.)
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return int rc >  0: The map did not already contain a mapping for the key.
 * @return int rc == 0: The map did already contain a mapping for the key.
 * @return int rc <  0: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 * <BR>PBL_ERROR_OUT_OF_BOUNDS - Maximum capacity of the hash set exceeded.
 */
int pblMapAdd( /*                                             */
PblMap * map, /**                   The map to add to         */
void * key, /**                     Key to add a mapping for  */
size_t keyLength, /**               Length of the key         */
void * value, /**                   Value of the new mapping  */
size_t valueLength /**              Length of the value       */
)
{
	int rc;
	PblMapEntry * mapEntry;
	PblMapEntry * newEntry = pblMapEntryNew(key, keyLength, value, valueLength);
	if (!newEntry)
	{
		return -1;
	}

	mapEntry = (PblMapEntry *) pblSetReplaceElement((PblSet *) map, newEntry);
	if (mapEntry)
	{
		PBL_FREE(mapEntry);
		return 0;
	}

	rc = pblSetAdd((PblSet *) map, newEntry);
	if (rc < 0)
	{
		PBL_FREE(newEntry);
		return -1;
	}

	return 1;
}

/**
 * Appends the specified value to the association with the specified string key in this map.
 *
 * If the map previously contained a mapping for the key, the new value is appended to the old value.
 * Otherwise, associates the specified string value with the specified string key in this map.
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return int rc >  0: The map did not already contain a mapping for the key.
 * @return int rc == 0: The map did already contain a mapping for the key.
 * @return int rc <  0: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 * <BR>PBL_ERROR_OUT_OF_BOUNDS - Maximum capacity of the hash set exceeded.
 */
int pblMapAppendStrStr( /*                                    */
PblMap * map, /**                   The map to add to         */
char * key, /**                     Key to add a mapping for  */
char * value /**                    Value to append           */
)
{
	char * oldValue = pblMapGetStr(map, key);
	if (!oldValue)
	{
		return pblMapAddStrStr(map, key, value);
	}
	if (!value || !*value)
	{
		return 0;
	}

	char * newValue = pbl_mem2dup( NULL, oldValue, strlen(oldValue), value, strlen(value) + 1);
	int rc = pblMapAddStrStr(map, key, newValue);
	PBL_FREE(newValue)
	return rc;
}

/**
 * Associates the specified string value with the specified string key in this map.
 *
 * If the map previously contained a mapping for the key, the old value is replaced by the specified value.
 * (A map m is said to contain a mapping for a key k if and only if pblMapContainsKey(k) would return true.)
 *
 * Returns the previous value associated with key, NULL if there was no mapping for key
 * or (void*)-1 in case of an error.
 *
 * Note: If a valid pointer to a value is returned, the value returned is
 * malloced on the heap. It is the caller's responsibility to free that memory
 * once it is no longer needed.
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return void * retptr != (void*)-1: The previously associated value.
 * @return void * retptr == (void*)-1: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 * <BR>PBL_ERROR_OUT_OF_BOUNDS - Maximum capacity of the hash set exceeded.
 */
void * pblMapPutStrStr( /*                                             */
PblMap * map, /**                                    The map to add to */
char * key, /**                               Key to add a mapping for */
char * value /**                              Value of the new mapping */
)
{
	return pblMapPut(map, key, key ? 1 + strlen(key) : 0, value, value ? 1 + strlen(value) : 0, NULL);
}

/**
 * Associates the specified value with the specified key in this map.
 *
 * If the map previously contained a mapping for the key, the old value is replaced by the specified value.
 * (A map m is said to contain a mapping for a key k if and only if pblMapContainsKey(k) would return true.)
 *
 * Returns the previous value associated with key, NULL if there was no mapping for key
 * or (void*)-1 in case of an error.
 *
 * Note: If a valid pointer to a value is returned, the value returned is
 * malloced on the heap. It is the caller's responsibility to free that memory
 * once it is no longer needed.
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return void * retptr != (void*)-1: The previously associated value.
 * @return void * retptr == NULL: There was no previously associated value.
 * @return void * retptr == (void*)-1: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 * <BR>PBL_ERROR_OUT_OF_BOUNDS - Maximum capacity of the hash set exceeded.
 */
void * pblMapPut( /*                                                   */
PblMap * map, /**                                    The map to add to */
void * key, /**                               Key to add a mapping for */
size_t keyLength, /**                                Length of the key */
void * value, /**                             Value of the new mapping */
size_t valueLength, /**                            Length of the value */
size_t * valueLengthPtr /**          Out: Length of the value returned */
)
{
	int rc;
	PblMapEntry * mapEntry;
	PblMapEntry * newEntry = pblMapEntryNew(key, keyLength, value, valueLength);
	if (!newEntry)
	{
		return (void*) -1;
	}

	mapEntry = (PblMapEntry *) pblSetReplaceElement((PblSet *) map, newEntry);
	if (mapEntry)
	{
		void * retptr;

		if (mapEntry->valueLength > 0)
		{
			retptr = pbl_memdup("pblMapPut", mapEntry->buffer + mapEntry->keyLength, mapEntry->valueLength);
		}
		else
		{
			retptr = pbl_malloc0("pblMapPut0", 1);
		}
		if (!retptr)
		{
			if (valueLengthPtr)
			{
				*valueLengthPtr = 0;
			}
			pblSetReplaceElement((PblSet *) map, mapEntry);
			PBL_FREE(newEntry);
			return (void*) -1;
		}

		if (valueLengthPtr)
		{
			*valueLengthPtr = mapEntry->valueLength;
		}
		PBL_FREE(mapEntry);
		return retptr;
	}

	if (valueLengthPtr)
	{
		*valueLengthPtr = 0;
	}

	rc = pblSetAdd((PblSet *) map, newEntry);
	if (rc < 0)
	{
		PBL_FREE(newEntry);
		return (void*) -1;
	}

	return NULL;
}

/**
 * Copies all of the mappings from the specified source map to this map.
 * These mappings will replace any mappings that this map had for any
 * of the keys currently in the specified map.
 *
 * For hash maps this method has a time complexity of O(M).
 * For tree maps this method has a time complexity of O(M * Log N).
 * With M being the number of elements in the source map and
 * N being the number of elements in the target map.
 *
 * @return int rc == 0: Ok.
 * @return int rc <  0:  An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_CONCURRENT_MODIFICATION - The source map was modified concurrently.
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
int pblMapPutAll( /*                                            */
PblMap * map, /**                The map to copy the entries to */
PblMap * sourceMap /**         The map to copy the entries from */
)
{
	int hasNext;
	void * element;
	PblMapEntry * entry;

	PblIterator iterator;
	pblIteratorInit((PblSet *) sourceMap, &iterator);

	while ((hasNext = pblIteratorHasNext(&iterator)) > 0)
	{
		element = pblIteratorNext(&iterator);
		if (element == (void*) -1)
		{
			// Concurrent modification
			//
			return -1;
		}

		entry = (PblMapEntry *) element;
		if (!entry)
		{
			continue;
		}

		if (pblMapAdd(map, entry->buffer, entry->keyLength, entry->buffer + entry->keyLength, entry->valueLength) < 0)
		{
			return -1;
		}
	}

	return 0;
}

/**
 * Removes the mapping for this string key from this map if it is present.
 *
 * More formally, if this map contains a mapping from a key k to a value v such that
 * (key==null ? k==null : memcmp( key, k, keyLength ) == 0,
 * that mapping is removed.
 * (The map can contain at most one such mapping.)
 *
 * Returns the value previously associated with key,
 * NULL if there was no mapping for key
 * or (void*)-1 in case of an error.
 *
 * Note: If a valid pointer to a value is returned, the value returned is
 * malloced on the heap. It is the caller's responsibility to free that memory
 * once it is no longer needed.
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return void * retptr != (void*)-1: The previously associated value.
 * @return void * retptr == (void*)-1: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
void * pblMapRemoveStr( /*                                          */
PblMap * map, /**                            The map to remove from */
char * key /**                     Key whose association is removed */
)
{
	return pblMapRemove(map, key, key ? 1 + strlen(key) : 0, NULL);
}

/**
 * Removes the mapping for this key from this map if it is present.
 *
 * More formally, if this map contains a mapping from a key k to a value v such that
 * (key==null ? k==null : memcmp( key, k, keyLength ) == 0,
 * that mapping is removed.
 * (The map can contain at most one such mapping.)
 *
 * Returns the value previously associated with key,
 * NULL if there was no mapping for key
 * or (void*)-1 in case of an error.
 *
 * Note: If a valid pointer to a value is returned, the value returned is
 * malloced on the heap. It is the caller's responsibility to free that memory
 * once it is no longer needed.
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return void * retptr != (void*)-1: The previously associated value.
 * @return void * retptr == (void*)-1: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY - Out of memory.
 */
void * pblMapRemove( /*                                             */
PblMap * map, /**                            The map to remove from */
void * key, /**                    Key whose association is removed */
size_t keyLength, /**                             Length of the key */
size_t * valueLengthPtr /**       Out: Length of the value returned */
)
{
	PblMapEntry * mapEntry;
	void * retptr = NULL;
	PblMapKey mapKey;

	mapKey.tag = PBL_MAP_KEY_TAG;
	mapKey.keyLength = keyLength;
	mapKey.key = key;

	mapEntry = (PblMapEntry *) pblSetGetElement((PblSet *) map, &mapKey);
	if (!mapEntry)
	{
		if (valueLengthPtr)
		{
			*valueLengthPtr = 0;
		}
		return NULL;
	}

	if (mapEntry->valueLength > 0)
	{
		retptr = pbl_memdup("pblMapRemove", mapEntry->buffer + mapEntry->keyLength, mapEntry->valueLength);
	}
	else
	{
		retptr = pbl_malloc0("pblMapRemove0", 1);
	}
	if (!retptr)
	{
		if (valueLengthPtr)
		{
			*valueLengthPtr = 0;
		}
		return (void*) -1;
	}

	if (valueLengthPtr)
	{
		*valueLengthPtr = mapEntry->valueLength;
	}

	pblSetRemoveElement((PblSet *) map, mapEntry);

	PBL_FREE(mapEntry);

	return retptr;
}

/**
 * Unmaps, removes the mapping for this string key from this map if it is present.
 *
 * More formally, if this map contains a mapping from a key k to a value v such that
 * (key==null ? k==null : memcmp( key, k, keyLength ) == 0,
 * that mapping is removed.
 * (The map can contain at most one such mapping.)
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return int rc >  0: The map did not contain a mapping for the key.
 * @return int rc == 0: The map did contain a mapping for the key.
 */
int pblMapUnmapStr( /*                                              */
PblMap * map, /**                            The map to remove from */
char * key /**                     Key whose association is removed */
)
{
	return pblMapUnmap(map, key, key ? 1 + strlen(key) : 0);
}

/**
 * Unmaps, removes the mapping for this key from this map if it is present.
 *
 * More formally, if this map contains a mapping from a key k to a value v such that
 * (key==null ? k==null : memcmp( key, k, keyLength ) == 0,
 * that mapping is removed.
 * (The map can contain at most one such mapping.)
 *
 * For hash maps this method has a time complexity of O(1).
 * For tree maps this method has a time complexity of O(Log N).
 *
 * @return int rc >  0: The map did not contain a mapping for the key.
 * @return int rc == 0: The map did contain a mapping for the key.
 */
int pblMapUnmap( /*                                                 */
PblMap * map, /**                            The map to remove from */
void * key, /**                    Key whose association is removed */
size_t keyLength /**                              Length of the key */
)
{
	PblMapEntry * mapEntry;
	PblMapKey mapKey;

	mapKey.tag = PBL_MAP_KEY_TAG;
	mapKey.keyLength = keyLength;
	mapKey.key = key;

	mapEntry = (PblMapEntry *) pblSetGetElement((PblSet *) map, &mapKey);
	if (!mapEntry)
	{
		return 1;
	}

	pblSetRemoveElement((PblSet *) map, mapEntry);

	PBL_FREE(mapEntry);

	return 0;
}

/**
 * Tests if this map has no elements.
 *
 * This method has a time complexity of O(1).
 *
 * @return int rc != 0: This map has no elements.
 * @return int rc == 0: This map has elements.
 */
int pblMapIsEmpty( /*                               */
PblMap * map /**                    The map to test */
)
{
	return 0 == ((PblSet *) map)->size;
}

/**
 * Returns the number of entries in this map.
 *
 * This method has a time complexity of O(1).
 *
 * @return int rc: The number of entries in this map.
 */
int pblMapSize( /*                              */
PblMap * map /**                 The map to use */
)
{
	return ((PblSet *) map)->size;
}

/**
 * Returns an iterator over the map entries in this map in proper sequence.
 *
 * The iterator starts the iteration at the beginning of the map.
 *
 * The elements returned by the pblIteratorNext() and pblIteratorPrevious() calls to
 * this iterator are of type 'PblMapEntry *'. Use the methods pblMapEntryKeyLength(),
 * pblMapEntryKey(), pblMapEntryValueLength() and pblMapEntryValue() to retrieve
 * the attributes of the map entries.
 *
 * <B>Note</B>: The memory allocated by this method for the iterator returned needs to be released
 *              by calling pblIteratorFree() once the iterator is no longer needed.
 *
 * The iterators returned by the this method are fail-fast:
 * if the map is structurally modified at any time after the iterator is created,
 * in any way, the iterator will return a PBL_ERROR_CONCURRENT_MODIFICATION error.
 *
 * Thus, in the face of concurrent modification,
 * the iterator fails quickly and cleanly,
 * rather than risking arbitrary, non-deterministic
 * behavior at an undetermined time in the future.
 *
 * This method has a time complexity of O(1).
 *
 * @return void * retptr != NULL: The iterator.
 * @return void * retptr == NULL: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY       - Out of memory.
 * <BR>PBL_ERROR_PARAM_COLLECTION    - The parameter map is not of type (PblCollection *).
 */
PblIterator * pblMapIteratorNew( /*                 */
PblMap * map /** The map to create the iterator for */
)
{
	return pblIteratorNew((PblSet *) map);
}

/**
 * Returns a reverse iterator over the elements in this map in proper sequence.
 *
 * The reverse iterator starts the iteration at the end of the map.
 *
 * The elements returned by the pblIteratorNext() and pblIteratorPrevious() calls to
 * this iterator are of type 'PblMapEntry *'. Use the methods pblMapEntryKeyLength(),
 * pblMapEntryKey(), pblMapEntryValueLength() and pblMapEntryValue() to retrieve
 * the attributes of the map entries.
 *
 * <B>Note</B>: The memory allocated by this method for the iterator returned needs to be released
 *              by calling pblIteratorFree() once the iterator is no longer needed.
 *
 * The iterators returned by the this method are fail-fast:
 * if the map is structurally modified at any time after the iterator is created,
 * in any way, the iterator will return a PBL_ERROR_CONCURRENT_MODIFICATION error.
 *
 * Thus, in the face of concurrent modification,
 * the iterator fails quickly and cleanly,
 * rather than risking arbitrary, non-deterministic
 * behavior at an undetermined time in the future.
 *
 * This method has a time complexity of O(1).
 *
 * @return void * retptr != NULL: The iterator.
 * @return void * retptr == NULL: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY       - Out of memory.
 * <BR>PBL_ERROR_PARAM_COLLECTION    - The parameter map is not of type (PblMap *).
 */
PblIterator * pblMapIteratorReverseNew( /*          */
PblMap * map /** The map to create the iterator for */
)
{
	return pblIteratorReverseNew((PblSet *) map);
}

/**
 * Returns the key length of the map entry passed.
 *
 * This method has a time complexity of O(1).
 *
 * @return size_t length: The key length of the map entry.
 */
size_t pblMapEntryKeyLength( /*                   */
PblMapEntry * entry /**                 The entry */
)
{
	return entry->keyLength;
}

/**
 * Returns the value length of the map entry passed.
 *
 * This method has a time complexity of O(1).
 *
 * @return size_t length: The value length of the map entry.
 */
size_t pblMapEntryValueLength( /*                   */
PblMapEntry * entry /**                   The entry */
)
{
	return entry->valueLength;
}

/**
 * Returns a pointer to the key of the map entry passed.
 *
 * This method has a time complexity of O(1).
 *
 * @return void * ptr: The key of the map entry.
 */
void * pblMapEntryKey( /*                         */
PblMapEntry * entry /**                 The entry */
)
{
	return entry->buffer;
}

/**
 * Returns a pointer to the value of the map entry passed.
 *
 * This method has a time complexity of O(1).
 *
 * @return void * ptr: The value of the map entry.
 */
void * pblMapEntryValue( /*                       */
PblMapEntry * entry /**                 The entry */
)
{
	return entry->buffer + entry->keyLength;
}

typedef struct
{

	PblStringBuilder * stringBuilder;
	char * separator;
	char * separator2;

} PblMapContext;

static int pblMapStrAppendToStringBuilder(void * context, int index, void * element)
{
	PblMapContext * mapContext = (PblMapContext *) context;
	PblMapEntry * entry = (PblMapEntry*) element;

	if (entry->keyLength < 1)
	{
		return 0;
	}
	if (index > 0)
	{
		if (pblStringBuilderAppendStr(mapContext->stringBuilder, mapContext->separator) == ((size_t) -1))
		{
			return -1;
		}
	}
	if (pblStringBuilderAppendStr(mapContext->stringBuilder, pblMapEntryKey(entry)) == ((size_t) -1))
	{
		return -1;
	}
	if (pblStringBuilderAppendStr(mapContext->stringBuilder, mapContext->separator2) == ((size_t) -1))
	{
		return -1;
	}
	if (entry->valueLength > 0)
	{
		if (pblStringBuilderAppendStr(mapContext->stringBuilder, pblMapEntryValue(entry)) == ((size_t) -1))
		{
			return -1;
		}
	}
	else
	{
		if (pblStringBuilderAppendStr(mapContext->stringBuilder, "") == ((size_t) -1))
		{
			return -1;
		}
	}
	return 0;
}

/**
 * Appends all entries of a map with strings as keys and values
 * to a string builder.
 *
 * The key value pairs of the elements of the map are separated by separator2
 * and the individual key value pairs are separated by separator from each other.
 *
 * This method has a time and memory complexity of O(N).
 *
 * @return void * retptr != NULL: The string builder with the key value pairs.
 * @return void * retptr == NULL: An error, see pbl_errno:
 *
 * <BR>PBL_ERROR_OUT_OF_MEMORY       - Out of memory.
 */
PblStringBuilder * pblMapStrStrToStringBuilder( /*          */
PblMap * map, /**                            The map to use */
char * separator, /**     Separator between key-value pairs */
char * separator2 /**     Separator between keys and values */
)
{
	PblMapContext context;

	context.separator = separator;
	context.separator2 = separator2;
	context.stringBuilder = pblStringBuilderNew();
	if (context.stringBuilder == NULL)
	{
		return NULL;
	}

	if (pblCollectionAggregate((PblSet *) map, &context, pblMapStrAppendToStringBuilder) != 0)
	{
		pblStringBuilderFree(context.stringBuilder);
		return NULL;
	}
	return context.stringBuilder;
}

