/*
 pblkf.c - key file library implementation

 Copyright (C) 2002 - 2007     Peter Graf

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

    $Log: pblkf.c,v $
    Revision 1.28  2021/06/23 14:32:49  peter
    Switch to MIT license


*/

/*
 * make sure "strings <exe> | grep Id | sort -u" shows the source file versions
 */
char * pblkf_c_id = "$Id: pblkf.c,v 1.28 2021/06/23 14:32:49 peter Exp $";

#ifndef _WIN32

#include <unistd.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <memory.h>
#include <string.h>


#include "pbl.h"                 /* program base library                      */

/******************************************************************************/
/* #defines                                                                   */
/******************************************************************************/

#ifndef PBL_KF_TEST_DEFINES

#ifndef PBLDATASIZE             /* value might be set by compiler switch      */
#define PBLDATASIZE      4096   /* size of entire block data                  */
#endif

#ifndef PBLBLOCKSPERFILE        /* value might be set by compiler switch      */
#define PBLBLOCKSPERFILE 2048   /* default number of cache blocks per file    */
#endif

#ifndef PBLFILEBLOCKS           /* value might be set by compiler switch      */
#define PBLFILEBLOCKS  0xffff   /* number of blocks per big file  on disk     */
#endif

#ifndef PBL_NFILES              /* value might be set by compiler switch      */
#define PBL_NFILES        256   /* maximum number of open key files           */
#endif

#else /* PBL_TEST_DEFINES */

#define PBLDATASIZE        64   /* size of entire block data                  */
#define PBLBLOCKSPERFILE   10   /* default number of blocks in our cache      */
#define PBLNEXPANDED        5   /* default number of expanded blocks          */
#define PBLFILEBLOCKS       3   /* number of blocks per file                  */

#undef  PBLKEYLENGTH
#define PBLKEYLENGTH        8   /* maximum length of a key                  */

#undef  PBLDATALENGTH
#define PBLDATALENGTH      16   /* maximum length of data that is stored      */
                                /* with an item on the level 0 block          */
                                /* data that is longer is stored on data      */
                                /* blocks                                     */
#ifndef PBL_NFILES
#define PBL_NFILES        256   /* maximum number of open key files           */
#endif

#endif /* PBL_TEST_DEFINES */

#define PBLHEADERSIZE      13   /* offset of first free byte on a new block   */

#define PBLDATABLOCKMAXFREE  ( PBLDATASIZE - PBLHEADERSIZE )
                                /* maximum number of free bytes on a data     */
                                /* block                                      */

#define PBLMAGIC          "1.00 Peter's B Tree"
                                /* during creation this string is written     */
                                /* to the files                               */

/******************************************************************************/
/* #macros                                                                    */
/******************************************************************************/

/*
 * macro to determine free space on an index block
 */
#define PBLINDEXBLOCKNFREE( B ) ((PBLDATASIZE - B->free) - (2 * B->nentries))

/*
 * macro to determine free space on a data block
 */
#define PBLDATABLOCKNFREE( B ) ( PBLDATASIZE - B->free )

/*
 * macros for getting pointers or buffer offsets from the index
 */
#define PBLINDEXTOPTR( B, I ) ( B->data\
                              + ( PBLDATASIZE - 2 * ( 1 + ( I ) )))

#define PBLINDEXTOBUFOFF( B, I ) pbl_BufToShort( PBLINDEXTOPTR( B, I ))

#define PBL_MINIMAL_ITEM_SIZE  5

#define PBL_CACHED_ITEMS_PER_BLOCK ((( PBLDATASIZE - PBLHEADERSIZE ) / PBL_MINIMAL_ITEM_SIZE) + 1 )

/******************************************************************************/
/* typedefs                                                                   */
/******************************************************************************/

/*
 * PBL key file descriptor
 */
typedef struct PBLKFILE_s
{
    char * magic;                /* magic string                              */
    int    bf;                   /* file handle from bigfile handling         */
    int    update;               /* flag: file open for update                */

    long   blockno;              /* of block current record is on             */
    int    index;                /* of current record on this block           */

    long   saveblockno;          /* block number of saved position            */
    int    saveindex;            /* item index of saved position              */

    void * filesettag;           /* file set tag attached to file             */

    int    transactions;         /* number of transactions active for file    */
    int    rollback;             /* next commit should lead to a rollback     */

    void * writeableListHead;    /* head and tail of writeable blocks         */
    void * writeableListTail;

                                 /* a user defined key compare function       */
    int (*keycompare)( void * left, size_t llen, void * right, size_t rlen );

} PBLKFILE_t;

/*
 * Items are the things we store in the data array of the index blocks,
 * while a block is in memory the items of the block are cached for
 * fast data access
 */
typedef struct PBL_CACHED_ITEM_s
{
    unsigned char * key;         /* pointer to the key                        */

    long            datalen;     /* length of the data                        */

    long            datablock;   /* number of block the data is on            */
    long            dataoffset;  /* offset of data on block                   */
    unsigned char   keycommon;   /* number of initial bytes this item has in  */
                                 /* common with its predecessor on tbe block  */

    unsigned char   keylen;      /* length of the key                         */

} PBL_CACHED_ITEM_t;

/*
 * Items are the things we store in the data array of the index blocks
 */
typedef struct PBLITEM_s
{
    unsigned int    level;       /* level where item is inserted              */

    int             keycommon;   /* number of initial bytes this item has in  */
                                 /* common with its predecessor on tbe block  */

    int             keylen;      /* length of the key                         */
    unsigned char * key;         /* pointer to the key                        */

    long            datalen;     /* length of the data                        */
    unsigned char * data;        /* the data of the item                      */

    long            datablock;   /* number of block the data is on            */
    long            dataoffset;  /* offset of data on block                   */

    struct PBLITEM_s * next;     /* we save items in a list                   */
    struct PBLITEM_s * prev;     /* during an insert                          */

} PBLITEM_t;

/*
 * A file block in memory has this format
 */
typedef struct PBLBLOCK_s
{
    unsigned int   level;               /* of block                           */

    long           blockno;             /* block number in file               */
    int            bf;                  /* big file handle block belongs to   */
    void         * filesettag;          /* file set tag attached              */

    unsigned char  data[ PBLDATASIZE ]; /* the real data                      */

    long           nblock;              /* next block of same level           */
    long           pblock;              /* previous block of same level       */
    unsigned int   nentries;            /* number of entries                  */
    int            free;                /* offset of first free byte          */

    long           parentblock;         /* number of parent block             */
    int            parentindex;         /* index on parentblock               */

    int            writeable;           /* block can be written to            */
    int            dirty;               /* has the block been written to ?    */

    unsigned int   maxItemCacheIndex;   /* maximum index of cached item       */
                                        /* pointers to the cached items       */
    PBL_CACHED_ITEM_t * cachedItems[ PBL_CACHED_ITEMS_PER_BLOCK ];

    struct PBLBLOCK_s * next;           /* we keep the blocks we have         */
    struct PBLBLOCK_s * prev;           /* in memory in a LRU chain           */

} PBLBLOCK_t;

/*
 * All file handles of all open filesystem files of
 * all bigfiles are stored in a list
 */
typedef struct PBLBIGFILEHANDLE_s
{
    int    bf;                     /* bigfile handle of file                  */
    int    n;                      /* bigfile index of file                   */

    int    fh;                     /* filesystem handle                       */
    int    mode;                   /* open mode                               */

    struct PBLBIGFILEHANDLE_s * next;
    struct PBLBIGFILEHANDLE_s * prev;

} PBLBIGFILEHANDLE_t;

/*
 * A bigfile in memory
 */
typedef struct PBLBIGFILE_s
{
    char * name;                    /* name of first filesystem file          */

    int    mode;                    /* open mode                              */
    long   blocksperfile;           /* blocks per filesystem file             */
    long   blocksize;               /* size of one block                      */
    long   nextblockno;             /* block number of next free block        */

} PBLBIGFILE_t;

/*
 * References to blocks are stored in a hashtable
 */
typedef struct PBLBLOCKREF_s
{
    long           blockno;             /* block number in file               */
    long           bf;                  /* big file handle block belongs to   */

    PBLBLOCK_t   * block;               /* address of block                   */

} PBLBLOCKREF_t;

typedef struct PBLBLOCKHASHKEY_s
{
    long           blockno;             /* block number in file               */
    long           bf;                  /* big file handle block belongs to   */

} PBLBLOCKHASHKEY_t;

/******************************************************************************/
/* globals                                                                    */
/******************************************************************************/

long pblnreads  = 0;
long pblnwrites = 0;

static char * magic = PBLMAGIC;

/*
 * A pool of open bigfiles
 */
static PBLBIGFILE_t pbf_pool[ PBL_NFILES ];

/*
 * Headers for lists
 */
static PBLBIGFILEHANDLE_t     * pbf_ft_head;
static PBLBIGFILEHANDLE_t     * pbf_ft_tail;
static PBLBLOCK_t             * blockListHead;
static PBLBLOCK_t             * blockListTail;
static PBLITEM_t              * itemListHead;
static PBLITEM_t              * itemListTail;

/*
 * Counters
 */
static int            pblnblocks;
static int            pblblocksperfile = PBLBLOCKSPERFILE;
static long           pblnfiles;

/*
 * Block reference hash table
 */
static pblHashTable_t * pblblockhash;

/******************************************************************************/
/* functions                                                                  */
/******************************************************************************/
/*
 * Verify the consistency of parameters
 */
static int pblParamsCheck(
unsigned char * key,
unsigned int    keylen,
unsigned char * data,
long            datalen
)
{
    if(( keylen < 1 ) || ( keylen > 255 ))
    {
        pbl_errno = PBL_ERROR_PARAM_KEYLEN;
        return -1;
    }

    if( datalen < 0 )
    {
        pbl_errno = PBL_ERROR_PARAM_DATALEN;
        return -1;
    }

    if( !key )
    {
        pbl_errno = PBL_ERROR_PARAM_KEY;
        return -1;
    }

    if( datalen && ( !data ))
    {
        pbl_errno = PBL_ERROR_PARAM_DATA;
        return -1;
    }

    return 0;
}

/*
 * functions on the block reference hash table
 */
/*
------------------------------------------------------------------------------
  FUNCTION:     pblBlockHashInsert

  DESCRIPTION:  Inserts a new block reference into the hash table

  RETURNS:      int rc == 0: a new reference was inserted
                int rc == 1: the reference was already there, it was udpated
                int rc <  0: some error occured, see pbl_errno
                 PBL_ERROR_OUT_OF_MEMORY:        malloc failed, out of memory
------------------------------------------------------------------------------
*/
static int pblBlockHashInsert( long blockno, long bf, PBLBLOCK_t * block )
{
    PBLBLOCKHASHKEY_t   key;
    PBLBLOCKREF_t     * ref;
    int                 rc;

    memset( &key, 0, sizeof( key ));
    key.blockno = blockno;
    key.bf      = bf;

    if( !pblblockhash )
    {
        pblblockhash = pblHtCreate();
        if( !pblblockhash )
        {
            return -1;
        }
    }

    /*
     * See whether we have the reference already
     */
    ref = (PBLBLOCKREF_t *)pblHtLookup( pblblockhash, &key, sizeof( key ));
    if( ref )
    {
        /*
         * The reference is already there, update the block pointer
         */
        ref->block = block;
        return 1;
    }

    if( pbl_errno == PBL_ERROR_NOT_FOUND )
    {
        pbl_errno = 0;
    }

    /*
     * Allocate memory for new reference
     */
    ref = pbl_malloc0( "pblBlockHashInsert BLOCKREF", sizeof( PBLBLOCKREF_t ));
    if( !ref )
    {
        return -1;
    }

    /*
     * Insert to the hash table
     */
    rc = pblHtInsert( pblblockhash, &key, sizeof( key ), ref );
    if( !rc )
    {
        ref->block   = block;
        ref->blockno = blockno;
        ref->bf      = bf;
    }
    else
    {
        PBL_FREE( ref );
        return -1;
    }

    return 0;
}

/*
------------------------------------------------------------------------------
  FUNCTION:     pblBlockHashRemove

  DESCRIPTION:  Remove a block reference from the hash table

  RETURNS:      int rc == 0: call went ok;
                otherwise:   block not found in hash table
------------------------------------------------------------------------------
*/
static int pblBlockHashRemove( long blockno, long bf )
{
    PBLBLOCKHASHKEY_t   key;
    PBLBLOCKREF_t     * ref;
    int                 rc;

    memset( &key, 0, sizeof( key ));
    key.blockno = blockno;
    key.bf      = bf;

    /*
     * If there is no hash table yet, the reference is not found
     */
    if( !pblblockhash )
    {
        return 1;
    }

    /*
     * See whether we have the reference
     */
    ref = pblHtLookup( pblblockhash, &key, sizeof( key ));
    if( !ref )
    {
        if( pbl_errno == PBL_ERROR_NOT_FOUND )
        {
            pbl_errno = 0;
        }

        return 1;
    }

    /*
     * Remove the reference from the hash table
     */
    rc = pblHtRemove( pblblockhash, &key, sizeof( key ));
    if( rc )
    {
        return 1;
    }

    PBL_FREE( ref );

    /*
     * Attempt to remove the hashtable
     */
    rc = pblHtDelete( pblblockhash );
    if( !rc )
    {
        /*
         * The hash table was empty and is deleted now
         */
        pblblockhash = 0;
    }
    else
    {
        /*
         * The hash table was not deleted because it is not
         * empty, clear the error
         */
        pbl_errno = 0;
    }

    return 0;
}

/*
------------------------------------------------------------------------------
  FUNCTION:     pblBlockHashFind

  DESCRIPTION:  Find a block reference in the hash table

  RETURNS:      PBLBLOCK_t * retptr == 0: block not found
                otherwise:                pointer to block in memory
------------------------------------------------------------------------------
*/
static PBLBLOCK_t * pblBlockHashFind( long blockno, long bf )
{
    PBLBLOCKHASHKEY_t   key;
    PBLBLOCKREF_t     * ref;

    memset( &key, 0, sizeof( key ));
    key.blockno = blockno;
    key.bf      = bf;

    /*
     * If there is no hash table yet, the reference is not found
     */
    if( !pblblockhash )
    {
        return NULL;
    }

    /*
     * See whether we have the reference
     */
    ref = pblHtLookup( pblblockhash, &key, sizeof( key ));
    if( !ref )
    {
        if( pbl_errno == PBL_ERROR_NOT_FOUND )
        {
            pbl_errno = 0;
        }

        return NULL;
    }

    return ref->block;
}

/*
 * BIGFILE functions
 */
/*
 * Find a filehandle of a filesystem file
 */
static PBLBIGFILEHANDLE_t * pbf_fh_find( int bf, int n )
{
    PBLBIGFILEHANDLE_t * entry;

    /*
     * File system handles are kept in an LRU list
     */
    for( entry = pbf_ft_head; entry; entry = entry->next )
    {
        if( bf == entry->bf && n == entry->n )
        {
            if( entry != pbf_ft_head )
            {
                PBL_LIST_UNLINK( pbf_ft_head, pbf_ft_tail, entry, next, prev );
                PBL_LIST_PUSH( pbf_ft_head, pbf_ft_tail, entry, next, prev );
            }
            return entry;
        }
    }

    return NULL;
}

/*
 * Close files with the filesystem
 */
static void pbf_fh_close( int bf, int n )
{
    PBLBIGFILEHANDLE_t * entry;
    PBLBIGFILEHANDLE_t * tmp;

    if( n < 0 )
    {
        /*
         * Close all file handles of this big file
         */
        for( entry = pbf_ft_head; entry; )
        {
            tmp = entry;
            entry = entry->next;

            if( bf == tmp->bf )
            {
                close( tmp->fh );
                PBL_LIST_UNLINK( pbf_ft_head, pbf_ft_tail, tmp, next, prev );
                PBL_FREE( tmp );
            }
        }
        return;
    }

    /*
     * Close a particular file handle
     */
    entry = pbf_fh_find( bf, n );
    if( !entry )
    {
        return;
    }

    close( entry->fh );

    PBL_LIST_UNLINK( pbf_ft_head, pbf_ft_tail, entry, next, prev );
    PBL_FREE( entry );
}

/*
 * Create the bigfile path of a file
 * bigfiles are stored in multiple filesystem files.
 * If the first one is called "file.ext",
 * the others have names like "file_0002.ext" etc.
 */
static char * pbf_fh_path( char * name, long n )
{
    char * path;
    char * dotptr;

    if( n < 1 )
    {
        /*
         * The name of the first file does not change
         */
        path = pbl_strdup( "pbf_fh_path first path", name );
        if( !path )
        {
            pbl_errno = PBL_ERROR_OUT_OF_MEMORY;
            return NULL;
        }
        return path;
    }

    path = (char *)pbl_malloc( "pbf_fh_path path", 6 + strlen( name ));
    if( !path )
    {
        return NULL;
    }

    /*
     * See if there is a ".ext" after the last slash
     */
    dotptr = strrchr( name, '.' );
    if( dotptr )
    {
        if( strchr( dotptr, '/' ) || strchr( dotptr, '\\' ))
        {
            dotptr = 0;
        }
    }

    /*
     * Build the filename, start counting at one
     */
    n++;
    if( dotptr )
    {
        memcpy( path, name, dotptr - name );
#ifdef PBL_MS_VS_2012
#pragma warning(disable: 4996)
#endif
        snprintf( path + ( dotptr - name ), 6, "_%04lx", 0xffff & n );
        strcat( path, dotptr );
    }
    else
    {
        strcpy( path, name );
        snprintf( path + strlen( path ), 6, "_%04lx", 0xffff & n );
    }

    return path;
}

/*
 * Open a file with the file system
 */
static int pbf_fh_open( char * name, int mode, int bf, int n )
{
    PBLBIGFILEHANDLE_t * entry;
    int                  fh = -1;
    int                  i;
    char               * path;

    /*
     * Look in LRU list of open filesystem files
     */
    entry = pbf_fh_find( bf, n );
    if( entry )
    {
        if( entry->mode != mode )
        {
            /*
             * the file was found, but the open mode is wrong, close the file
             */
            pbf_fh_close( bf, n );
            entry = 0;
        }
        else
        {
            /*
             * The file is open in the right mode, use the file handle
             */
            return entry->fh;
        }
    }

    /*
     * Open the file
     */
    path = pbf_fh_path( name, n );
    if( !path )
    {
        return -1;
    }

    for( i = 0; i < 3; i++ )
    {
        fh = open( path, mode, S_IREAD | S_IWRITE );
        if( -1 == fh && pbf_ft_tail )
        {
            /*
             * Maybe the process or the system ran out of filehandles,
             * close one file and try again
             */
            pbf_fh_close( pbf_ft_tail->bf, pbf_ft_tail->n );
            continue;
        }

        break;
    }
    PBL_FREE( path );

    if( -1 == fh )
    {
        pbl_errno = PBL_ERROR_OPEN;
        return -1;
    }

    /*
     * Create and link the file handle list entry
     */
    entry = (PBLBIGFILEHANDLE_t *)pbl_malloc0( "pbf_fh_open *entry", sizeof( *entry ));
    if( !entry )
    {
        close( fh );
        return -1;
    }

    entry->fh   = fh;
    entry->mode = mode;
    entry->bf   = bf;
    entry->n    = n;

    PBL_LIST_PUSH( pbf_ft_head, pbf_ft_tail, entry, next, prev );

    return entry->fh;
}

/*
 * Close a bigfile
 */
static int pbf_close( int bf )
{
    if( bf < 0 || bf >= PBL_NFILES )
    {
        return -1;
    }

    /*
     * Close all filesystem files
     */
    pbf_fh_close( bf, -1 );

    PBL_FREE( pbf_pool[ bf ].name );
    pbf_pool[ bf ].name = 0;

    return 0;
}

/*
 * Open a bigfile
 */
static int pbf_open(
char * name,
int    update,
long   blocksperfile,
long   blocksize
)
{
    int        fh = -1;
    int        mode;
    int        i;
    char     * path;

    path = pbf_fh_path( name, 0 );
    if( !path )
    {
        return -1;
    }

    if( update )
    {
        mode = O_CREAT | O_BINARY | O_RDWR;
    }
    else
    {
        mode = O_BINARY | O_RDONLY;
    }

    /*
     * Find a slot in the big file pool that is free
     */
    for( i = 0; i < PBL_NFILES; i++ )
    {
        if( pbf_pool[ i ].name )
        {
            continue;
        }

        /*
         * Reserve the slot
         */
        pbf_pool[ i ].mode = mode;
        pbf_pool[ i ].nextblockno = -1;
        pbf_pool[ i ].blocksperfile = blocksperfile;
        pbf_pool[ i ].blocksize = blocksize;

        /*
         * Open the first filesystem file
         */
        fh = pbf_fh_open( path, mode, i, 0 );
        if( -1 == fh )
        {
            PBL_FREE( path );
            pbl_errno = PBL_ERROR_OPEN;
            return -1;
        }

        pbf_pool[ i ].name = path;
        return i ;
    }

    PBL_FREE( path );
    pbl_errno = PBL_ERROR_OPEN;
    return -1;
}

/*
 * File io for a bigfile
 */
static int pbf_blockio(
int             bf,
int             blockwrite,
long            blockno,
unsigned char * buffer
)
{
    long       rc = -1;
    long       n;
    int        fh;
    int        i;
    int        j;
    long       offset;

    if( bf < 0 || bf >= PBL_NFILES || !pbf_pool[ bf ].name )
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return -1;
    }

    /*
     * Make sure the n'th filesystem file is open
     */
    n = blockno / pbf_pool[ bf ].blocksperfile;

    fh = pbf_fh_open( pbf_pool[ bf ].name, pbf_pool[ bf ].mode, bf, n );
    if( -1 == fh )
    {
        pbl_errno = PBL_ERROR_READ;
        return -1;
    }

    blockno %= pbf_pool[ bf ].blocksperfile;
    offset = blockno * pbf_pool[ bf ].blocksize;

    if( blockwrite )
    {
        /*
         * Try the write more than once if needed
         */
        for( i = 0; i < 3; i++ )
        {
            /*
             * Try the seek more than once if needed
             */
            for( j = 0; j < 3; j++ )
            {
                rc = lseek( fh, offset, SEEK_SET );
                if( offset != rc )
                {
                    continue;
                }
                break;
            }
            if( offset != rc )
            {
                pbl_errno = PBL_ERROR_SEEK;
                return -1;
            }

            rc = write( fh, buffer, (unsigned int) pbf_pool[ bf ].blocksize );
            if( rc != pbf_pool[ bf ].blocksize )
            {
                if( errno == EINTR )
                {
                    continue;
                }
                pbl_errno = PBL_ERROR_WRITE;
                return -1;
            }
            break;
        }
        if( i >= 3 )
        {
            pbl_errno = PBL_ERROR_WRITE;
            return -1;
        }

        pblnwrites++;
    }
    else
    {
        /*
         * Try the read more than once if needed
         */
        for( i = 0; i < 3; i++ )
        {
            /*
             * Try the seek more than once if needed
             */
            for( j = 0; j < 3; j++ )
            {
                rc = lseek( fh, offset, SEEK_SET );
                if( offset != rc )
                {
                    continue;
                }
                break;
            }
            if( offset != rc )
            {
                pbl_errno = PBL_ERROR_SEEK;
                return -1;
            }

            rc = read( fh, buffer, (unsigned int) pbf_pool[ bf ].blocksize );
            if( rc < 0 )
            {
                if( errno == EINTR )
                {
                    continue;
                }
                pbl_errno = PBL_ERROR_READ;
                return -1;
            }
            pblnreads++;

            if( rc != pbf_pool[ bf ].blocksize )
            {
                if( errno == EINTR )
                {
                    continue;
                }
                pbl_errno = PBL_ERROR_BAD_FILE;
                return -1;
            }
            break;
        }
        if( i >= 3 )
        {
            pbl_errno = PBL_ERROR_READ;
            return -1;
        }
    }

    return 0;
}

/*
 * Read a block from a bigfile
 */
static int pbf_blockread( int bf, long blockno, unsigned char * buffer )
{
    int        rc;

    rc = pbf_blockio( bf, 0, blockno, buffer );

    return rc;
}

/*
 * Write a block to a bigfile
 */
static int pbf_blockwrite( int bf, long blockno, unsigned char * buffer )
{
    int        rc;

    rc = pbf_blockio( bf, 1, blockno, buffer );

    return rc;
}

/*
 * Append a block to a bigfile
 */
static long pbf_blockappend( int bf, unsigned char * buffer )
{
    int           fh;
    long          offset;
    int           i;
    int           j;
    long          rc = -1;
    unsigned char c;

    if( bf < 0 || bf >= PBL_NFILES || !pbf_pool[ bf ].name )
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return -1;
    }

    if( pbf_pool[ bf ].nextblockno == 0x3fff )
    {
        rc = -1;
    }

    /*
     * If we don't know the next free block of the file yet
     */
    if( pbf_pool[ bf ].nextblockno < 0 )
    {
        /*
         * find the next free block of a bigfile
         */
        for( i = 0; 1; i++ )
        {
            /*
             * Create and open next filesytem file used for the bigfile
             */
            fh = pbf_fh_open( pbf_pool[ bf ].name, pbf_pool[ bf ].mode, bf, i );
            if( -1 == fh )
            {
                pbl_errno = PBL_ERROR_WRITE;
                return -1;
            }

            /*
             * Reopen the filesystem file read only
             */
            fh = pbf_fh_open( pbf_pool[ bf ].name, O_BINARY | O_RDONLY, bf, i );
            if( -1 == fh )
            {
                pbl_errno = PBL_ERROR_WRITE;
                return -1;
            }

            /*
             * Seek to the end of the file
             */
            offset = pbf_pool[ bf ].blocksperfile * pbf_pool[ bf ].blocksize - 1;

            for( j = 0; j < 3; j++ )
            {
                rc = lseek( fh, offset, SEEK_SET );
                if( offset != rc )
                {
                    if( errno == EINTR )
                    {
                        continue;
                    }
                    rc = -1;
                    break;
                }

                /*
                 * Check whether the block exist, by reading its last byte
                 */
                rc = read( fh, &c, 1 );
                if( rc < 1 )
                {
                    if( errno == EINTR )
                    {
                        continue;
                    }
                }
                break;
            }

            if( rc == 1 )
            {
                /*
                 * We can read the last byte of the block, it is not free
                 */
                continue;
            }

            /*
             * We found the last filesystem file used for the bigfile
             * seek to the end of the file
             */
            for( j = 0; j < 3; j++ )
            {
                rc = lseek( fh, 0, SEEK_END );
                if( rc < 0 )
                {
                    if( errno == EINTR )
                    {
                        continue;
                    }

                    pbl_errno = PBL_ERROR_WRITE;
                    return -1;
                }
                break;
            }

            if( rc % pbf_pool[ bf ].blocksize )
            {
                pbl_errno = PBL_ERROR_BAD_FILE;
                return -1;
            }

            pbf_pool[ bf ].nextblockno = rc / pbf_pool[ bf ].blocksize;
            break;
        }
    }

    /*
     * Append the block to the bigfile
     */
    rc = pbf_blockio( bf, 1, pbf_pool[ bf ].nextblockno, buffer );
    if( rc )
    {
        return rc;
    }

    return pbf_pool[ bf ].nextblockno++;
}

/*
 * ITEM functions
 *
 * Layout of an item in the data of a block with a level greater than 0
 *
 * LENGTH    NAME        SEMANTICS
 *
 * 1        KEYLEN        length of the key of the item
 * 1        KEYCOMMON     number of bytes the key has in common with
 *                        the key of the predecessor of the item on the block
 * VARLONG  DATABLOCK     block number of block this item points to
 * KEYLEN   KEY           the key itself, only the last KEYLEN - KEYCOMMON
 *                        bytes are stored
 *
 * Layout of an item in the data of a block with a level 0 and
 * with the datalen of the item smaller than or equal to PBLDATALENGTH
 *
 * LENGTH    NAME        SEMANTICS
 *
 * 1         KEYLEN       length of the key of the item
 * 1         KEYCOMMON    number of bytes the key has in common with
 *                        the key of the predecessor of the item on the block
 * VARLONG   DATALEN      length of the data stored on the block
 * KEYLEN    KEY          the key itself, only the last KEYLEN - KEYCOMMON
 *                        bytes are stored
 * DATALEN   DATA         the data is stored on the block
 *
 * Layout of an item in the data of a block with a level 0 and
 * with the datalen of the item greater than PBLDATALENGTH
 *
 * LENGTH    NAME        SEMANTICS
 *
 * 1         KEYLEN       length of the key of the item
 * 1         KEYCOMMON    number of bytes the key has in common with
 *                        the key of the predecessor of the item on the block
 * VARLONG   DATALEN      length of the data stored on the datablock
 * KEYLEN    KEY          the key itself, only the last KEYLEN - KEYCOMMON
 *                        bytes are stored
 * VARLONG   DATABLOCK    block number of block data is stored on
 * VARLONG   DATAOFFSET   offset of the data on that block
 *
 * The long values stored for an item, DATALEN, DATABLOCK and DATAOFFSET
 * are stored as variable length buffers, i.e. the number of bytes
 * used in the file for the values depends on their numerical value.
 * See the VarBuf* functions for details.
 *
 * The smallest item of an inner block always has KEYLEN 0, which makes
 * it's key logically smaller than any other possible key of an item.
 *
 * The items stored on a block start at address ( block->data + PBLHEADERSIZE ).
 * The items are always stored immediately one after the other starting at
 * at this address, we leave no space in between.
 *
 * As the keys of the items and therefore the items themselves can have
 * different lengths, we store the relative offsets of the items in the
 * data of a block also in the data of the block. Those relative offsets
 * are stored as two byte unsigned shorts at the end of the data of the block.
 * The relative offsets are called slots in the code below.
 *
 * The slots in the slot array are kept in order.
 *
 * For every block the short block->free gives the relative offset of the first
 * free byte of the block, immediately after the end of the last item stored.
 *
 * Before we append an item we check if there is enough space for the item
 * and its slot between the end of the last item stored and the beginning
 * of the slot array of the items stored. If not, PBL_ERROR_NOFIT is given
 * to the pblinsert procedure which then has to split the block.
 *
 * During deletion we pack the ordered array of the slots
 * and the items themselves by shifting them on the block.
 *
 * The number of bytes the key of an item has in common with the key
 * of the predecessor of the item is stored with each item.
 * This is done in order to minimize the space needed for keys.
 *
 * EG: If a key is "New York" and its predecessor is "New Haven",
 *     only the string "York" is stored for the second key together
 *     with one byte indicating that the key has the first 4 bytes
 *     with its predecessor, the key "New Haven".
 *
 * Whenever the full keys are needed for the items of a block,
 * the keys of the items of the block have to be "expanded" first.
 */

/*
 * The following three static procedures are the only ones that 'know' the
 * layout of an item stored on a block
 */
static void pblItemToBuf( PBLBLOCK_t * block, int bufoff, PBLITEM_t * item )
{
    unsigned char * ptr = &( block->data[ bufoff ] );
    int             bytesToStore = item->keylen - item->keycommon;

    *ptr++ = ( unsigned char ) item->keylen;
    *ptr++ = ( unsigned char ) item->keycommon;

    if( block->level > 0 )
    {
        ptr += pbl_LongToVarBuf( ptr, item->datablock );
    }
    else
    {
        ptr += pbl_LongToVarBuf( ptr, item->datalen );
    }

    if( bytesToStore > 0 )
    {
        unsigned char * from = item->key + item->keycommon;

        /*
         * Make sure we don't copy in place
         */
        if( ptr != from )
        {
            pbl_memlcpy( ptr, PBLKEYLENGTH, from, bytesToStore );
        }
        ptr += bytesToStore;
    }

    /*
     * The block needs to be written to the filesystem
     */
    block->dirty = 1;

    if( block->level > 0 )
    {
        return;
    }

    if( item->datalen <= PBLDATALENGTH )
    {
        if( item->datalen > 0 )
        {
            if( ptr != item->data )
            {
                memcpy( ptr, item->data, item->datalen );
            }
        }
    }
    else
    {
        ptr += pbl_LongToVarBuf( ptr, item->datablock );
        pbl_LongToVarBuf( ptr, item->dataoffset );
    }
}

static PBL_CACHED_ITEM_t * pblBufToCachedItem( PBLBLOCK_t * block, unsigned int itemIndex )
{
    unsigned char * ptr;
    unsigned char * endptr;
    int             bufoff;
    unsigned int    index;
    int             keylen;
    int             keycommon;
    int             keybytesmissing;
    PBL_CACHED_ITEM_t * item;
    unsigned char   keybuffer[ PBLKEYLENGTH + 1];

    if( itemIndex >= PBL_CACHED_ITEMS_PER_BLOCK )
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return NULL;
    }

    /*
     * If the item is already cached, we are done
     */
    item = block->cachedItems[ itemIndex ];
    if( item )
    {
        return item;
    }
    index = itemIndex;

    /*
     * Make sure the offset is in bounds
     */
    bufoff = PBLINDEXTOBUFOFF( block, itemIndex );
    if( bufoff < 0 || bufoff >= (int)sizeof( block->data ))
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return NULL;
    }

    /*
     * Malloc space for the cached item
     */
    item = (PBL_CACHED_ITEM_t *)pbl_malloc0("pblBufToCachedItem item", sizeof(PBL_CACHED_ITEM_t));
    if( !item )
    {
        return NULL;
    }

    endptr = &( block->data[ sizeof( block->data ) ] );
    ptr = &( block->data[ bufoff ] );

    /*
     * Parse the item
     */
    item->keylen = *ptr++;
    keylen = item->keylen & 0xff;

    item->keycommon = *ptr++;
    keycommon = item->keycommon & 0xff;

    keylen -= keycommon;

    if( block->level > 0 )
    {
        ptr += pbl_VarBufToLong( ptr, (unsigned long *)&( item->datablock ));
        item->datalen = 0;
    }
    else
    {
        ptr += pbl_VarBufToLong( ptr, (unsigned long *)&( item->datalen ));
        item->datablock = 0;
    }

    if( ptr >= endptr )
    {
        PBL_FREE( item );
        pbl_errno = PBL_ERROR_BAD_FILE;
        return NULL;
    }

    if( item->keylen > 0 )
    {
        item->key = ptr;
        ptr += keylen;
        if( ptr >= endptr )
        {
            PBL_FREE( item );
            pbl_errno = PBL_ERROR_BAD_FILE;
            return NULL;
        }
    }
    else
    {
        item->key = 0;
    }

    if( block->level > 0 )
    {
        item->dataoffset = 0;

        if( item->keylen < 1 || keycommon < 1)
        {
            /*
             * Store the key on the heap
             */
            if( item->keylen > 0 )
            {
                item->key = pbl_memdup("pblBufToCachedItem key", item->key, item->keylen);
                if( !item->key )
                {
                    PBL_FREE( item );
                    return NULL;
                }
            }
            block->cachedItems[ itemIndex ] = item;
            return item;
        }

        keybytesmissing = keycommon;
        keybuffer[ item->keylen ] = 0;

        if( keylen > 0 )
        {
            /*
             * Copy the key to the key buffer
             */
            memcpy( keybuffer + keybytesmissing, item->key, keylen );
        }
        item->key = keybuffer;

        while( keycommon > 0 )
        {
            if( index-- < 1)
            {
                PBL_FREE( item );
                pbl_errno = PBL_ERROR_BAD_FILE;
                return NULL;
            }
            else
            {
                PBL_CACHED_ITEM_t * prevItem = block->cachedItems[ index ];
                if( prevItem )
                {
                    /*
                     * Take the bytes from the previous cached item
                     */
                    memcpy( keybuffer, prevItem->key, keybytesmissing );
                    break;
                }
            }

            bufoff = PBLINDEXTOBUFOFF( block, index );

            /*
             * Make sure the offset is in bounds
             */
            if( bufoff < 0 || bufoff >= (int)sizeof( block->data ))
            {
                PBL_FREE( item );
                pbl_errno = PBL_ERROR_BAD_FILE;
                return NULL;
            }
            ptr = &( block->data[ bufoff ] );

            /*
             * Parse the item
             */
            keylen = *ptr++;
            if( keylen < 1 )
            {
                PBL_FREE( item );
                pbl_errno = PBL_ERROR_BAD_FILE;
                return NULL;
            }

            keycommon = *ptr++;
            if( keycommon >= keybytesmissing)
            {
                continue;
            }

            keylen = keybytesmissing - keycommon;

            ptr += pbl_VarBufSize( ptr );

            if( keylen > 0 )
            {
                if( ptr + keylen >= endptr )
                {
                    PBL_FREE( item );
                    pbl_errno = PBL_ERROR_BAD_FILE;
                    return NULL;
                }

                keybytesmissing -= keylen;
                memcpy( keybuffer + keybytesmissing, ptr, keylen );
            }
        }

        /*
         * Store the key on the heap
         */
        if( item->keylen > 0 )
        {
            item->key = pbl_memdup("pblBufToCachedItem key", item->key, item->keylen);
            if( !item->key )
            {
                PBL_FREE( item );
                return NULL;
            }
        }
        block->cachedItems[ itemIndex ] = item;
        return item;
    }

    if( item->datalen <= PBLDATALENGTH )
    {
        if( item->datalen > 0 )
        {
            item->dataoffset = ptr - block->data;
            ptr += item->datalen;
            if( ptr >= endptr )
            {
                PBL_FREE( item );
                pbl_errno = PBL_ERROR_BAD_FILE;
                return NULL;
            }
        }
        else
        {
            item->dataoffset = 0;
        }
    }
    else
    {
        ptr += pbl_VarBufToLong( ptr, (unsigned long *)&( item->datablock ));
        pbl_VarBufToLong( ptr, (unsigned long *)&( item->dataoffset ));
    }

    if( ptr >= endptr )
    {
        PBL_FREE( item );
        pbl_errno = PBL_ERROR_BAD_FILE;
        return NULL;
    }

    if( item->keylen < 1 || keycommon < 1)
    {
        /*
         * Store the key on the heap
         */
        if( item->keylen > 0 )
        {
            item->key = pbl_memdup("pblBufToCachedItem key", item->key, item->keylen);
            if( !item->key )
            {
                PBL_FREE( item );
                return NULL;
            }
        }
        block->cachedItems[ itemIndex ] = item;
        return item;
    }

    keybytesmissing = keycommon;
    keybuffer[ item->keylen ] = 0;

    if( keylen > 0 )
    {
        /*
         * Copy the key to the key buffer
         */
        memcpy( keybuffer + keybytesmissing, item->key, keylen );
    }
    item->key = keybuffer;

    while( keycommon > 0)
    {
        if( index-- < 1)
        {
            PBL_FREE( item );
            pbl_errno = PBL_ERROR_BAD_FILE;
            return NULL;
        }
        else
        {
            PBL_CACHED_ITEM_t * prevItem = block->cachedItems[ index ];
            if( prevItem )
            {
                /*
                 * Take the bytes from the previous cached item
                 */
                memcpy( keybuffer, prevItem->key, keybytesmissing );
                break;
            }
        }

        bufoff = PBLINDEXTOBUFOFF( block, index );

        /*
         * Make sure the offset is in bounds
         */
        if( bufoff < 0 || bufoff >= (int)sizeof( block->data ))
        {
            PBL_FREE( item );
            pbl_errno = PBL_ERROR_BAD_FILE;
            return NULL;
        }
        ptr = &( block->data[ bufoff ] );

        /*
         * Parse the item
         */
        keylen = *ptr++;
        if( keylen < 1 )
        {
            PBL_FREE( item );
            pbl_errno = PBL_ERROR_BAD_FILE;
            return NULL;
        }

        keycommon = *ptr++;
        if( keycommon >= keybytesmissing)
        {
            continue;
        }

        keylen = keybytesmissing - keycommon;

        ptr += pbl_VarBufSize( ptr );

        if( keylen > 0 )
        {
            if( ptr + keylen >= endptr )
            {
                PBL_FREE( item );
                pbl_errno = PBL_ERROR_BAD_FILE;
                return NULL;
            }

            keybytesmissing -= keylen;
            memcpy( keybuffer + keybytesmissing, ptr, keylen );
        }
    }

    /*
     * Store the key on the heap
     */
    if( item->keylen > 0 )
    {
        item->key = pbl_memdup("pblBufToCachedItem key", item->key, item->keylen);
        if( !item->key )
        {
            PBL_FREE( item );
            return NULL;
        }
    }
    block->cachedItems[ itemIndex ] = item;
    return item;
}

static int pblBufToItem( PBLBLOCK_t * block, unsigned int index, PBLITEM_t * item )
{
    PBL_CACHED_ITEM_t * cachedItem;

    item->level = block->level;

    /*
     * Get the item to the cache
     */
    cachedItem = pblBufToCachedItem( block, index );
    if( !cachedItem )
    {
        return -1;
    }

    if( index > block->maxItemCacheIndex )
    {
        block->maxItemCacheIndex = index;
    }

    /*
     * Get the values from the cached item
     */
    if( block->level > 0 )
    {
        item->data = NULL;
        item->datablock = cachedItem->datablock;
        item->datalen = cachedItem->datalen;
        item->dataoffset = cachedItem->dataoffset;
    }
    else
    {
        item->datalen = cachedItem->datalen;
        if( item->datalen <= PBLDATALENGTH )
        {
            item->datablock = 0;
            item->dataoffset = 0;

            if( item->datalen > 0 )
            {
                item->data = block->data + cachedItem->dataoffset;
            }
            else
            {
                item->data = NULL;
            }
        }
        else
        {
            item->data = NULL;
            item->datablock = cachedItem->datablock;
            item->dataoffset = cachedItem->dataoffset;
        }
    }

    item->key = cachedItem->key;
    item->keycommon = cachedItem->keycommon & 0xff;
    item->keylen = cachedItem->keylen & 0xff;

    return 0;
}

static int pblBufToItemKey( PBLBLOCK_t * block, unsigned int index, PBLITEM_t * item )
{
    /*
     * Get the item to the cache
     */
    PBL_CACHED_ITEM_t * cachedItem = pblBufToCachedItem( block, index );
    if( !cachedItem )
    {
        return -1;
    }

    if( index > block->maxItemCacheIndex )
    {
        block->maxItemCacheIndex = index;
    }

    item->key = cachedItem->key;
    item->keylen = cachedItem->keylen & 0xff;

    return 0;
}

static int pblItemSize( PBLBLOCK_t * block, PBLITEM_t * item )
{
    int rc = 2;

    if( block->level > 0 )
    {
        rc += pbl_LongSize( item->datablock );
    }
    else
    {
        rc += pbl_LongSize( item->datalen );
    }

    if( item->keylen - item->keycommon > 0 )
    {
        rc += item->keylen - item->keycommon;
    }

    if( block->level > 0 )
    {
        return rc;
    }

    if( item->datalen <= PBLDATALENGTH )
    {
        if( item->datalen > 0 )
        {
            rc += item->datalen;
        }
    }
    else
    {
        rc += pbl_LongSize( item->datablock );
        rc += pbl_LongSize( item->dataoffset );
    }

    return rc;
}

/*
 * Compare two items
 */
static int pblItemCompare( PBLKFILE_t *kf, PBLITEM_t *left, PBLITEM_t *right )
{
    int rc;

    if( kf->keycompare )
    {
        /*
         * There is a specific compare function for the key file
         */
        rc = (*kf->keycompare)( left->key, left->keylen,
                                right->key, right->keylen );
    }
    else
    {
        /*
         * Use the default key compare function
         */
        rc = pbl_memcmp( left->key, left->keylen,
                         right->key, right->keylen );
    }

    return rc;
}

static int pblItemGet( PBLBLOCK_t * block, unsigned int index, PBLITEM_t *item )
{
    /*
     * If there is no item with required index
     */
    if( index >= (unsigned int)block->nentries )
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return -1;
    }

    /*
     * Copy the item with the required index
     */
    if( pblBufToItem( block, index, item ))
    {
        return -1;
    }

    return 0;
}

static int pblItemGetKey( PBLBLOCK_t * block, unsigned int index, PBLITEM_t *item )
{
    /*
     * If there is no item with required index
     */
    if( index >= (unsigned int)block->nentries )
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return -1;
    }

    /*
     * Copy the item with the required index
     */
    if( pblBufToItemKey( block, index, item ))
    {
        return -1;
    }

    return 0;
}

/*
 * Append an item to a block
 */
static int pblItemAppend(
PBLBLOCK_t      * block,
unsigned char   * predkey,
unsigned int      predkeylen,
PBLITEM_t       * item
)
{
    unsigned char * ptr;
    unsigned int    itemsize;

    if( !block->writeable )
    {
        pbl_errno = PBL_ERROR_PROGRAM;
        return -1;
    }

    if( !predkey && block->nentries > 0 )
    {
        pbl_errno = PBL_ERROR_PROGRAM;
        return -1;
    }

    /*
     * Calculate how many bytes the key of the item has in common
     * with the key of the predecessor on the block
     */
    if( predkey && ( predkeylen > 0 ))
    {
        item->keycommon = pbl_memcmplen( predkey, predkeylen,
                                         item->key, item->keylen );
    }
    else
    {
        item->keycommon = 0;
    }

    /*
     * Calculate the size the item needs on the block
     */
    itemsize = pblItemSize( block, item );

    /*
     * Check if the item fits here, the "+ 2" is for the slot!
     */
    if( PBLINDEXBLOCKNFREE( block ) < itemsize + 2 )
    {
        pbl_errno = PBL_ERROR_NOFIT;
        return -1;
    }

    /*
     * Put item to data part of block
     */
    pblItemToBuf( block, block->free, item );

    /*
     * Put the slot to the block
     */
    ptr = PBLINDEXTOPTR( block, block->nentries );
    pbl_ShortToBuf( ptr, block->free );

    block->free     += itemsize;
    block->nentries += 1;

    return 0;
}

/*
 * Delete an item from a block
 */
static int pblItemDelete( PBLBLOCK_t * block, int index )
{
    PBLITEM_t       item;
    int             ntomove;
    unsigned int    i;
    int             offset;
    int             itemoffset;
    unsigned char * ptr;
    unsigned int    itemsize;
    PBL_CACHED_ITEM_t * cachedItem;
    int             itemsMoved = 0;

    /*
     * Read the item to delete
     */
    if( pblItemGet( block, index, &item ))
    {
        return -1;
    }

    /*
     * Calculate the size the item ocupies on the block
     */
    itemsize = pblItemSize( block, &item );

    /*
     * Calculate the number of items that have to be moved
     */
    itemoffset = PBLINDEXTOBUFOFF( block, index );
    ptr = block->data + itemoffset;
    ntomove = block->free - ( itemoffset + itemsize );
    if( ntomove < 0 )
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return -1;
    }

    if( ntomove > 0 )
    {
        /*
         * Move the other items to the left
         */
        memmove( ptr, ptr + itemsize, ntomove );
        itemsMoved = 1;

        /*
         * The slots who's offsets are to the right of the deleted item
         * have to change, because the items where moved
         */
        for( i = 0; i < block->nentries; i++ )
        {
            offset = PBLINDEXTOBUFOFF( block, i );
            if( offset > itemoffset )
            {
                offset -= itemsize;
                ptr = PBLINDEXTOPTR( block, i );
                pbl_ShortToBuf( ptr, offset );
            }
        }
    }

    /*
     * Blank out the bytes deleted
     */
    memset(( block->data + block->free ) - itemsize, 0, itemsize );

    /*
     * There is more free space on the block now
     */
    block->free -= itemsize;

    /*
     * Delete the slot from the slots array
     */
    ptr = PBLINDEXTOPTR( block, block->nentries - 1 );
    if( index < (int)block->nentries - 1 )
    {
        ntomove = ( block->nentries - 1 ) - index;
        memmove( ptr + 2, ptr, ntomove * 2 );
    }

    /*
     * Blank out the last slot
     */
    *ptr++ = 0;
    *ptr   = 0;

    /*
     * There is one less slot on the block
     */
    block->nentries -= 1;

    /*
     * Remove the item from the itemcache
     */
    cachedItem = block->cachedItems[ index ];
    if( cachedItem )
    {
        if( cachedItem->key )
        {
            PBL_FREE( cachedItem->key );
        }
        PBL_FREE( cachedItem );
    }

    for( i = index + 1; i < PBL_CACHED_ITEMS_PER_BLOCK; i++ )
    {
        block->cachedItems[ i - 1 ] = block->cachedItems[ i ];

        if( i > block->maxItemCacheIndex )
        {
            break;
        }
    }
    block->cachedItems[ PBL_CACHED_ITEMS_PER_BLOCK - 1 ] = 0;

    if( block->level == 0 && itemsMoved != 0 )
    {
        /*
         * The cached items that point to items that where moved above
         * need to have their dataoffset adjusted by the size of the deleted item
         */
        for( i = 0; i < PBL_CACHED_ITEMS_PER_BLOCK; i++ )
        {
            if( i > block->maxItemCacheIndex )
            {
                break;
            }

            cachedItem = block->cachedItems[ i ];
            if( !cachedItem )
            {
                continue;
            }

            if( cachedItem->datalen < 1 )
            {
                continue;
            }

            if( cachedItem->datalen > PBLDATALENGTH )
            {
                continue;
            }

            if( cachedItem->dataoffset < itemoffset )
            {
                continue;
            }

            cachedItem->dataoffset -= itemsize;
        }
    }
    block->dirty = 1;
    return 0;
}

/*
 * Insert an item on a block before the item with a given index
 */
static int pblItemInsert( PBLBLOCK_t * block, PBLITEM_t * item, int index )
{
    int             ntomove;
    unsigned char * ptr;
    unsigned int    itemsize;

    /*
     * Calculate the size the item needs on the block
     */
    itemsize = pblItemSize( block, item );

    /*
     * Check if the item fits here, the "+ 2" is for the slot!
     */
    if( PBLINDEXBLOCKNFREE( block ) < itemsize + 2 )
    {
        pbl_errno = PBL_ERROR_NOFIT;
        return -1;
    }

    /*
     * Put item to data part of block
     */
    pblItemToBuf( block, block->free, item );

    /*
     * Move the slots of the items after index
     */
    ptr = PBLINDEXTOPTR( block, block->nentries );
    ntomove = block->nentries - index;
    if( ntomove > 0 )
    {
        memmove( ptr, ptr + 2, 2 * ntomove );
    }

    /*
     * Put the slot to the slots array
     */
    ptr = PBLINDEXTOPTR( block, index );
    pbl_ShortToBuf( ptr, block->free );

    block->free     += itemsize;
    block->nentries += 1;

    /*
     * Move the cached item pointers
     */
    if( ntomove > 0 )
    {
        char * ptr = (char*)&(block->cachedItems[ index ]);
        memmove( ptr + sizeof( PBL_CACHED_ITEM_t *), ptr, ntomove * sizeof( PBL_CACHED_ITEM_t *));
        block->maxItemCacheIndex++;
    }

    block->cachedItems[ index ] = 0;
    block->dirty = 1;
    return 0;
}

/*
 * Remove an item from a block
 */
static int pblItemRemove( PBLBLOCK_t *block, unsigned int index )
{
    PBLITEM_t         peeritem;
    PBLITEM_t         previtem;
    unsigned char     data[ PBLDATALENGTH ];
    unsigned char     savekey[ PBLKEYLENGTH ];
    int               rc;
    int               keycommon;
    int               dodelete = 0;

    memset( &previtem,0, sizeof( previtem));

    if( !block->writeable )
    {
        pbl_errno = PBL_ERROR_PROGRAM;
        return -1;
    }

    /*
     * If there is no item with required index
     */
    if( index >= block->nentries )
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return -1;
    }

    if( index == block->nentries - 1 )
    {
        /*
         * Delete the last item on the block
         */
        rc = pblItemDelete( block, index );
        return rc;
    }

    /*
     * Read the previous item on the block
     */
    if( index > 0 )
    {
        if( pblItemGet( block, index - 1, &previtem ))
        {
            return -1;
        }
    }

    /*
     * Read the next item on the block
     */
    if( pblItemGet( block, index + 1, &peeritem ))
    {
        return -1;
    }

    if(( index > 0 ) && ( previtem.keylen > 0 ))
    {
        keycommon = pbl_memcmplen( previtem.key, previtem.keylen,
                                   peeritem.key, peeritem.keylen );
    }
    else
    {
        keycommon = 0;
    }

    /*
     * If the next item has to change
     */
    if( keycommon != peeritem.keycommon )
    {
        /*
         * Delete and reinsert the next item, because its keycommon changed
         */
        dodelete = 1;

        /*
         * Set the new keycommon value for the next item
         */
        peeritem.keycommon = keycommon;

        /*
         * Save the data of the next item
         */
        if( peeritem.datalen <= PBLDATALENGTH )
        {
            memcpy( data, peeritem.data, peeritem.datalen );
            peeritem.data = data;
        }

        /*
         * Save the key of the next item
         */
        memcpy( savekey, peeritem.key, peeritem.keylen );
        peeritem.key = savekey;

        /*
         * Delete the next item
         */
        rc = pblItemDelete( block, index + 1 );
        if( rc )
        {
            return rc;
        }
    }

    /*
     * Delete the index'th item
     */
    rc = pblItemDelete( block, index );
    if( rc )
    {
        return rc;
    }

    if( dodelete )
    {
        /*
         * Insert the saved item again
         */
        rc = pblItemInsert( block, &peeritem, index );
    }
    return rc;
}

/*
 * Find an item that has a key equal to the search key
 *
 * - uses a binary search to position to an item on a block
 */
static int pblItemFind(
PBLKFILE_t * kf,
PBLBLOCK_t * block,
PBLITEM_t  * item,
int          which
)
{
    int       found = -1;
    int       index = 0;
    int       left  = 0;
    int       right = block->nentries - 1;
    int       rc    = 1;
    PBLITEM_t curitem;

    while( left <= right )
    {
        index = ( left + right ) / 2;

        if( pblItemGetKey( block, index, &curitem ))
        {
            return -1;
        }

        rc = pblItemCompare( kf, &curitem, item );
        if( rc == 0 )
        {
            switch( which )
            {
              case PBLLT:
                right = index - 1;
                break;

              case PBLLE:
              case PBLFI:
                found = index;
                right = index - 1;
                break;

              case PBLEQ:
                found = index;
                return found;

              case PBLLA:
              case PBLGE:
                found = index;
                left  = index + 1;
                break;

              case PBLGT:
                left  = index + 1;
                break;
            }
        }
        else if ( rc < 0 )
        {
            switch( which )
            {
              case PBLLT:
              case PBLLE:
                found = index;
                left  = index + 1;
                break;

              case PBLFI:
              case PBLEQ:
              case PBLLA:
              case PBLGE:
              case PBLGT:
                left  = index + 1;
                break;
            }
        }
        else
        {
            switch( which )
            {
              case PBLLT:
              case PBLLE:
              case PBLFI:
              case PBLEQ:
              case PBLLA:
                right = index - 1;
                break;

              case PBLGE:
              case PBLGT:
                found = index;
                right = index - 1;
                break;
            }
        }
    }

    if( found < 0 )
    {
        pbl_errno = PBL_ERROR_NOT_FOUND;
    }

    return found;
}

static int pblItemAdd( PBLKFILE_t * kf, PBLBLOCK_t * block, PBLITEM_t * item )
{
    PBLITEM_t         previtem;
    PBLITEM_t         peeritem;
    int               rc;
    int               index;
    unsigned char     savekey[ PBLKEYLENGTH ];
    unsigned int      savekeylen;
    unsigned char     data[ PBLDATALENGTH ];
    unsigned int      itemsize;
    int               keycommon;

    if( !block->writeable )
    {
        pbl_errno = PBL_ERROR_PROGRAM;
        return -1;
    }

    if( block->nentries < 1 )
    {
        if( pblItemAppend( block, 0, 0, item ))
        {
            return -1;
        }
        return 0;
    }

    /*
     * Find the first item that is bigger than the one we insert
     */
    index = pblItemFind( kf, block, item, PBLGT );
    if( index < 0 )
    {
        if( pbl_errno != PBL_ERROR_NOT_FOUND )
        {
            return -1;
        }

        /*
         * Append to the block
         */
        pbl_errno = 0;
        index = block->nentries;

        if( pblItemGet( block, index - 1, &peeritem ))
        {
            return -1;
        }

        savekeylen = peeritem.keylen;
        if( savekeylen > 0 )
        {
            pbl_memlcpy( savekey, sizeof( savekey ), peeritem.key, savekeylen );
        }

        if( pblItemAppend( block, savekey, savekeylen, item ))
        {
            return -1;
        }
        return index;
    }

    /*
     * Read the previous item on the block
     */
    if( index > 0 )
    {
        if( pblItemGet( block, index - 1, &previtem ))
        {
            return -1;
        }
    }

    /*
     * Calculate the number of bytes the key of the item has in
     * common with the key of its predecessor
     */
    if(( index > 0 ) && ( previtem.keylen > 0 ))
    {
        item->keycommon = pbl_memcmplen( previtem.key, previtem.keylen,
                                         item->key, item->keylen );
    }
    else
    {
        item->keycommon = 0;
    }

    /*
     * Calculate the size the item needs on the block
     */
    itemsize = pblItemSize( block, item );

    /*
     * Check if the item fits here, the "+ 2" is for the slot!
     */
    if( PBLINDEXBLOCKNFREE( block ) < itemsize + 2 )
    {
        pbl_errno = PBL_ERROR_NOFIT;
        return -1;
    }

    /*
     * Read the next item on the block
     */
    if( pblItemGet( block, index, &peeritem ))
    {
        return -1;
    }

    /*
     * Calculate the number of bytes the key of the next item has in
     * common with the key of the new item
     */
    if( item->keylen > 0 )
    {
        keycommon = pbl_memcmplen( item->key, item->keylen,
                                   peeritem.key, peeritem.keylen );
    }
    else
    {
        keycommon = 0;
    }

    /*
     * If the next item has to change
     */
    if( keycommon != peeritem.keycommon )
    {
        /*
         * Set the new keycommon value for the next item
         */
        peeritem.keycommon = keycommon;

        /*
         * Save the data of that item
         */
        if( peeritem.datalen <= PBLDATALENGTH )
        {
            memcpy( data, peeritem.data, peeritem.datalen );
            peeritem.data = data;
        }

        /*
         * Save the key of the item
         */
        memcpy( savekey, peeritem.key, peeritem.keylen );
        peeritem.key = savekey;

        /*
         * Delete the next item
         */
        rc = pblItemDelete( block, index );
        if( rc )
        {
            return rc;
        }

        /*
         * Insert the saved item again
         */
        rc = pblItemInsert( block, &peeritem, index );
        if( rc )
        {
            return rc;
        }
    }

    /*
     * Insert the new item
     */
    rc = pblItemInsert( block, item, index );
    if( rc )
    {
        return rc;
    }

    return index;
}

/*
 * BLOCK functions
 *
 * The size of a block in the file is PBLDATASIZE
 *
 * The layout is as follows:
 *
 * OFFSET    NAME    SEMANTICS
 *
 * 0        LEVEL    Level of the block in the tree
 *                   if 0:      the block is a leaf node
 *                   if 254:    the block is a free block, can be reused
 *                   if 255:    the block is a data block, not belonging to
 *                              the tree.
 *                   otherwise: block is an inner node of the tree
 *
 * 1  -  4 NOFFSET   Block number of next block of same level, as the root block
 *                   always is the only block of it's level, the block number
 *                   of the last data block is stored here, we use this block
 *                   for appending data if necessary.
 *
 * 5  -  8 POFFEST   File offset of previous block of the same level,
 *                   as the root block always is the only block of it's level,
 *                   the block number of the first free block with level 254
 *                   is stored on the root block
 *
 * 9  - 10 NENTRIES  Number of items stored in the data of the block.
 *                   Always 0 for data blocks.
 *
 * 11 - 12 FREE      Relative offset of first free byte in the data of the block
 *
 * 13 - ( PBLDATASIZE - 1 ) Data area of the block used for storing items on
 *                          tree blocks ( level 0 - 253 ) or used for storing
 *                          the data of the items on data blocks ( level 255 ).
 *
 * The root block of the tree is always stored at file offset 0. The first data
 * block at file offset PBLDATASIZE. There are at least two blocks in a file
 * even if there are no items stored at all.
 *
 * For records with a datalen of less or equal to PBLDATALENGTH characters
 * the data is stored on the level 0 index blocks. For records with
 * data longer than PBLDATALENGTH characters the data is stored on data blocks.
 *
 * This is done in order to keep the height of the tree small and to allow
 * data lengths of more than PBLDATASIZE bytes.
 *
 * Only one data record is stored per data block.
 *
 * Blocks read from disk are cached in an LRU list, references to blocks
 * in that list are kept in a hash table in order to speed up access.
 *
 * Blocks changed during a transaction are kept in the writeable block
 * list. If a cached block that is dirty has to be made writeable a copy
 * of the block is created in the writeable list, if the block is not
 * dirty the block is moved from the cached list to the writeable list
 * without creating a copy.
 *
 * During a transaction blocks from the writeable list take precedence
 * over the copy of the same block from the cached list.
 *
 * During a rollback all blocks from the writeable list are discarded,
 * thus reverting the file to the state before the beginning of the
 * transaction.
 *
 * During a commit all blocks are moved from the writeable list to the
 * cached list.
 */

/*
 * The following two procedures are the only ones that 'know' the layout
 * of a the data of a block in the file
 */
static void pblDataToBlock( PBLBLOCK_t * block )
{
    block->data[ 0 ] = block->level;

    pbl_LongToBuf ( &( block->data[ 1 ]),  block->nblock );
    pbl_LongToBuf ( &( block->data[ 5 ]),  block->pblock );
    pbl_ShortToBuf( &( block->data[ 9 ]),  block->nentries );
    pbl_ShortToBuf( &( block->data[ 11 ]), block->free );
}

static void pblBlockToData( PBLBLOCK_t * block )
{
    block->level = block->data[ 0 ];

    block->nblock   = pbl_BufToLong ( &( block->data[ 1 ]));
    block->pblock   = pbl_BufToLong ( &( block->data[ 5 ]));
    block->nentries = pbl_BufToShort( &( block->data[ 9 ]));
    block->free     = pbl_BufToShort( &( block->data[ 11 ]));
}

/*
 * Release the cached items of a block
 */
static void pblBlockCachedItemsRelease( PBLBLOCK_t * block )
{
    unsigned int i;
    PBL_CACHED_ITEM_t * cachedItem;

    for( i = 0; i < PBL_CACHED_ITEMS_PER_BLOCK; i++ )
    {
        cachedItem = block->cachedItems[ i ];
        if( cachedItem )
        {
            if( cachedItem->key )
            {
                PBL_FREE( cachedItem->key );
            }
            PBL_FREE( cachedItem );
            block->cachedItems[ i ] = 0;
        }

        if( i > block->maxItemCacheIndex )
        {
            break;
        }
    }
    block->maxItemCacheIndex = 0;
}

/*
 * Release all memory blocks of a file
 */
static void pblBlocksRelease( int bf )
{
    PBLBLOCK_t * block;

    /*
     * All blocks that were belonging to the file are marked unused
     */
    for( block = blockListHead; block; block = block->next )
    {
        if( block->bf == bf )
        {
            pblBlockHashRemove( block->blockno, block->bf );
            pblBlockCachedItemsRelease( block );

            block->bf         = -1;
            block->filesettag = NULL;
        }
    }
    return;
}


static int pblBlockWrite( PBLBLOCK_t * block )
{
    long rc;

    /*
     * Prepare the block data for writing
     */
    pblDataToBlock( block );

    /*
     * Write the block to the big file
     */
    rc = pbf_blockwrite( block->bf, block->blockno, block->data );
    if( rc < 0 )
    {
        block->bf         = -1;
        block->filesettag = NULL;
        pbl_errno = PBL_ERROR_WRITE;
        return -1;
    }

    return 0;
}

static int pblBlockFlush( int bf, int release )
{
    PBLBLOCK_t * block;
    PBLBLOCK_t * tmp;

    for( tmp = blockListHead; tmp; )
    {
        /*
         * Move through the list of blocks before handling this one
         */
        block = tmp;
        tmp = tmp->next;

        if( block->bf != bf )
        {
            continue;
        }

        /*
         * If a file set tag is set for the block we flush
         * all blocks having the same tag set
         */
        if( block->dirty && block->filesettag )
        {
            PBLBLOCK_t * b;

            /*
             * Run through all blocks on all files in the set and write them
             */
            for( b = blockListHead; b; b = b->next )
            {
                if( b->dirty && b->bf >= 0
                 && ( block->filesettag == b->filesettag ))
                {
                    if( pblBlockWrite( b ))
                    {
                        pblBlocksRelease( b->bf );
                        break;
                    }
                    else
                    {
                        b->dirty = 0;
                    }
                }
            }
        }

        if( block->dirty )
        {
            if( pblBlockWrite( block ))
            {
                /*
                 * This write always is a rewrite of an existing block
                 * therefore a write error is a strange condition,
                 * we unlink all blocks from the file
                 * most likely the file is inconsistent after that !!!!
                 */
                pblBlocksRelease( bf );
                return -1;
            }
            else
            {
                block->dirty = 0;
            }
        }

        if( release )
        {
            pblBlockHashRemove( block->blockno, block->bf );
            pblBlockCachedItemsRelease( block );

            block->bf         = -1;
            block->filesettag = NULL;

            /*
             * Put the block to the end of the LRU list
             */
            if( block != blockListTail )
            {
                PBL_LIST_UNLINK( blockListHead, blockListTail,
                                 block, next, prev );
                PBL_LIST_APPEND( blockListHead, blockListTail,
                                 block, next, prev );
            }
        }
    }
    return 0;
}

static PBLBLOCK_t * pblBlockGetVictim( PBLKFILE_t * file )
{
    int          rc;
    PBLBLOCK_t * block;

    /*
     * If we have not exceeded the number of blocks we can have at most
     * or of the last block in the LRU chain is dirty and we are updating
     */
    if(( pblnblocks < 8 + ( pblblocksperfile * pblnfiles ) )
     ||( blockListTail && blockListTail->dirty
      && blockListTail->bf != -1 && file->writeableListHead ))
    {
        block = (PBLBLOCK_t *)pbl_malloc0( "pblBlockGetVictim BLOCK", sizeof( PBLBLOCK_t ));
        if( !block )
        {
            return NULL;
        }
        pblnblocks++;
        PBL_LIST_PUSH( blockListHead, blockListTail, block, next, prev );
    }
    else
    {
        /*
         * We reuse the last block in the LRU chain
         */
        if( blockListTail )
        {
            if( blockListTail->bf != -1 )
            {
                /*
                 * Flush the block to disk if it is dirty
                 */
                if( blockListTail->dirty )
                {
                    rc = pblBlockFlush( blockListTail->bf, 0 );
                    if( rc )
                    {
                        return NULL;
                    }
                }

                /*
                 * Remove the reference to the block from the hash table
                 */
                pblBlockHashRemove( blockListTail->blockno, blockListTail->bf );
            }

            if(( block = blockListTail ))
            {
                PBL_LIST_UNLINK( blockListHead, blockListTail,
                                 block, next, prev );
                PBL_LIST_PUSH(   blockListHead, blockListTail,
                                 block, next, prev );
            }
        }
        else
        {
            pbl_errno = PBL_ERROR_PROGRAM;
            return NULL;
        }

        pblBlockCachedItemsRelease( block );
    }

    block->parentblock  = -1;
    block->parentindex  = -1;
    block->dirty        = 0;
    block->bf           = -1;
    block->filesettag   = NULL;

    return block;
}

static PBLBLOCK_t * pblBlockFind( PBLKFILE_t * file, long blockno )
{
    PBLBLOCK_t * block;

    /*
     * Check if the block is in the LRU list of writeable blocks
     */
    for( block = file->writeableListHead; block; block = block->next )
    {
        if(( block->blockno == blockno )
         &&( block->bf      == file->bf ))
        {
            /*
             * The block is already there, make it the first of the LRU chain
             */
            if( block != file->writeableListHead )
            {
                PBL_LIST_UNLINK( file->writeableListHead,
                                 file->writeableListTail, block, next, prev );
                PBL_LIST_PUSH( file->writeableListHead,
                               file->writeableListTail, block, next, prev );
            }

            return block;
        }
    }

    /*
     * Check if the block is the head of the LRU list of blocks cached
     */
    if( blockListHead
     && blockListHead->blockno == blockno
     && blockListHead->bf      == file->bf )
    {
        return blockListHead;
    }

    /*
     * Lookup the block in the LRU list of blocks cached
     */
    block = pblBlockHashFind( blockno, file->bf );
    if( block )
    {
        /*
         * The block is there, make it the first of the LRU chain
         */
        if( block != blockListHead )
        {
            PBL_LIST_UNLINK( blockListHead, blockListTail, block, next, prev );
            PBL_LIST_PUSH( blockListHead, blockListTail, block, next, prev );
        }

        return block;
    }

    return NULL;
}

static PBLBLOCK_t * pblBlockGet( PBLKFILE_t * file, long blockno )
{
    PBLBLOCK_t * block;
    long       rc;

    /*
     * Check if the block is in memory
     */
    block = pblBlockFind( file, blockno );
    if( block )
    {
        return block;
    }

    /*
     * The block is not in memory, we have to load it; get an empty block
     */
    block = pblBlockGetVictim( file );
    if( !block )
    {
        return block;
    }

    /*
     * Read the data from file
     */
    rc = pbf_blockread( file->bf, blockno, block->data );
    if( rc < 0 )
    {
        return NULL;
    }

    /*
     * The block has been read successfully, so it belongs to this file from now on
     */
    pblBlockToData( block );

    block->blockno    = blockno;
    block->bf         = file->bf;
    block->filesettag = file->filesettag;

    /*
     * Insert the reference into the hash table
     */
    if( pblBlockHashInsert( block->blockno, block->bf, block ) )
    {
        pbl_errno = PBL_ERROR_PROGRAM;
        return NULL;
    }
    return block;
}

/*
 * Get a version of the block we can write to to the writeable list of the file
 */
static PBLBLOCK_t * pblBlockGetWriteable( PBLKFILE_t * file, long blockno )
{
    PBLBLOCK_t * newblock;
    PBLBLOCK_t * block;

    /*
     * get the block to memory
     */
    block = pblBlockGet( file, blockno );
    if( !block || block->writeable )
    {
        return block;
    }

    if( !block->dirty )
    {
        /*
         * Move the block over to the writeable list
         */
        pblnblocks--;
        PBL_LIST_UNLINK( blockListHead, blockListTail, block, next, prev );
        pblBlockHashRemove( block->blockno, block->bf );

        block->writeable = 1;
        PBL_LIST_PUSH( file->writeableListHead,
                       file->writeableListTail, block, next, prev );

        return block;
    }

    /*
     * Create a copy of the block in the writeable block list
     */
    newblock = pbl_memdup( "pblBlockGetWriteable block",
                           block, sizeof( *block ) );
    if( !newblock )
    {
        return newblock;
    }

    newblock->writeable = 1;
    PBL_LIST_PUSH( file->writeableListHead, file->writeableListTail,
                   newblock, next, prev );

    /*
     * Clear the references of the block to its cached items
     */
    memset( block->cachedItems, 0, sizeof( block->cachedItems ));
    block->maxItemCacheIndex = 0;

    return newblock;
}

static int pblBlockFree( PBLKFILE_t * file, long blockno )
{
    PBLBLOCK_t * rootblock;
    PBLBLOCK_t * block;
    PBLBLOCK_t * nblock = 0;
    PBLBLOCK_t * pblock = 0;

    /*
     * Get the root block to memory
     */
    rootblock = pblBlockGetWriteable( file, 0 );
    if( !rootblock )
    {
        return -1;
    }

    /*
     * Read the block to be freed
     */
    block = pblBlockGetWriteable( file, blockno );
    if( !block )
    {
        return -1;
    }

    /*
     * Read the previous and next block if they exists
     */
    if( block->nblock )
    {
        nblock = pblBlockGetWriteable( file, block->nblock );
        if( !nblock )
        {
            return -1;
        }
    }

    if( block->pblock )
    {
        pblock = pblBlockGetWriteable( file, block->pblock );
        if( !pblock )
        {
            return -1;
        }
    }

    if( nblock )
    {
        nblock->pblock = block->pblock;
        nblock->dirty = 1;
    }

    if( pblock )
    {
        pblock->nblock = block->nblock;
        pblock->dirty = 1;
    }

    pblBlockCachedItemsRelease( block );

    /*
     * Set the values of the free block
     */
    block->level  = 254;
    block->nblock = rootblock->pblock;

    /*
     * Blocks freed always have their predecessor set to 0
     */
    block->pblock   = 0;
    block->nentries = 0;
    block->free     = PBLHEADERSIZE;           /* offset of first free byte   */

    memset( block->data, 0, PBLDATASIZE );

    block->dirty = 1;

    /*
     * Set the link from the rootblock to the block
     */
    rootblock->pblock = blockno;
    rootblock->dirty  = 1;

    return 0;
}

static int pblBlockConcat(
PBLKFILE_t      * file,
PBLBLOCK_t      * block,
PBLBLOCK_t      * from,
unsigned char   * key,
unsigned int      keylen
)
{
    PBLBLOCK_t      tmpblock;
    unsigned char   predkey[ PBLKEYLENGTH ];
    unsigned int    predkeylen = 0;
    PBLITEM_t       item;
    int             rc;
    unsigned int    i;
    int             nentries;

    if( !block->writeable || !from->writeable )
    {
        pbl_errno = PBL_ERROR_PROGRAM;
        return -1;
    }

    /*
     * Read the last item of the left block
     */
    nentries = block->nentries;
    if( block->nentries > 0 )
    {
        if( pblItemGet( block, block->nentries - 1, &item ))
        {
            return -1;
        }

        predkeylen = item.keylen;
        if( predkeylen )
        {
            pbl_memlcpy( predkey, sizeof( predkey ), item.key, predkeylen );
        }
    }

    /*
     * Create a local copy to concatenate to
     */
    tmpblock = *block;

    /*
     * Copy all items to be merged to the temporary block
     */
    for( i = 0; i < from->nentries; i++ )
    {
        if( pblItemGet( from, i, &item ))
        {
            return -1;
        }
        
        /*
         * The first item can have an empty key, if so we use
         * the key given as parameter
         */
        if( i == 0 && keylen > 0 && item.keylen < 1 )
        {
            item.key = key;
            item.keylen = keylen;
        }
        rc = pblItemAppend( &tmpblock, predkey, predkeylen, &item );
        if( rc )
        {
            if( pbl_errno == PBL_ERROR_NOFIT )
            {
                pbl_errno = 0;
                return 0;
            }

            return rc;
        }

        predkeylen = item.keylen;
        if( predkeylen > 0 )
        {
            pbl_memlcpy( predkey, sizeof( predkey ), item.key, predkeylen );
        }
    }

    /*
     * Copy the values back to our original block
     */
    block->nentries = tmpblock.nentries;
    block->free     = tmpblock.free;
    memcpy( block->data, tmpblock.data, PBLDATASIZE );

    block->dirty = 1;

    /*
     * Change values to the current record if they point to the right block
     */
    if( file->blockno == from->blockno )
    {
        /*
         * Set the current record values to the left block
         */
        file->blockno = block->blockno;
        file->index  += nentries;
    }

    return 1;
}

static long pblBlockAppend(
PBLKFILE_t * file,
int          level,
long         nblock,
long         pblock
)
{
    PBLBLOCK_t      * rootblock = 0;
    PBLBLOCK_t      * block = 0;
    long              freeblockno = 0;

    /*
     * If nblock is 1, we are called to create the first block of a file
     * no need to try to read any block of the file
     */
    if( nblock != 1 )
    {
        /*
         * Get the root block to memory
         */
        rootblock = pblBlockGetWriteable( file, 0 );
        if( !rootblock )
        {
            pbl_errno = PBL_ERROR_BAD_FILE;
            return -1;
        }

        /*
         * Read block number of first free block of file
         */
        freeblockno = rootblock->pblock;
        if( freeblockno )
        {
            /*
             * Read the free block
             */
            block = pblBlockGetWriteable( file, freeblockno );
            if( block )
            {
                if( block->level != 254 )
                {
                    pbl_errno = PBL_ERROR_BAD_FILE;
                    return -1;
                }

                /*
                 * Set the next free block to the rootblock
                 */
                rootblock->pblock = block->nblock;
                rootblock->dirty = 1;
            }
            else
            {
                pbl_errno = PBL_ERROR_BAD_FILE;
                return -1;
            }
        }
    }

    if( !block )
    {
        PBLBLOCK_t newblock;

        /*
         * Init the new block
         */
        memset( &newblock, 0, sizeof( newblock ));

        /*
         * Add a free block
         */
        newblock.level = ( char ) 254 & 0xff;

        /*
         * Init new block
         */
        newblock.nblock = nblock;
        newblock.pblock = pblock;
        newblock.free = PBLHEADERSIZE;

        /*
         * Prepare the new block for writing
         */
        pblDataToBlock( &newblock );

        /*
         * append a new block to the file
         */
        freeblockno = pbf_blockappend( file->bf, newblock.data );
        if( freeblockno < 0 )
        {
            pbl_errno = PBL_ERROR_BAD_FILE;
            return -1;
        }

        /*
         * Read the free block
         */
        block = pblBlockGetWriteable( file, freeblockno );
        if( !block || block->level != 254 )
        {
            pbl_errno = PBL_ERROR_BAD_FILE;
            return -1;
        }
    }

    /*
     * Init the new block
     */
    memset( block->data, 0, PBLDATASIZE );

    block->level      = ( char ) level & 0xff;
    block->nentries   = 0;
    block->nblock     = nblock;
    block->pblock     = pblock;
    block->free       = PBLHEADERSIZE;
    block->dirty      = 1;

    return block->blockno;
}

static int pblBlockMerge(
PBLKFILE_t * file,
long         parentblockno,
int          parentindex,
long         blockno
)
{
    int             merged = 0;
    PBLBLOCK_t    * parentblock;
    PBLBLOCK_t    * block;
    PBLBLOCK_t    * peerblock;
    PBLITEM_t       item;
    int             rc;
    unsigned char   key[ PBLKEYLENGTH ];
    unsigned int    keylen;

    /*
     * Read the parentblock
     */
    parentblock = pblBlockGet( file, parentblockno );
    if( !parentblock )
    {
        /*
         * No error because the parent block might have been split
         */
        pbl_errno = 0;
        return merged;
    }

    /*
     * Check the parentindex because the parentblock might have been
     * split without the child knowing about it
     */
    if( parentindex >= (int)parentblock->nentries )
    {
        return merged;
    }

    /*
     * Read the item pointing to blockno
     */
    if( pblItemGet( parentblock, parentindex, &item ))
    {
        return -1;
    }

    /*
     * Check the pointer to the child, because the parentblock might have been
     * split without the child knowing about it
     */
    if( blockno != item.datablock )
    {
        return merged;
    }

    /*
     * If there is a block to the left
     */
    while( parentindex > 0 )
    {
        /*
         * Check the parentindex because the parentblock might have been
         * split without the child knowing about it
         */
        if( parentindex >= (int)parentblock->nentries )
        {
            return merged;
        }

        /*
         * Read the item pointing to blockno
         */
        if( pblItemGet( parentblock, parentindex, &item ))
        {
            return -1;
        }

        /*
         * Set the pointer to the child
         */
        blockno = item.datablock;

        /*
         * Read the child block
         */
        block = pblBlockGet( file, blockno );
        if( !block )
        {
            return -1;
        }

        /*
         * Read the item pointing to the peer
         */
        if( pblItemGet( parentblock, parentindex - 1, &item ))
        {
            return -1;
        }

        /*
         * Read the peerblock
         */
        peerblock = pblBlockGet( file, item.datablock );
        if( !peerblock )
        {
            return -1;
        }

        /*
         * See how much empty space we have on the two blocks
         */
        rc = PBLINDEXBLOCKNFREE( block ) + PBLINDEXBLOCKNFREE( peerblock );
        if( rc < ( PBLDATASIZE + 6 + PBLKEYLENGTH ))
        {
            /*
             * We do not merge
             */
            break;
        }

        /*
         * Read the child block
         */
        block = pblBlockGetWriteable( file, blockno );
        if( !block )
        {
            return -1;
        }

        /*
         * Read the peerblock
         */
        peerblock = pblBlockGetWriteable( file, item.datablock );
        if( !peerblock )
        {
            return -1;
        }

        /*
         * Read the first key of the right block to merge
         */
        parentblock = pblBlockGetWriteable( file, parentblockno );
        if( !parentblock )
        {
            return -1;
        }

        /*
         * Check the parentindex
         */
        if( parentindex >= (int)parentblock->nentries )
        {
            return merged;
        }

        /*
         * Read the item pointing to blockno
         */
        if( pblItemGet( parentblock, parentindex, &item ))
        {
            return -1;
        }

        if( item.keylen < 1 )
        {
            pbl_errno = PBL_ERROR_BAD_FILE;
            return -1;
        }

        keylen = item.keylen;
        pbl_memlcpy( key, sizeof( key ), item.key, keylen );

        /*
         * Concatenate the two blocks
         */
        rc = pblBlockConcat( file, peerblock, block, key, keylen );
        if( rc < 0 )
        {
            return rc;
        }
        else if( rc == 0 )
        {
            /*
             * We could not merge, break the loop
             */
            break;
        }

        /*
         * The two blocks were merged
         */
        merged += 1;

        /*
         * Free the block
         */
        rc = pblBlockFree( file, blockno );
        if( rc )
        {
            return rc;
        }

        rc = pblItemRemove( parentblock, parentindex );
        if( rc )
        {
            return rc;
        }
    }

    /*
     * If there is a block to the left
     */
    while( parentindex < (int)parentblock->nentries - 1 )
    {
        /*
         * Read the item pointing to blockno
         */
        if( pblItemGet( parentblock, parentindex, &item ))
        {
            return -1;
        }

        /*
         * Set the pointer to the child
         */
        blockno = item.datablock;

        /*
         * Read the child block
         */
        block = pblBlockGet( file, blockno );
        if( !block )
        {
            return -1;
        }

        /*
         * Read the item pointing to the peer
         */
        if( pblItemGet( parentblock, parentindex + 1, &item ))
        {
            return -1;
        }

        /*
         * Read the peerblock
         */
        peerblock = pblBlockGet( file, item.datablock );
        if( !peerblock )
        {
            return -1;
        }

        /*
         * See how much empty space we have on the two blocks
         */
        rc = PBLINDEXBLOCKNFREE( block ) + PBLINDEXBLOCKNFREE( peerblock );
        if( rc < ( PBLDATASIZE + 6 + PBLKEYLENGTH ))
        {
            /*
             * We do not merge
             */
            break;
        }

        /*
         * Read the child block
         */
        block = pblBlockGetWriteable( file, blockno );
        if( !block )
        {
            return -1;
        }

        /*
         * Read the peerblock
         */
        blockno = item.datablock;
        peerblock = pblBlockGetWriteable( file, blockno );
        if( !peerblock )
        {
            return -1;
        }

        /*
         * Read the first key of the right block to merge
         */
        parentblock = pblBlockGetWriteable( file, parentblockno );
        if( !parentblock )
        {
            return -1;
        }

        /*
         * Check the parentindex
         */
        if( parentindex >= (int)parentblock->nentries - 1 )
        {
            return merged;
        }

        /*
         * Read the item pointing to blockno
         */
        if( pblItemGet( parentblock, parentindex + 1, &item ))
        {
            return -1;
        }

        if( item.keylen < 1 )
        {
            pbl_errno = PBL_ERROR_BAD_FILE;
            return -1;
        }

        keylen = item.keylen;
        pbl_memlcpy( key, sizeof( key ), item.key, keylen );

        /*
         * Concatenate the two blocks
         */
        rc = pblBlockConcat( file, block, peerblock, key, keylen );
        if( rc < 0 )
        {
            return rc;
        }
        else if( rc == 0 )
        {
            /*
             * We could not merge, break the loop
             */
            break;
        }

        /*
         * The two blocks were merged
         */
        merged += 1;

        /*
         * Free the block
         */
        rc = pblBlockFree( file, blockno );
        if( rc )
        {
            return rc;
        }

        rc = pblItemRemove( parentblock, parentindex + 1 );
        if( rc )
        {
            return rc;
        }
    }

    return merged;
}

/*
 * Truncate the blocklist to the number of blocks allowed
 */
static int pblBlockListTruncate( void )
{
    PBLBLOCK_t * block;
    int          rc;

    /*
     * Truncate the list of blocks we have in memory
     */
    while( pblnblocks >= 8 + ( pblblocksperfile * pblnfiles ) )
    {
        block = blockListTail;
        if( !block )
        {
            pblnblocks = 0;
            break;
        }

        if( block->bf != -1 )
        {
            if( block->dirty )
            {
                /*
                 * If one block of a file is dirty, all blocks are flushed
                 */
                rc = pblBlockFlush( block->bf, 0 );
                if( rc )
                {
                    return rc;
                }
            }

            pblBlockHashRemove( block->blockno, block->bf );
        }

        PBL_LIST_UNLINK( blockListHead, blockListTail, block, next, prev );

        pblBlockCachedItemsRelease( block );

        PBL_FREE( block );

        pblnblocks--;
    }

    return 0;
}

/**
 * Change the number of cache blocks used per open key file
 *
 * The default number is 64, a memory block uses about 4096 bytes of heap memory
 *
 * @return int rc: the number of blocks used after the call
 *
 */

int pblKfInit(
int nblocks            /* number of blocks used per open file */
)
{
    pbl_errno = 0;

    if( nblocks < 1 )
    {
        return pblnblocks;
    }

    if( nblocks < 8 )
    {
        nblocks = 8;
    }

    pblblocksperfile = nblocks;

    return pblblocksperfile;
}

/*
 * FILE functions
 */
static long pblDataAppend(
PBLKFILE_t * file,
unsigned char * data,
long         datalen,
long       * offset
)
{
    long blockno;
    long returnoffset;
    long returnblock;
    long diff;
    long bytesWritten = 0;
    int  nbytes;
    PBLBLOCK_t * rootblock = 0;
    PBLBLOCK_t * datablock = 0;

    rootblock = pblBlockGet( file, 0 );
    if( !rootblock )
    {
        return -1;
    }

    /*
     * rootblock->nblock always contains the number of the last datablock
     */
    datablock = pblBlockGetWriteable( file, rootblock->nblock );
    if( !datablock )
    {
        return -1;
    }

    if( datablock->level != 255 )
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return -1;
    }

    if( PBLDATABLOCKNFREE( datablock ) > PBLDATABLOCKMAXFREE)
    {
        pbl_errno = PBL_ERROR_BAD_FILE;
        return -1;
    }

    /*
     * Since version 1.04, only one data record is stored on a data block,
     * if there is some data on the block already, a new empty block
     * is appended
     */
    if( PBLDATABLOCKNFREE( datablock ) < PBLDATABLOCKMAXFREE)
    {
        /*
         * Make a new data block
         */
        blockno = pblBlockAppend( file, -1, 0, datablock->blockno );
        if( blockno < 0 )
        {
            return -1;
        }

        datablock->nblock = blockno;
        datablock->dirty  = 1;

        /*
         * Get the new datablock to memory
         */
        datablock = pblBlockGetWriteable( file, blockno );
        if( !datablock )
        {
            return -1;
        }

        /*
         * Set address to rootblock
         */
        rootblock = pblBlockGetWriteable( file, 0 );
        if( !rootblock )
        {
            return -1;
        }

        rootblock->nblock = blockno;
        rootblock->dirty  = 1;
    }

    returnoffset = datablock->free;
    returnblock  = datablock->blockno;

    while( bytesWritten < datalen )
    {
        diff = datalen - bytesWritten;
        if( diff > PBLDATABLOCKNFREE( datablock ))
        {
            nbytes = PBLDATABLOCKNFREE( datablock );
        }
        else
        {
            nbytes = (( int )( diff % PBLDATASIZE));
        }

        if( nbytes > 0 )
        {
            memcpy((void *) &(datablock->data[ datablock->free ]),
                   (void *) data,
                   nbytes );
            datablock->dirty = 1;
        }

        bytesWritten    += nbytes;
        data            += nbytes;
        datablock->free += nbytes;

        if( PBLDATABLOCKNFREE( datablock ) < 1 )
        {
            /*
             * Make a new data block
             */
            blockno = pblBlockAppend( file, -1, 0, datablock->blockno );
            if( blockno < 0 )
            {
                return -1;
            }

            datablock->nblock = blockno;
            datablock->dirty  = 1;

            /*
             * Get the new datablock to memory
             */
            datablock = pblBlockGetWriteable( file, blockno );
            if( !datablock )
            {
                return -1;
            }

            /*
             * Set address to rootblock
             */
            rootblock = pblBlockGetWriteable( file, 0 );
            if( !rootblock )
            {
                return -1;
            }

            rootblock->nblock = blockno;
            rootblock->dirty  = 1;
        }
    }

    *offset = returnoffset;

    return returnblock;
}

/*
 * Remove a data record from a key file.
 *
 * Only removes the data record if it is the only data record
 * on the data block.
 *
 * Since 1.04, 4 Nov 2008 23:02:03 CET
 */
static long pblDataRemove(
PBLKFILE_t    * file,
long         blockno,
long         blockoffset,
long         datalen
)
{
    long         diff;
    long         bytesRemoved = 0;
    int          nbytes;
    PBLBLOCK_t * datablock = (PBLBLOCK_t *) 0;
    long         nextBlockNo = 0;
    long         prevBlockNo = -1;

    while( bytesRemoved < datalen )
    {
        datablock = pblBlockGetWriteable( file, blockno );
        if( !datablock )
        {
            return -1;
        }
        nextBlockNo = datablock->nblock;

        if( prevBlockNo == -1 )
        {
            prevBlockNo = datablock->pblock;
        }

        if( datablock->level != 255 )
        {
            pbl_errno = PBL_ERROR_BAD_FILE;
            return -1;
        }

        /*
         * Only data records that start at the beginning
         * of a block are removed
         */
        if( blockoffset != PBLHEADERSIZE)
        {
            return 0;
        }

        diff = datalen - bytesRemoved;
        if( diff > ( PBLDATASIZE - blockoffset ) )
        {
            nbytes = PBLDATASIZE - blockoffset;
        }
        else
        {
            nbytes = ((int ) ( diff % PBLDATASIZE));
        }

        if( nbytes > 0 )
        {
            if( nbytes != PBLDATABLOCKMAXFREE - PBLDATABLOCKNFREE( datablock ))
            {
                return 0;
            }

            if( pblBlockFree(file, datablock->blockno))
            {
                return -1;
            }
            else
            {
                /*
                 * If the last of the data blocks is freed,
                 * we also need to update the root block
                 */
                PBLBLOCK_t * rootblock = pblBlockGetWriteable( file, 0 );
                if( !rootblock )
                {
                    return -1;
                }

                if( rootblock->nblock == blockno)
                {
                    rootblock->nblock = prevBlockNo;
                    rootblock->dirty = 1;
                }
            }

            bytesRemoved += nbytes;
        }

        if( bytesRemoved < datalen )
        {
            /*
             * Set offset of next block and set blockoffset to beginning of
             * data
             */
            blockno = nextBlockNo;
            if( blockno < 1 )
            {
                pbl_errno = PBL_ERROR_BAD_FILE;
                return -1;
            }

            blockoffset = PBLHEADERSIZE;
        }
    }

    return bytesRemoved;
}

/**
 * Create a key file with the name specified by path.
 *
 * A file set tag can be attached to the file,
 * if a file having a non NULL file set tag is flushed
 * to disk all files having the same file set tag attached
 * are flushed as well.
 *
 * <P>
 * <B>RESTRICTIONS</B>:
 * <BR> - the file to create must not exists.
 * <BR> - the current record of the file will not be set
 *
 * @return  pblKeyFile_t * retptr == NULL: an error occured, see pbl_errno
 * @return  pblKeyFile_t * retptr != NULL: a pointer to a key file descriptor
 */

pblKeyFile_t * pblKfCreate(
char * path,       /** path of file to create                                 */
void * filesettag  /** file set tag, for flushing multiple files consistently */
)
{
    pblKeyFile_t * k         = NULL;
    PBLKFILE_t   * kf        = NULL;
    PBLBLOCK_t   * rootblock = NULL;
    PBLITEM_t      item;
    int            fh;
    int            bf;
    long           blockno;
    int            rc;

    pbl_errno = 0;

    /*
     * Make sure we have one filehandle for the create, close one file
     */
    if( pbf_ft_tail )
    {
        pbf_fh_close( pbf_ft_tail->bf, pbf_ft_tail->n );
    }

    /*
     * Do a exclusive create open, make sure the file does not exist yet
     */
    fh = open( path, O_CREAT | O_EXCL | O_BINARY | O_RDWR, S_IREAD | S_IWRITE );
    if( -1 == fh )
    {
        pbl_errno = PBL_ERROR_CREATE;
        return NULL;
    }
    close( fh );

    /*
     * Open the file
     */
    bf = pbf_open( path, 1, PBLFILEBLOCKS, PBLDATASIZE );
    if( bf < 0 )
    {
        return NULL;
    }

    /*
     * Get and init the file structure
     */
    kf = (PBLKFILE_t *)pbl_malloc0( "pblKfCreate FILE", sizeof( PBLKFILE_t ));
    if( !kf )
    {
        goto errout;
    }

    /*
     * We have a key file, set the return value
     */
    k              = ( pblKeyFile_t * )kf;
    kf->magic      = pblkf_c_id;
    kf->bf         = bf;
    kf->update     = 1;
    kf->filesettag = filesettag;

    /*
     * Start a transaction on the file
     */
    pblKfStartTransaction( k );

    /*
     * Make the root block, next offset is first data block
     */
    blockno = pblBlockAppend( kf, 0, 1, 0 );
    if( blockno < 0 )
    {
        goto errout;
    }

    rootblock = pblBlockGet( kf, 0 );
    if( !rootblock )
    {
        goto errout;
    }

    /*
     * Make the first data block
     */
    blockno = pblBlockAppend( kf, -1, 0, 0 );
    if( blockno != 1 )
    {
        goto errout;
    }

    /*
     * Init the first item we insert into each file
     */
    item.level     = 0;
    item.key       = ( unsigned char * ) 0;
    item.keylen    = 0;
    item.keycommon = 0;
    item.datalen   = strlen( magic ) + 1;
    item.data      = (unsigned char *)magic;

    /*
     * Append the magic string as first data item
     */
    if( item.datalen > PBLDATALENGTH )
    {
        item.datablock = pblDataAppend( kf, item.data,
                                        item.datalen, &item.dataoffset );
        if( item.datablock < 1 )
        {
            goto errout;
        }
        item.data = 0;
    }

    /*
     * Insert the first item into the root block
     */
    rootblock = pblBlockGetWriteable( kf, 0 );
    if( !rootblock )
    {
        goto errout;
    }

    rc = pblItemAdd( kf, rootblock, &item );
    if( rc )
    {
        goto errout;
    }

    /*
     * No current record yet
     */
    kf->blockno = -1;
    kf->index   = -1;

    /*
     * Commit the changes
     */
    if( pblKfCommit( k, 0 ))
    {
        goto errout;
    }

    if( pblBlockFlush( kf->bf, 0 ))
    {
        goto errout;
    }

    pblnfiles++;

    return k;

errout:

    if( kf )
    {
        if( kf->bf >= 0 )
        {
            pblKfCommit( k, 1 );
        }
        PBL_FREE( kf );
    }

    if( -1 != bf )
    {
        pblBlocksRelease( bf );
        close( fh );
        unlink( path );
    }

    return NULL;
}

/**
 * Open an existing key file.
 *
 * If update is 0, the file is opened for read access only,
 * if update is not 0 the file is opened for reading and writing
 *
 * A file set tag can be attached to the file,
 * if a file having a non NULL file set tag is flushed
 * to disk all files having the same file set tag attached
 * are flushed as well.
 *
 * <P>
 * <B>RESTRICTIONS</B>:
 * <BR> - the file must exist already
 * <BR> - the current record of the file will not be set
 *
 * @return  pblKeyFile_t * retptr == NULL: an error occured, see pbl_errno
 * @return  pblKeyFile_t * retptr != NULL: a pointer to a key file descriptor
 */

pblKeyFile_t * pblKfOpen(
char * path,       /** path of file to create                                 */
int    update,     /** flag: should file be opened for update?                */
void * filesettag  /** file set tag, for flushing multiple files consistently */
)
{
    PBLKFILE_t  * kf;
    PBLBLOCK_t  * datablock;
    int           bf;

    pbl_errno = 0;

    bf = pbf_open( path, update, PBLFILEBLOCKS, PBLDATASIZE );
    if( -1 == bf )
    {
        return NULL;
    }

    /*
     * Get and init the file structure
     */
    kf = (PBLKFILE_t *)pbl_malloc0( "pblKfOpen FILE", sizeof( PBLKFILE_t ));
    if( !kf )
    {
        pbf_close( bf );
        return NULL;
    }
    kf->magic      = pblkf_c_id;
    kf->bf         = bf;
    kf->update     = update;
    kf->filesettag = filesettag;
    kf->blockno    = -1;
    kf->index      = -1;

    /*
     * Read and check the first datablock
     */
    datablock = pblBlockGet( kf, 1 );
    if( !datablock || ( datablock->level != 255 ))
    {
        pblBlocksRelease( bf );
        pbf_close( bf );
        PBL_FREE( kf );
        pbl_errno = PBL_ERROR_BAD_FILE;
        return NULL;
    }

    pblnfiles++;

    return (pblKeyFile_t *) kf;
}

/**
 * Close a key file
 *
 * All changes are flushed to disk before,
 * all memory allocated for the file is released.
 *
 * @return int rc == 0: call went ok, file is closed
 * @return int rc != 0: some error, see pbl_errno
 */

int pblKfClose(
pblKeyFile_t * k   /** key file to close */
)
{
    PBLKFILE_t * kf = ( PBLKFILE_t * ) k;
    int rc = 0;

    pbl_errno = 0;

    if( kf->update )
    {
        rc = pblBlockFlush( kf->bf, 1 );
    }
    else
    {
        pblBlocksRelease( kf->bf );
    }

    pbf_close( kf->bf );
    PBL_FREE( kf );

    pblnfiles--;

    if( pblBlockListTruncate())
    {
        rc = -1;
    }

    return rc;
}

/**
 * Set an application specific compare function for the keys of a key file
 *
 * An application specific compare function can be used in order to
 * implement special orderings of the values of an index, e.g.
 * because of the use of european "umlauts" in names
 *
 * The default compare function is the c-library memcmp function,
 * the key compare function should behave like memcmp
 *
 * @return void
 */

void pblKfSetCompareFunction(
pblKeyFile_t * k,             /** key file to set compare function for  */
int ( *keycompare )           /** compare function to set               */
    (
                void* left,   /** "left" buffer for compare             */
                size_t llen,  /** length of that buffer                 */
                void* right,  /** "right" buffer for compare            */
                size_t rlen   /** length of that buffer                 */
    )
)
{
    PBLKFILE_t * kf = ( PBLKFILE_t * ) k;

    kf->keycompare = keycompare;
}

/**
 * Flush a key file
 *
 * All changes are flushed to disk,
 *
 * @return int rc == 0: call went ok
 * @return int rc != 0: some error, see pbl_errno
 */

int pblKfFlush(
pblKeyFile_t * k   /** key file to flush */
)
{
    PBLKFILE_t * kf = ( PBLKFILE_t * ) k;
    int rc;

    pbl_errno = 0;

    rc = pblBlockFlush( kf->bf, 0 );
    if( rc )
    {
        return rc;
    }

    rc = pblBlockListTruncate();

    return rc;
}

/**
 * Commit or rollback changes done during a transaction.
 *
 * Transactions can be nested, if so the commit
 * only happens when the outermost transaction
 * calls a commit.
 *
 * The commit only happens to process space buffer cache,
 * call \Ref{pblKfFlush}() after \Ref{pblKfCommit}() if you want to
 * flush to kernel space buffer cache.
 *
 * @return int rc == 0: the commit went ok
 * @return int rc >  0: a rollback happened, either because the caller
 *                      requested it or because an inner transaction resulted
 *                      in a rollback
 * @return int rc <  0: some error, see pbl_errno
 */

int pblKfCommit(
pblKeyFile_t * k, /** key file to commit                                      */
int rollback      /** != 0: roll back the changes, == 0: commit the changes   */
)
{
    PBLKFILE_t * kf = ( PBLKFILE_t * ) k;
    PBLBLOCK_t * block;
    PBLBLOCK_t * b;

    pbl_errno = 0;

    /*
     * If there is no transaction active for the file
     */
    if( !rollback && ( kf->transactions < 1 ))
    {
        pbl_errno = PBL_ERROR_PROGRAM;
        return -1;
    }
    kf->transactions--;

    /*
     * If a rollback is requested for this commit
     */
    if( rollback )
    {
        /*
         * remember that at least one rollback is requested for the file
         */
        kf->rollback = 1;
    }

    /*
     * If there is an outer transaction active for the file
     */
    if( kf->transactions > 0 )
    {
        return kf->rollback;
    }

    /*
     * There is no more transaction active, rollback or commit
     */
    if( kf->rollback )
    {
        /*
         * Release all blocks that were changed without putting
         * them back into the blocklist buffer cache
         */
        while(( block = kf->writeableListTail ))
        {
            PBL_LIST_UNLINK( kf->writeableListHead, kf->writeableListTail,
                             block, next, prev );

            pblBlockCachedItemsRelease( block );

            PBL_FREE( block );
            continue;
        }

        /*
         * Reset the transaction and the rollback value
         */
        kf->transactions = 0;
        kf->rollback = 0;
        return 1;
    }

    /*
     * Commit the changed blocks
     */
    while(( block = kf->writeableListTail ))
    {
        /*
         * Commit all blocks that were changed by rechaining
         * them into the blocklist buffer cache
         */
        PBL_LIST_UNLINK( kf->writeableListHead, kf->writeableListTail,
                         block, next, prev );

        /*
         * Find a potential copy of the block in the LRU list
         */
        b = pblBlockFind( kf, block->blockno );
        if( b )
        {
            /*
             * delete the copy from the LRU list
             */
            PBL_LIST_UNLINK( blockListHead, blockListTail, b, next, prev );

            pblBlockCachedItemsRelease( b );

            PBL_FREE( b );
        }
        else
        {
            /*
             * We add a block to the LRU list
             */
            pblnblocks++;
        }

        /*
         * Blocks in the buffer cache are not writeable
         */
        block->writeable = 0;
        PBL_LIST_PUSH( blockListHead, blockListTail, block, next, prev );

        /*
         * Insert or update the reference in the hash table
         */
        if( pblBlockHashInsert( block->blockno, block->bf, block ) < 0 )
        {
            pbl_errno = PBL_ERROR_PROGRAM;
            return 0;
        }
    }

    /*
     * Reset the transaction and the rollback value
     */
    kf->transactions = 0;
    kf->rollback = 0;

    return 0;
}

/**
 * Start a transaction on a key file
 *
 * Transactions can be nested
 *
 * @return int rc == 0: the transaction was started successfully
 * @return int rc >  0: the transaction was started
 *                      but another transaction has resulted in
 *                      a rollback request on the file already
 */

int pblKfStartTransaction(
pblKeyFile_t * k           /** key file to start transaction on               */
)
{
    PBLKFILE_t * kf = ( PBLKFILE_t * ) k;
    pbl_errno = 0;

    /*
     * If there is no transaction active for the file
     */
    if( kf->transactions < 1 )
    {
        kf->transactions = 1;
        kf->rollback = 0;
        return 0;
    }
    kf->transactions++;

    return kf->rollback;
}

/**
 * Save the position of the current record for later restore
 *
 * @return int rc == 0: the position was saved
 * @return int rc  < 0: an error, see pbl_errno
 */
int pblKfSavePosition(
pblKeyFile_t * k       /** key file to save position for                     */
)
{
    PBLKFILE_t * kf = ( PBLKFILE_t * ) k;

    pbl_errno = 0;

    /*
     * Save the block number and the index
     */
    kf->saveblockno = kf->blockno;
    kf->saveindex   = kf->index;

    return 0;
}

/**
 * Restore the position of the current record saved by the
 * last previous call to \Ref{pblKfSavePosition}().
 *
 * @return int rc == 0: the position was restored
 * @return int rc  < 0: an error, see pbl_errno
 */

int pblKfRestorePosition(
pblKeyFile_t * k       /** key file to restore position for                   */
)
{
    PBLKFILE_t * kf = ( PBLKFILE_t * ) k;

    /*
     * Restore the block number and the index
     */
    kf->blockno = kf->saveblockno;
    kf->index   = kf->saveindex;

    return 0;
}

/*
 * INSERT functions
 */
static int pblItemSave( PBLITEM_t * item )
{
    PBLITEM_t * newitem  = ( PBLITEM_t * ) 0;

    newitem = (PBLITEM_t *)pbl_malloc0( "pblItemSave ITEM", sizeof( PBLITEM_t ));
    if( !newitem )
    {
        return -1;
    }

    /*
     * Save the values of the item
     */
    *newitem = *item;

    /*
     * Save the key
     */
    if( newitem->keylen > 0 )
    {
        newitem->key = pbl_memdup( "pblItemSave item->key",
                                   item->key, newitem->keylen );
        if( !newitem->key )
        {
            PBL_FREE( newitem );
            return -1;
        }
    }
    else
    {
        newitem->key = 0;
    }

    /*
     * Save the data
     */
    if( newitem->datalen < 1 || newitem->datalen > PBLDATALENGTH )
    {
        newitem->data = 0;
    }

    /*
     * Push the new item to top of list
     */
    PBL_LIST_PUSH( itemListHead, itemListTail, newitem, next, prev );

    return 0;
}

static void pblItemRelease( void )
{
    PBLITEM_t * item;

    if(( item = itemListHead ))
    {
        PBL_LIST_UNLINK( itemListHead, itemListTail, item, next, prev );

        if( item->key )
        {
            PBL_FREE( item->key );
        }
        PBL_FREE( item );
    }
}

static void pblItemReleaseAll( void )
{
    while( itemListHead )
    {
        pblItemRelease( );
    }
}

static int pblSplit( PBLKFILE_t *file, PBLBLOCK_t * block )
{
    unsigned int   index;
    PBLITEM_t      splititem;
    PBLITEM_t      item;
    PBLBLOCK_t     tmpblock;
    PBLBLOCK_t    *newblock;
    PBLBLOCK_t    *target;
    unsigned char  predkey[ PBLKEYLENGTH + 1 ];
    unsigned int   predkeylen = 0;
    long           newblockno;
    int            rc;

    /*
     * Create a new block
     */
    newblockno = pblBlockAppend( file, block->level,
                                 block->nblock, block->blockno );
    if( newblockno < 0 )
    {
        return -1;
    }

    /*
     * Set the backward pointer in successor block of block
     */
    if( block->nblock )
    {
        PBLBLOCK_t * nextBlock = pblBlockGetWriteable( file, block->nblock );
        if( !nextBlock )
        {
            return -1;
        }

        nextBlock->pblock = newblockno;
        nextBlock->dirty  = 1;
    }

    /*
     * Get the new block to memory
     */
    newblock = pblBlockGetWriteable( file, newblockno );
    if( !newblock )
    {
        return -1;
    }

    /*
     * Copy the block to split onto the stack
     */
    pblBlockCachedItemsRelease( block );

    tmpblock = *block;

    /*
     * Prepare the block for split
     */
    block->nblock   = newblockno;
    block->nentries = 0;
    block->free     = PBLHEADERSIZE;
    block->dirty    = 1;
    memset( block->data, 0, PBLDATASIZE );
    memset( block->cachedItems, 0, sizeof( block->cachedItems ));

    /*
     * Copy the items from tmpblock to our two real blocks
     */
    for( target = block, index = 0; index < tmpblock.nentries; index++ )
    {
        rc = pblItemGet( &tmpblock, index, &item );
        if( rc )
        {
            pblBlockCachedItemsRelease( &tmpblock );

            return -1;
        }

        /*
         * If first block we copy to is more than half full
         */
        if(  ( target == block )
         && (( PBLINDEXBLOCKNFREE( block ) < ( PBLDATASIZE / 2 ))
          || ( index == tmpblock.nentries - 1 )))
        {
            /*
             * The first item that goes to the new second block has to be
             * saved for later insert into the father block of the new block
             */
            splititem = item;
            splititem.datalen    = 0;
            splititem.datablock  = newblockno;
            splititem.dataoffset = 0;
            splititem.level++;

            rc = pblItemSave( &splititem );
            if( rc )
            {
                pblBlockCachedItemsRelease( &tmpblock );

                return -1;
            }

            /*
             * For blocks of level greater than 0 the first item on each block
             * has keylength 0
             */
            if( tmpblock.level > 0 )
            {
                item.keylen = 0;
            }

            /*
             * From now on copy to the second block
             */
            target = newblock;
            predkeylen = 0;
        }

        rc = pblItemAppend( target, predkey, predkeylen, &item );
        if( rc < 0 )
        {
            pblBlockCachedItemsRelease( &tmpblock );

            return -1;
        }

        predkeylen = item.keylen;
        if( predkeylen > 0 )
        {
            memcpy(predkey, item.key, item.keylen);
        }
        else
        {
            predkey[ 0 ] = 0;
        }
    }

    pblBlockCachedItemsRelease( &tmpblock );

    /*
     * Set the parent pointers to the new block
     */
    newblock->parentblock = block->parentblock;
    newblock->parentindex = block->parentindex + 1;

    return 0;
}

static int pblSplitRoot( PBLKFILE_t *file )
{
    PBLBLOCK_t * rootblock;
    PBLITEM_t    item;
    PBLBLOCK_t * newblock;
    long         newblockno;
    int          rc;

    /*
     * Get the root block to memory
     */
    rootblock = pblBlockGetWriteable( file, 0 );
    if( !rootblock )
    {
        return -1;
    }

    /*
     * Create a new block and get it to memory
     */
    newblockno = pblBlockAppend( file, rootblock->level, 0, 0 );
    if( newblockno < 0 )
    {
        return -1;
    }

    newblock = pblBlockGetWriteable( file, newblockno );
    if( !newblock )
    {
        return -1;
    }

    /*
     * Copy some data of the root block to the new block
     */
    newblock->nentries = rootblock->nentries;
    newblock->free     = rootblock->free;
    memcpy( newblock->data, rootblock->data, PBLDATASIZE );

    newblock->dirty    = 1;

    /*
     * Get the root block to memory
     */
    rootblock = pblBlockGetWriteable( file, 0 );
    if( !rootblock )
    {
        return -1;
    }

    /*
     * Clear the root block
     */
    rootblock->level   += 1;
    rootblock->nentries = 0;
    rootblock->free     = PBLHEADERSIZE;
    rootblock->dirty    = 1;
    memset( rootblock->data, 0, PBLDATASIZE );
    pblBlockCachedItemsRelease( rootblock );

    newblock = pblBlockGetWriteable( file, newblockno );
    if( !newblock )
    {
        return -1;
    }

    /*
     * Copy the first item from new block to the root block
     */
    rc = pblItemGet( newblock, 0, &item );
    if( rc )
    {
        return -1;
    }

    item.level      = rootblock->level;
    item.keylen     = 0;
    item.datalen    = 0;
    item.datablock  = newblockno;
    item.dataoffset = 0;

    rc = pblItemAppend( rootblock, 0, 0, &item );
    if( rc < 0 )
    {
        return -1;
    }

    /*
     * Set the parent pointers to the new block
     */
    newblock->parentblock = 0;
    newblock->parentindex = 0;

    /*
     * Split the new block
     */
    return pblSplit( file, newblock );
}

/**
 * Insert a new record with the given key and data into a key file,
 *
 * Multiple records with the same key are allowed,
 * if there are already records with the same key the new
 * record will be inserted behind all records with the same key.
 *
 * The current record of the file will be set to the new record
 *
 * <P>
 * <B>RESTRICTIONS</B>:
 * <BR> - the file must be open for update,
 * <BR> - key must point to the key to be inserted,
 * <BR> - keylen must be bigger than 0 and smaller than 256,
 * <BR> - data must point to the data be inserted,
 * <BR> - datalen must not be negative,
 * <BR> - if datalen == 0, the pointer data is not evaluated at all
 *
 * @return int rc == 0: call went ok
 * @return int rc != 0: some error occured, see pbl_errno
 */

int pblKfInsert(
pblKeyFile_t  * k,      /** key file to insert to                             */
void          * key,    /** key to insert                                     */
size_t          keylen, /** length of the key                                 */
void          * data,   /** data to insert                                    */
size_t          datalen /** length of the data                                */
)
{
    PBLKFILE_t *  kf = ( PBLKFILE_t * ) k;
    PBLITEM_t     item;
    PBLITEM_t   * insertitem;
    PBLBLOCK_t  * block;
    long          blockno;
    int           index;
    int           saveerrno;
    int           rc;

    long          parentblock = -1;
    int           parentindex = -1;
    int           split = 0;

    pbl_errno = 0;

    /*
     * Start a transaction on the key file
     */
    pblKfStartTransaction( k );
    if( pblBlockListTruncate())
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    if( !kf->update )
    {
        pblKfCommit( k, 1 );
        pbl_errno = PBL_ERROR_NOT_ALLOWED;
        return -1;
    }

    rc = pblParamsCheck( key, keylen, data, datalen );
    if( rc )
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    /*
     * Only data that is longer than PBLDATALENGTH
     * bytes is written to data blocks
     */
    if( datalen > PBLDATALENGTH )
    {
        /*
         * Append the data to the file
         */
        item.datablock = pblDataAppend( kf, data, datalen, &item.dataoffset );
        if( item.datablock < 1 )
        {
            pblKfCommit( k, 1 );
            return -1;
        }
        item.data = 0;
    }
    else
    {
        item.datablock = 0;
        item.dataoffset = 0;
        item.data = data;
    }

    /*
     * Prepare the data item to be inserted
     */
    item.level      = 0;
    item.keylen     = keylen;
    item.keycommon  = 0;
    item.key        = key;
    item.datalen    = datalen;

    /*
     * Push the item to the insert stack of the file
     */
    rc = pblItemSave( &item );
    if( rc )
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    /*
     * Insert all items that are on the insert stack
     */
    while( itemListHead )
    {
        insertitem = itemListHead;

        /*
         * We always start the insert at the root block
         */
        blockno = 0;
        parentblock = -1;
        parentindex = -1;

        /*
         * Handle all levels of the tree
         */
        while( !pbl_errno )
        {
            block = pblBlockGet( kf, blockno );
            if( !block )
            {
                break;
            }

            /*
             * Set the links to the parentblock of the block
             */
            block->parentblock = parentblock;
            block->parentindex = parentindex;

            /*
             * If the item has to be inserted in this level
             */
            if( block->level <= insertitem->level )
            {
                block = pblBlockGetWriteable( kf, blockno );
                if( !block )
                {
                    break;
                }

                index = pblItemAdd( kf, block, insertitem );
                if( index < 0 )
                {
                    if( pbl_errno == PBL_ERROR_NOFIT )
                    {
                        pbl_errno = 0;

                        /*
                         * Split the root block or a normal block
                         */
                        if( blockno )
                        {
                            rc = pblSplit( kf, block );
                        }
                        else
                        {
                            rc = pblSplitRoot( kf );
                        }
                        if( !rc )
                        {
                            split = 1;
                        }
                    }
                    break;
                }

                /*
                 * Insert was successful
                 */
                if( block->level == 0 )
                {
                    /*
                     * Set values of current record
                     */
                    kf->blockno = block->blockno;
                    kf->index   = index;
                }

                /*
                 * Release the item that was inserted
                 */
                pblItemRelease( );

                break;
            }

            for(;;)
            {
                block = pblBlockGet( kf, blockno );
                if( !block )
                {
                    break;
                }

                /*
                 * Set the links to the parentblock of the block
                 */
                block->parentblock = parentblock;
                block->parentindex = parentindex;

                /*
                 * The item has to be inserted on a lower level, find out where
                 *
                 * We either insert into the last subtree, or into the
                 * greatest smaller subtree
                 */
                index = pblItemFind( kf, block, insertitem, PBLLA );
                if( index < 0 )
                {
                    if( pbl_errno == PBL_ERROR_NOT_FOUND )
                    {
                        pbl_errno = 0;
                        index = pblItemFind( kf, block, insertitem, PBLLT );
                    }

                    if( index < 0 )
                    {
                        break;
                    }
                }

                rc = pblItemGet( block, index, &item );
                if( rc )
                {
                    break;
                }

                /*
                 * See if we can merge blocks before the insert
                 */
                if( !split )
                {
                    rc = pblBlockMerge( kf, blockno,
                                        index, item.datablock );
                    if( rc > 0 )
                    {
                        continue;
                    }
                    else if( rc < 0 )
                    {
                        break;
                    }
                }

                /*
                 * Get the blockno of the relevant child block
                 */
                blockno = item.datablock;
                parentblock = block->blockno;
                parentindex = index;

                pbl_errno = 0;
                break;
            }
        }

        /*
         * If an error occurred during this insert
         */
        if( pbl_errno )
        {
            break;
        }
    }

    saveerrno = pbl_errno;

    pblItemReleaseAll( );

    if( saveerrno )
    {
        kf->blockno = -1;
        kf->index = -1;
        pblKfCommit( k, 1 );
        pbl_errno = saveerrno;
        return -1;
    }

    pblKfCommit( k, 0 );
    return 0;
}

/*
 * UPDATE functions
 */
static PBLBLOCK_t * pblPositionCheck( PBLKFILE_t *kf )
{
    PBLBLOCK_t * block;
    int          index;

    /*
     * Check if the current block is set for the file
     */
    if( kf->blockno < 0 )
    {
        pbl_errno = PBL_ERROR_POSITION;
        return NULL;
    }

    /*
     * Get the current block to memory
     */
    block = pblBlockGet( kf, kf->blockno );
    if( !block )
    {
        return NULL;
    }
    index = kf->index;

    /*
     * If we are positioned on our pseudo magic item, we set to next item
     */
    if(( index == 0 ) && ( !block->pblock || !block->blockno ))
    {
        index = 1;
    }

    /*
     * If the index is negative, we actually are set on the last item of
     * the predecessor of the current block, or if the there is no predecessor
     * on the first item of the current block
     */
    while( index < 0 )
    {
        /*
         * If we are on the first block, we need the second item, because the
         * the first item is our pseudo magic item
         */
        if( !block->pblock || !block->blockno )
        {
            index = 1;
        }
        else
        {
            block = pblBlockGet( kf, block->pblock );
            if( !block )
            {
                return NULL;
            }

            if( block->nentries )
            {
                index = block->nentries - 1;
            }
        }
    }

    while( ( int )index >= ( int )block->nentries )
    {
        /*
         * If there is no successor of the current block, we have to stay here,
         * the rootblock never has a successor !
         */
        if( !block->nblock || !block->blockno )
        {
            break;
        }
        else
        {
            block = pblBlockGet( kf, block->nblock );
            if( !block )
            {
                return NULL;
            }

            if( block->nentries )
            {
                index = 0;
            }
        }
    }

    while( ( int )index >= ( int )block->nentries )
    {
        /*
         * If the block has entries, we take the last one
         */
        if( block->nentries )
        {
            index = block->nentries - 1;
        }
        else if( !block->pblock || !block->blockno )
        {
            /*
             * This is a structure error, because the first block always has
             * at least one item, our pseudo magic item
             */
            pbl_errno = PBL_ERROR_BAD_FILE;
            kf->blockno = -1;
            return NULL;
        }
        else
        {
            block = pblBlockGet( kf, block->pblock );
            if( !block )
            {
                return NULL;
            }

            if( block->nentries )
            {
                index = block->nentries - 1;
            }
        }
    }

    /*
     * If we ended up positioning at our pseudo item, the file does not
     * have any other items
     */
    if(( index == 0 ) && ( !block->pblock || !block->blockno ))
    {
        pbl_errno = PBL_ERROR_NOT_FOUND;
        kf->blockno = -1;
        return NULL;
    }

    kf->blockno = block->blockno;
    kf->index = index;

    return block;
}

static long pblDataWrite(
PBLKFILE_t    * file,
unsigned char * data,
long         blockno,
long         blockoffset,
long         datalen
)
{
    long         diff;
    long         bytesWritten = 0;
    int          nbytes;
    PBLBLOCK_t * datablock = (PBLBLOCK_t *) 0;

    while( bytesWritten < datalen )
    {
        datablock = pblBlockGetWriteable( file, blockno );
        if( !datablock )
        {
            return -1;
        }

        if( datablock->level != 255 )
        {
            pbl_errno = PBL_ERROR_BAD_FILE;
            return -1;
        }

        diff = datalen - bytesWritten;
        if( diff > ( PBLDATASIZE - blockoffset ) )
        {
            nbytes = PBLDATASIZE - blockoffset;
        }
        else
        {
            nbytes = ((int ) ( diff % PBLDATASIZE));
        }

        if( nbytes > 0 )
        {
            memcpy((void *) &(datablock->data[ blockoffset ]),
                   (void *) data,
                   nbytes );
            datablock->dirty = 1;

            bytesWritten += nbytes;
            data         += nbytes;
        }

        if( bytesWritten < datalen )
        {
            /*
             * Get offset of next block and set blockoffset to beginning of
             * data
             */
            blockno     = datablock->nblock;
            blockoffset = PBLHEADERSIZE;
        }
    }

    return bytesWritten;
}

/**
 * Delete the current record of the key file.
 *
 * The current record of the file is set to the next record or
 * if the last record is deleted, to the previous record.
 *
 * If there are no more records in the file after the delete
 * the current record is of course unpositioned
 *
 * <P>
 * <B>RESTRICTIONS</B>:
 * <BR> - the file must be open for update,
 * <BR> - no space will be given back to the file system,
 * <BR> - if an index block and its successor or its predeccessor
 *        together use less than half of a block the two blocks are merged
 *
 * @return int rc == 0: call went ok
 * @return int rc != 0: some error occured, see pbl_errno
 */

int pblKfDelete(
pblKeyFile_t  * k       /** key file to delete record from                    */
)
{
    PBLKFILE_t *    kf = ( PBLKFILE_t * ) k;
    long            parentblock;
    PBLBLOCK_t    * block;
    PBLITEM_t       item;
    int             rc;

    pbl_errno = 0;

    /*
     * Start a transaction on the key file
     */
    pblKfStartTransaction( k );
    if( pblBlockListTruncate())
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    if( !kf->update )
    {
        pblKfCommit( k, 1 );
        pbl_errno = PBL_ERROR_NOT_ALLOWED;
        return -1;
    }

    /*
     * Make sure the current record of the file is positioned
     */
    block = pblPositionCheck( kf );
    if( !block )
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    /*
     * Read the block the current item is on
     */
    block = pblBlockGetWriteable( kf, kf->blockno );
    if( !block )
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    /*
     * Read the item
     */
    rc = pblItemGet( block, kf->index, &item );
    if( rc )
    {
        return -1;
    }

    /*
     * Only data that is longer than PBLDATALENGTH
     * bytes is written to data blocks
     */
    if( item.datalen > PBLDATALENGTH )
    {
        /*
         * Since 1.04, 4 Nov 2008 23:02:03 CET, the data is also deleted
         */
        rc = pblDataRemove(kf, item.datablock, item.dataoffset, item.datalen);
        if (rc < 0)
        {
            return rc;
        }

		/*
		 * The block might have become a cache victim,
		 * make sure it is in memory,
         * read the block the current item is on
         */
        block = pblBlockGetWriteable( kf, kf->blockno );
        if( !block )
        {
            pblKfCommit( k, 1 );
            return -1;
        }

        /*
         * Read the item
         */
        rc = pblItemGet( block, kf->index, &item );
        if( rc )
        {
            return -1;
        }
    }

    /*
     * Delete the current item
     */
    rc = pblItemRemove( block, kf->index );
    if( rc )
    {
        pblKfCommit( k, 1 );
        return rc;
    }

    /*
     * We deleted an item, now see if we can merge some blocks
     */
    for(;;)
    {
        if( block->parentblock < 0 || block->parentindex < 0 )
        {
            rc = 0;
            break;
        }

        /*
         * Remember the blocknumber of the parent block
         */
        parentblock = block->parentblock;

        /*
         * See whether some blocks can be merged because of the delete
         */
        rc = pblBlockMerge( kf, block->parentblock,
                            block->parentindex, block->blockno );
        if( rc < 1 )
        {
            break;
        }

        /*
         * The merge deleted an item on the parent block, read that block
         */
        block = pblBlockGetWriteable( kf, parentblock );
        if( !block )
        {
            pblKfCommit( k, 1 );
            return -1;
        }
    }

    if( rc )
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    pblKfCommit( k, 0 );
    return rc;
}

/**
 * Update the data of the current record
 *
 * The current record of the file is updated with the new data given.
 *
 *
 * <P>
 * <B>RESTRICTIONS</B>:
 * <BR> - the file must be open for update,
 * <BR> - if the new datalen of the record is not bigger than the old datalen,
 *        the data will be updated in place, otherwise the new data of the
 *        record will be appended to the file, the space previously used for
 *        the data of the record will not be reused in this case,
 * <BR> - data must point to the new data be inserted,
 * <BR> - datalen must not be negative,
 * <BR> - if datalen == 0, the pointer data is not evaluated at all
 *
 * @return int rc == 0: call went ok
 * @return int rc != 0: some error occured, see pbl_errno
 */

int pblKfUpdate(
pblKeyFile_t  * k,      /** key file to delete record from                    */
void          * data,   /** new data to update with                           */
size_t          datalen /** length of the new data                            */
)
{
    PBLKFILE_t      * kf = ( PBLKFILE_t * ) k;
    PBLITEM_t         item;
    PBLBLOCK_t      * block;
    long              rc;
    unsigned char     key[ PBLKEYLENGTH ];

    pbl_errno = 0;

    /*
     * Start a transaction on the key file
     */
    pblKfStartTransaction( k );
    if( pblBlockListTruncate())
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    if( !kf->update )
    {
        pblKfCommit( k, 1 );
        pbl_errno = PBL_ERROR_NOT_ALLOWED;
        return -1;
    }

    if( pblParamsCheck( (unsigned char*)1, 1, data, datalen ))
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    /*
     * Make sure current record of the file is positioned
     */
    block = pblPositionCheck( kf );
    if( !block )
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    /*
     * Read the block the current item is on
     */
    block = pblBlockGetWriteable( kf, kf->blockno );
    if( !block )
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    /*
     * Read the item
     */
    rc = pblItemGet( block, kf->index, &item );
    if( rc )
    {
        pblKfCommit( k, 1 );
        return -1;
    }

    if( datalen == item.datalen )
    {
        /*
         * If the data is to be stored on an index block
         */
        if( datalen <= PBLDATALENGTH )
        {
            /*
             * Update in place
             */
            if( datalen > 0 )
            {
                memcpy( item.data, data, datalen );
                block->dirty = 1;
            }

            pblKfCommit( k, 0 );
            return 0;
        }

        /*
         * Update the data in place
         */
        rc = pblDataWrite( kf, data, item.datablock, item.dataoffset, datalen );
        if( rc != datalen )
        {
            pblKfCommit( k, 1 );
            return -1;
        }

        pblKfCommit( k, 0 );
        return 0;
    }

    if( item.keycommon )
    {
        /*
         * Read the item to get its real key
         */
        rc = pblItemGet( block, kf->index, &item );
        if( rc )
        {
            pblKfCommit( k, 1 );
            return -1;
        }
    }

    /*
     * We do a delete and an insert of the record
     */
    item.keylen &= 0xff;
    pbl_memlcpy( key, sizeof( key ), item.key, item.keylen );

    rc = pblKfDelete( k );
    if( rc )
    {
        pblKfCommit( k, 1 );
        return rc;
    }

    rc = pblKfInsert( k, key, item.keylen, data, datalen );
    if( rc )
    {
        pblKfCommit( k, 1 );
        return rc;
    }

    pblKfCommit( k, 0 );

    return rc;
}

/*
 * READ functions
 */
/*
 * Recursive find procedure for records
 */
static long pblFindRec(
PBLKFILE_t    * kf,
int             mode,
long            blockno,
long            parentblock,
int             parentindex,
PBLITEM_t     * item
)
{
    PBLITEM_t       curitem;
    PBLBLOCK_t    * block;
    int             index;
    long            rc;
    int             which;
    int             direction;

    /*
     * Get the block to memory
     */
    block = pblBlockGet( kf, blockno );
    if( !block )
    {
        return -1;
    }

    block->parentblock = parentblock;
    block->parentindex = parentindex;

    /*
     * Level 0, terminate the recursion
     */
    if( block->level == 0 )
    {
        /*
         * Find the item on the block, first that matches
         */
        index = pblItemFind( kf, block, item, mode );
        if( index < 0 )
        {
            return -1;
        }

        /*
         * Make sure nobody is finding our pseudo record
         */
        if(( index == 0 ) && ( !block->pblock || !block->blockno ))
        {
            pbl_errno = PBL_ERROR_NOT_FOUND;
            return -1;
        }

        rc = pblItemGet( block, index, &curitem );
        if( rc )
        {
            return -1;
        }

        /*
         * Find was successful set values of current record
         */
        kf->blockno = block->blockno;
        kf->index = index;

        /*
         * We return the datalength of the item
         */
        return curitem.datalen;
    }

    direction = 1;
    switch( mode )
    {
      case PBLLT:
        which     = PBLLT;
        direction = -1;
        break;

      case PBLFI:
      case PBLEQ:
        which     = PBLLT;
        break;

      case PBLLA:
      case PBLGT:
        which     = PBLLA;
        break;

      default:
        pbl_errno  = PBL_ERROR_PARAM_MODE;
        return -1;
    }

    /*
     * Find the subtree where to continue the find
     */
    index = pblItemFind( kf, block, item, which );
    if( index < 0 )
    {
        if( which == PBLLA )
        {
            if( pbl_errno == PBL_ERROR_NOT_FOUND )
            {
                pbl_errno = 0;
                index = pblItemFind( kf, block, item, PBLLT );
            }
        }
    }

    /*
     * Search in all possible subtrees
     */
    for( ; index >= 0 && index < (int)block->nentries; index += direction )
    {
        /*
         * Check if subtree can contain the item
         */
        rc = pblItemGet( block, index, &curitem );
        if( rc )
        {
            return -1;
        }

        rc = pblItemCompare( kf, &curitem, item );
        if(( rc > 0 ) && ( mode != PBLGT ))
        {
            pbl_errno = PBL_ERROR_NOT_FOUND;
            return -1;
        }

        /*
         * Recursive call to find procedure
         */
        rc = pblFindRec( kf, mode, curitem.datablock, blockno, index, item );
        if( rc >= 0 )
        {
            /*
             * The find was successful
             */
            return rc;
        }

        /*
         * If an error other than PBL_ERROR_NOT_FOUND occured, we give up
         */
        if( pbl_errno != PBL_ERROR_NOT_FOUND )
        {
            return -1;
        }
        else
        {
            pbl_errno = 0;
        }

        /*
         * Get the block to memory because during the recursive call
         * it might have become a victim
         */
        block = pblBlockGet( kf, blockno );
        if( !block )
        {
            return -1;
        }

        block->parentblock = parentblock;
        block->parentindex = parentindex;
    }

    /*
     * We couldn't find the item, tell the caller
     */
    pbl_errno = PBL_ERROR_NOT_FOUND;
    return -1;
}

static long pblDataGet(
PBLKFILE_t    * file,
unsigned char * data,
long         blockno,
long         blockoffset,
long         datalen
)
{
    long         diff;
    long         bytesRead = 0;
    int          nbytes;
    PBLBLOCK_t * datablock = (PBLBLOCK_t *) 0;

    while( bytesRead < datalen )
    {
        datablock = pblBlockGet( file, blockno );
        if( !datablock )
        {
            return -1;
        }

        if( datablock->level != 255 )
        {
            pbl_errno = PBL_ERROR_BAD_FILE;
            return -1;
        }

        diff = datalen - bytesRead;
        if( diff > ( PBLDATASIZE - blockoffset ) )
        {
            nbytes = PBLDATASIZE - blockoffset;
        }
        else
        {
            nbytes = ((int ) ( diff % PBLDATASIZE));
        }

        if( nbytes > 0 )
        {
            memcpy((void *) data,
                   (void *) &(datablock->data[ blockoffset ]),
                   nbytes );

            bytesRead += nbytes;
            data      += nbytes;
        }

        if( bytesRead < datalen )
        {
            /*
             * Get number of next block and set blockoffset to beginning of
             * data
             */
            blockno     = datablock->nblock;
            blockoffset = PBLHEADERSIZE;
        }
    }

    return bytesRead;
}

/**
 * Find a record in a key file, set the current record.
 *
 * The parameter mode specifies which record to find relative
 * to the search key specified by skey and skeylen.
 * The following values for mode are possible
 *
 * <BR><B> PBLEQ </B> - find a record whose key is equal to skey
 * <BR><B> PBLFI </B> - find the first record that is equal
 * <BR><B> PBLLA </B> - find the last record that is equal
 * <BR><B> PBLGE </B> - find the last record that is equal or the smallest
 *                      record that is greater
 * <BR><B> PBLGT </B> - find the smallest record that is greater
 * <BR><B> PBLLE </B> - find the first record that is equal or the biggest
 *                      record that is smaller
 * <BR><B> PBLLT </B> - find the biggest record that is smaller
 *
 * Keep in mind that PBL allows multiple records with the same key.
 *
 * <P>
 * <B>RESTRICTIONS</B>:
 * <BR> - the out parameter okey must point to a memory area that is
 *        big enough to hold any possible key, i.e 255 bytes
 *
 * @return long rc >= 0:
 * <UL>
 * <LI>                  call went ok,
 *                       the value returned is the length
 *                       of the data of the record found,
 * <LI>                  the length of the key of the record is set in
 *                       the out parameter okeylen,
 * <LI>                  the key of the record is copied to okey,
 * <LI>                  the current record of the file is set to the
 *                       record found
 * </UL>
 *
 * @return long rc <  0:
 * <UL>
 * <LI>                  some error occured, see pbl_errno
 *                       especially PBL_ERROR_NOT_FOUND, if there is no
 *                       matching record
 * </UL>
 */
long pblKfFind(
pblKeyFile_t  * k,       /** key file to search in                            */
int             mode,    /** mode to use for search                           */
void          * skey,    /** key to use for search                            */
size_t          skeylen, /** length of search key                             */
void          * okey,    /** buffer for result key                            */
size_t        * okeylen  /** length of the result key after return            */
)
{
    PBLKFILE_t    * kf = ( PBLKFILE_t * ) k;
    PBLBLOCK_t    * block;
    PBLITEM_t       item;
    long            rc;
    int             which;

    pbl_errno = 0;

    rc = pblParamsCheck( skey, skeylen, (unsigned char*)0, 0 );
    if( rc )
    {
        return -1;
    }

    /*
     * Prepare the data item to be found
     */
    memset( &item, 0, sizeof( item ));
    item.keylen = skeylen;
    item.key    = skey;

    if( mode == PBLLE )
    {
        which = PBLFI;
    }
    else if( mode == PBLGE )
    {
        which = PBLLA;
    }
    else
    {
        which = mode;
    }

    /*
     * We always start the find at the root block
     */
    rc = pblFindRec( kf, which, 0, -1, -1, &item );
    if( rc < 0 )
    {
        if( pbl_errno == PBL_ERROR_NOT_FOUND )
        {
            if( mode == PBLLE )
            {
                rc = pblFindRec( kf, PBLLT, 0, -1, -1, &item );
            }
            else if( mode == PBLGE )
            {
                rc = pblFindRec( kf, PBLGT, 0, -1, -1, &item );
            }
        }
    }

    if( rc < 0 )
    {
        return -1;
    }

    /*
     * Get the current block to memory
     */
    block = pblBlockGet( kf, kf->blockno );
    if( !block )
    {
        return -1;
    }

    /*
     * Read the item
     */
    rc = pblItemGet( block, kf->index, &item );
    if( rc )
    {
        return -1;
    }

    /*
     * Set the out parameters, if a buffer is supplied
     */
    if( okey && okeylen )
    {
        *okeylen = item.keylen;
        pbl_memlcpy( okey, PBLKEYLENGTH, item.key, item.keylen );
    }

    return item.datalen;
}

/**
 * Read the data of the current record of the file.
 *
 * The caller can restrict the number of bytes read by
 * specifying the maximum number of bytes to read by parameter
 * datalen, if datalen is 0, all bytes stored for the
 * current record are copied to the buffer pointed to by data.
 * <P>
 * <B>RESTRICTIONS</B>:
 * <BR> - data must point to an area of memory being big enough to hold
 *        the bytes copied
 * <BR> - datalen must not be negative, it is ignored otherwise
 *
 * @return int rc == 0: call went ok, rc is the number of bytes copied
 * @return int rc != 0: some error occured, see pbl_errno
 */

long pblKfRead(
pblKeyFile_t  * k,      /** key file to read from                             */
void          * data,   /** data to insert                                    */
long            datalen /** length of the data                                */
)
{
    PBLKFILE_t    * kf = ( PBLKFILE_t * ) k;
    PBLITEM_t       item;
    PBLBLOCK_t    * block;
    long            rc;

    pbl_errno = 0;

    rc = pblParamsCheck( (unsigned char*)1, 1, (unsigned char*)data, datalen );
    if( rc )
    {
        return -1;
    }

    /*
     * Check position of current record
     */
    block = pblPositionCheck( kf );
    if( !block )
    {
        return -1;
    }

    /*
     * Read the item
     */
    rc = pblItemGet( block, kf->index, &item );
    if( rc )
    {
        return -1;
    }

    /*
     * The caller can restrict the number of bytes read
     */
    if( datalen > 0 )
    {
        if( datalen > item.datalen )
        {
            datalen = item.datalen;
        }
    }
    else
    {
        datalen = item.datalen;
    }

    /*
     * If the data is stored on an index block
     */
    if( datalen <= PBLDATALENGTH )
    {
        memcpy( data, item.data, datalen );
        return datalen;
    }

    /*
     * The data is stored on a data block, read it from the file
     */
    rc = pblDataGet( kf, data, item.datablock, item.dataoffset, datalen );
    if( rc != datalen )
    {
        return -1;
    }

    return datalen;
}

/**
 * Set current record to a record with a relative position index.
 *
 * This function is only to be used through the macro functions:
 *
 * <BR><B> \Ref{pblKfThis}( k, okey, okeylen ) </B> read key of current record
 * <BR><B> \Ref{pblKfNext}( k, okey, okeylen ) </B> read key of next record
 * <BR><B> \Ref{pblKfPrev}( k, okey, okeylen ) </B> read key of previous record
 *
 * @return long rc >= 0:
 * <UL>
 * <LI>                  call went ok,
 *                       the value returned is the length
 *                       of the data of the record found,
 * <LI>                  the length of the key of the record is set in
 *                       the out parameter okeylen,
 * <LI>                  the key of the record is copied to okey,
 * <LI>                  the current record of the file is set to the
 *                       record found
 * </UL>
 *
 * @return long rc <  0:
 * <UL>
 * <LI>                  some error occured, see pbl_errno
 * </UL>
 */

long pblKfGetRel(
pblKeyFile_t  * k,        /** key file to position in                */
long            relindex, /** index relative to current record       */
void          * okey,     /** buffer for result key                  */
size_t        * okeylen   /** length of the result key after return  */
)
{
    PBLKFILE_t * kf = ( PBLKFILE_t * ) k;
    PBLITEM_t    item;
    PBLBLOCK_t * block;
    int          index;
    long         rc;

    pbl_errno = 0;

    /*
     * Check position of current record
     */
    block = pblPositionCheck( kf );
    if( !block )
    {
        return -1;
    }

    /*
     * Start searching at current block and current index
     */
    index = kf->index;

    /*
     * If we want an item that is to the left of the current item
     */
    while( !pbl_errno && relindex < 0 )
    {
        relindex++;

        /*
         * As long as we can go to the left on current block
         */
        if( index > 0 )
        {
            index--;
            continue;
        }

        /*
         * Find a block that has entries
         */
        for(;;)
        {
            /*
             * Go to previous block
             */
            if( !block->pblock || !block->blockno )
            {
                pbl_errno = PBL_ERROR_NOT_FOUND;
                break;
            }

            block = pblBlockGet( kf, block->pblock );
            if( !block )
            {
                break;
            }

            if( block->nentries )
            {
                index = block->nentries - 1;
                break;
            }
        }
    }

    /*
     * If we want an item that is to the right of the current item
     */
    while( !pbl_errno && relindex > 0 )
    {
        relindex--;

        /*
         * As long as we can go to the right on this block
         */
        if(( int )( index + 1 ) < ( int )block->nentries )
        {
            index++;
            continue;
        }

        /*
         * Find a block that has at least one entry
         */
        for(;;)
        {
            /*
             * Go to next block, but beware that rootblock has no next
             */
            if( !block->nblock || !block->blockno )
            {
                pbl_errno = PBL_ERROR_NOT_FOUND;
                break;
            }

            block = pblBlockGet( kf, block->nblock );
            if( !block )
            {
                break;
            }

            if( block->nentries )
            {
                index = 0;
                break;
            }
        }
    }

    /*
     * If an error occured we tell the caller
     */
    if( pbl_errno )
    {
        return -1;
    }

    /*
     * Read the item
     */
    rc = pblItemGet( block, index, &item );
    if( rc )
    {
        return -1;
    }

    /*
     * If the item we are standing on is our first record, we have reached the
     * end of file while reading backward
     */
    if( item.keylen == 0 )
    {
        pbl_errno = PBL_ERROR_NOT_FOUND;
        return -1;
    }

    /*
     * Set the out parameters, if a buffer is supplied
     */
    if( okey && okeylen )
    {
        *okeylen = item.keylen;
        pbl_memlcpy( okey, PBLKEYLENGTH, item.key, item.keylen );
    }

    kf->blockno = block->blockno;
    kf->index = index;

    return item.datalen;
}

/**
 * Set the current record to a record with an absolute position index.
 *
 * This function is only to be used through the macro functions:
 *
 * <BR><B> \Ref{pblKfFirst}( k, okey, okeylen ) </B> read key of first record
 * <BR><B> \Ref{pblKfLast}( k, okey, okeylen ) </B> read key of last record
 *
 * @return long rc >= 0:
 * <UL>
 * <LI>                  call went ok,
 *                       the value returned is the length
 *                       of the data of the record found,
 * <LI>                  the length of the key of the record is set in
 *                       the out parameter okeylen,
 * <LI>                  the key of the record is copied to okey,
 * <LI>                  the current record of the file is set to the
 *                       record found
 * </UL>
 *
 * @return long rc <  0:
 * <UL>
 * <LI>                  some error occured, see pbl_errno
 * </UL>
 */

long pblKfGetAbs(
pblKeyFile_t  * k,        /** key file to position in                */
long            absindex, /** index of record to positon to          */
void          * okey,     /** buffer for result key                  */
size_t        * okeylen   /** length of the result key after return  */
)
{
    PBLKFILE_t * kf = ( PBLKFILE_t * ) k;
    PBLITEM_t    item;
    PBLBLOCK_t * block;
    int          index;
    long         rc;

    pbl_errno = 0;

    /*
     * Start searching at rootblock
     */
    block = pblBlockGet( kf, 0 );
    if( !block )
    {
        return -1;
    }

    /*
     * Step down through tree to level 0
     */
    while( !pbl_errno && block->level )
    {
        if( absindex >= 0 )
        {
           index = 0;
        }
        else
        {
           index = block->nentries - 1;
        }

        rc = pblItemGet( block, index, &item );
        if( rc )
        {
            break;
        }

        /*
         * Get the relevant child block
         */
        block = pblBlockGet( kf, item.datablock );
        if( !block )
        {
            return -1;
        }
    }

    /*
     * If no error yet, we do a relative get
     */
    if( !pbl_errno )
    {
        /*
         * prepare relative get
         */
        kf->blockno = block->blockno;

        if( absindex >= 0 )
        {
            kf->index = -1;
            return pblKfGetRel( k, absindex, okey, okeylen );
        }
        else
        {
            kf->index = block->nentries;
            return pblKfGetRel( k, absindex + 1, okey, okeylen );
        }
    }

    return -1;
}

/*
------------------------------------------------------------------------------
  FUNCTION:     pblKfXXX

  DESCRIPTION:  These macros allow to position the current record and
                to read the key, the keylen and the datalen of the new
                current record

                The following macros exist:

                pblKfFirst: set the current record to the first record of the
                            file

                pblKfLast:  set the current record to the last record of the
                            file

                pblKfNext:  set the current record to the record of the
                            file after the old current record, of course the
                            record of the file must be positioned before that

                pblKfPrev:  set the current record to the record of the
                            file before the old current record, of course the
                            record of the file must be positioned before that

                pblKfThis:  this function can be used to read the key, keylen
                            and datalen of the current record, the current
                            record is not changed by this

  RESTRICTIONS: the out parameter okey must point to a memory area that is
                big enough to hold any possible key, i.e 255 bytes

  RETURNS:      long rc >= 0: call went ok, the value returned is the length
                              of the data of the current record,
                              the length of the key of the record is set in
                              the out parameter okeylen,
                              the key of the record is copied to okey,

                long rc <  0: some error occured, see pbl_errno
                              PBL_ERROR_NOT_FOUND, there is no matching record
                              PBL_ERROR_POSITION, current record not set yet
------------------------------------------------------------------------------
*/


int pblKfBlockPrint(
char * path,       /** path of file to create                                 */
long blockno       /** number of block to print                               */
)
{
    PBLITEM_t     item;
    PBLKFILE_t  * kf;
    PBLBLOCK_t  * block;
    int           bf;
    int           i;

    pbl_errno = 0;
    memset( &item, 0, sizeof( item ));

    printf( "FILE %s, BLOCK %ld\n", path, blockno );

    bf = pbf_open( path, 0, PBLFILEBLOCKS, PBLDATASIZE );
    if( -1 == bf )
    {
        printf( "pbf_open failed, pbl_errno %d\n", pbl_errno );
        return -1;
    }

    /*
     * Get and init file structure
     */
    kf = (PBLKFILE_t *)pbl_malloc0( "pblKfBlockPrint FILE", sizeof( PBLKFILE_t ));
    if( !kf )
    {
        printf( "pbl_malloc0 failed, pbl_errno %d\n", pbl_errno );
        pbf_close( bf );
        return -1;
    }
    kf->magic      = pblkf_c_id;
    kf->bf         = bf;
    kf->update     = 0;
    kf->filesettag = NULL;
    kf->blockno    = -1;
    kf->index      = -1;

    pblnfiles++;

    /*
     * Get the block
     */
    block = pblBlockGet( kf, blockno );
    if( !block )
    {
        printf( "pblBlockGet failed, pbl_errno %d\n", pbl_errno );
        pblKfClose( ( pblKeyFile_t * ) kf );
        return -1;
    }

    if( block->level == 255 )
    {
        printf( "datablock\n" );
        pblKfClose( ( pblKeyFile_t * ) kf );
        return 0;
    }

    printf( "level %d, pblock %ld, nblock %ld, nentries %d, free %d\n",
            block->level, block->pblock, block->nblock,
            (int)block->nentries, block->free );

    if( block->nentries < 1 )
    {
        pblKfClose( ( pblKeyFile_t * ) kf );
        return 0;
    }

    for( i = 0; i < (int)block->nentries; i++ )
    {
        unsigned char * ptr;

        pblItemGet( block, i, &item );

        if( item.key )
        {
            ptr = item.key;
        }
        else
        {
            ptr = (unsigned char *) "NULL";
        }

        printf( "%03d %d %.*s, common %d, datalen %ld, block %ld, offset %ld\n",
                i, item.keylen,
                item.keylen, ptr,
                item.keycommon, item.datalen,
                item.datablock, item.dataoffset );
    }

    pblKfClose( ( pblKeyFile_t * ) kf );
    return 0;
}
