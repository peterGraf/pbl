/*
 pblListTest.c - pbl list test frame

 Copyright (C) 2008   Peter Graf

   This file is part of PBL - The Program Base Library.
   PBL is free software.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   For more information on the Program Base Library or Peter Graf,
   please see: http://www.mission-base.com/.

    $Log: pblListTest.c,v $
    Revision 1.31  2021/06/23 13:39:53  peter
    Cleanup of _WIN32 usage

    Revision 1.30  2021/06/12 11:18:27  peter
    Synchronizing with github version

    Revision 1.29  2018/03/11 19:07:07  peter
    Improved test framework

*/

/*
 * make sure "strings <exe> | grep Id | sort -u" shows the source file versions
 */
char* pblListTest_c_id = "$Id: pblListTest.c,v 1.31 2021/06/23 13:39:53 peter Exp $";

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

#include "pbl.h"

static FILE * log;
static FILE * infile;

static void putChar( int c )
{
   static int last = 0;

   if( last == '\n' && c == '\n' )
       return;

   last = c;
   ( void ) putc( last, log );
}

static int getNonSpace( FILE* file, int* lineNumber )
{
	int c = EOF;
	for(;;)
	{
		c = getc( file );
		if( c == '\n' )
		{
			*lineNumber += 1;
		}
		if( isspace( c ) )
		{
			continue;
		}
		break;
	}
	return c;
}

static int diffFiles( FILE * file1, char * logFileName )
{
	int rc = 0;
	int c1;
	int c2;
	int lineNumber1 = 1;
	int lineNumber2 = 1;

#ifdef _WIN32
#pragma warning(disable: 4996)
#endif
    FILE * file2 = fopen( logFileName, "r" );
    if( !file2 )
    {
        fprintf( stderr, "Failed to open %s, %s\n", logFileName, strerror( errno ));
        exit( -1 );
    }
	if( fseek( file1, 0L, SEEK_SET ) )
	{
		fprintf( stderr, "Failed to rewind input file, %s\n", strerror( errno ));
        exit( -1 );
	}
	for(;;)
	{
		c1 = getNonSpace( file1, &lineNumber1 );
		c2 = getNonSpace( file2, &lineNumber2 );

		if( c1 != c2 )
		{
			 printf( "# input and output differ in line %d\n", lineNumber1 );
             fprintf( log, "# input and output differ in line %d\n", lineNumber1 );
			 rc = lineNumber1;
			 break;
		}
		if( c1 == EOF )
		{
			break;
		}
	}
	fclose( file2 );
	return rc;
}

static int getChar( void )
{
    int c = getc( infile );

    /*
     * a '#' starts a comment for the rest of the line
     */
    if( c == '#')
    {
       while( c != '\n' && c != EOF )
       {
           c = getc( infile );
       }
    }

    if( c != EOF )
        putChar( c );

    return c ;
}


static void getWord( char * buffer )
{
    /*
     * skip preceeding blanks
     */
    int c = ' ';
    while( isspace( c ) )
        c = getChar();

    /*
     * read one word
     */
    for(;; c = getChar())
    {
        if( c == EOF )
        {
            exit( 0 );
        }

        if( isspace( c ) )
        {
            *buffer = '\0';
            return;
        }
        *buffer++ = c;
    }
}

static char * items[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
        "20", "21", "22", "23", "24", "25", "26", "27", "28", "29"
};

#if 0
int isPrime( unsigned int number )
{
    unsigned int i;
    for( i = 3; i < number / 2; i += 2 )
    {
        if( number / i * i == number )
        {
            return 0;
        }
    }

    return 1;
}

void printPower2Primes()
{
    unsigned int i = 1;
    unsigned int j;
    for( j = ( 1 << i++ ) + 1; i < 30; j = ( 1 << i++ ) + 1 )
    {
        unsigned int number = 0;
        for( number = j; number < 0xfffffff; number += 2 )
        {
            if( isPrime( number ))
            {
                printf( "%u\n", number );
                break;
            }
        }
    }
    exit( 0 );
}
#endif

/**
 * test frame for the key file library
 */
int pblList_TestFrame( int argc, char * argv[] )
{
	char*logFileName = "./pblListTest.log";
    char command[ 2048 ];
    char filename [ 2048 ];
    char buffer [ 2048 ];
    int  ival;
    int  dowork = 1;
    long rc;
    int  i;

    char * ptr;

    PblList * testCollection = ( PblList *) 0;
    PblIterator * testIterator = (PblIterator *) 0;

#if 0
    printPower2Primes();
#endif

    infile = stdin;
    if( argc > 1 )
    {
#ifdef _WIN32
#pragma warning(disable: 4996)
#endif
        infile = fopen( argv[ 1 ], "r" );
        if( !infile )
        {
            fprintf( stderr, "Failed to open %s, %s\n",
                     argv[ 1 ], strerror( errno ));
            exit( -1 );
        }
    }

    log = fopen( logFileName, "w" );
    if( !log )
    {
        fprintf( stderr, "cant open logfile, %s\n", logFileName );
        exit( 1 );
    }

    while( dowork )
    {
        memset( command, 0, sizeof( command ));
        memset( filename, 0, sizeof( filename ));
        memset( buffer, 0, sizeof( buffer ));

        errno = 0;

        printf( "\n##command: \n" );
        fprintf( log, "\n##command: \n" );

        fflush( stdout );
        fflush( log );
        getWord( command );

        if( command[0] == 'q' || command[0] == 'Q' )
        {
            dowork = 0;
        }
        else if( !strcmp( command, "createArrayList" ))
        {
            printf( "# createArrayList\n" );
            fprintf( log, "# createArrayList\n" );

            printf( "# pblListNewArrayList( void )\n" );
            fprintf( log, "# pblListNewArrayList( void )\n" );

            if( testCollection )
            {
                ( void ) pblListFree( testCollection );
            }

            testCollection = pblListNewArrayList( );

            if( testCollection )
            {
                printf( "# ok!\n" );
                fprintf( log, "# ok!\n" );
            }
            else
            {
                printf( "# pbl_errno %d\n", pbl_errno );
                fprintf( log, "# pbl_errno %d\n", pbl_errno );
            }
        }
        else if( !strcmp( command, "createLinkedList" ))
        {
            printf( "# createLinkedList\n" );
            fprintf( log, "# createLinkedList\n" );

            printf( "# pblListNewLinkedList( void )\n" );
            fprintf( log, "# pblListNewLinkedList( void )\n" );

            if( testCollection )
            {
                ( void ) pblListFree( testCollection );
            }

            testCollection = pblListNewLinkedList( );

            if( testCollection )
            {
                printf( "# ok!\n" );
                fprintf( log, "# ok!\n" );
            }
            else
            {
                printf( "# pbl_errno %d\n", pbl_errno );
                fprintf( log, "# pbl_errno %d\n", pbl_errno );
            }
        }


        else if( !strcmp( command, "free" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListFree( %d )\n", 1 );
            fprintf( log, "# pblListFree( %d )\n", 1 );

            pblListFree( testCollection );
            testCollection = ( PblList * ) 0;

            printf( "# ok" );
            fprintf( log, "# ok\n" );
        }

        else if( !strcmp( command, "add" ))
        {
            if( !testCollection )
                continue;

            printf( "# add itemIndex\n" );
            fprintf( log, "# add itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListAdd( %s )\n",
                    ptr ? ptr : "NULL" );

            fprintf( log, "# pblListAdd( %s )\n",
                     ptr ? ptr : "NULL" );

            rc = pblListAdd( testCollection, ptr );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "addAll" ))
        {
            PblList * clone = ( PblList *) 0;

            if( !testCollection )
                continue;

            printf( "# pblListAddAll( %d )\n", 1 );
            fprintf( log, "# pblListAddAll( %d )\n", 1 );

            clone = pblListClone( testCollection );
            if( !clone )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        (long)-1, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        (long)-1, pbl_errno );
                break;
            }

            rc = pblListAddAll( testCollection, clone );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                pblListFree( clone );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
            pblListFree( clone );
        }

        else if( !strcmp( command, "addAllAt" ))
        {
            PblList * clone = ( PblList *) 0;

            if( !testCollection )
                continue;

            printf( "# addAllAt index\n" );
            fprintf( log, "# addAllAt index\n" );

            getWord( command );
            i = atoi( command );

            printf( "# pblListAddAllAt( %d )\n",
                    i );

            fprintf( log, "# pblListAddAllAt( %d )\n",
                    i );

            clone = pblListClone( testCollection );
            if( !clone )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        (long)-1, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        (long)-1, pbl_errno );
                break;
            }

            rc = pblListAddAllAt( testCollection, i, clone );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                pblListFree( clone );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
            pblListFree( clone );
        }

        else if( !strcmp( command, "addAt" ))
        {
            if( !testCollection )
                continue;

            printf( "# addAt index itemIndex\n" );
            fprintf( log, "# addAt index itemIndex\n" );

            getWord( command );
            i = atoi( command );

            getWord( buffer );
            ival = atoi( buffer );
            ptr = items[ ival ];

            printf( "# pblListAddAt( %d, %s )\n",
                    i, ptr );

            fprintf( log, "# pblListAddAt( %d, %s )\n",
                    i, ptr);

            rc = pblListAddAt( testCollection, i, ptr );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }


        else if( !strcmp( command, "addFirst" ))
        {
            if( !testCollection )
                continue;

            printf( "# addFirst itemIndex\n" );
            fprintf( log, "# addFirst itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            ptr = items[ ival ];

            printf( "# pblListAddFirst( %s )\n",
                    ptr );

            fprintf( log, "# pblListAddFirst( %s )\n",
                    ptr);

            rc = pblListAddFirst( testCollection, ptr );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "addLast" ))
        {
            if( !testCollection )
                continue;

            printf( "# addLast itemIndex\n" );
            fprintf( log, "# addFirst itemLast\n" );

            getWord( buffer );
            ival = atoi( buffer );
            ptr = items[ ival ];

            printf( "# pblListAddLast( %s )\n",
                    ptr );

            fprintf( log, "# pblListAddLast( %s )\n",
                    ptr);

            rc = pblListAddLast( testCollection, ptr );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "clear" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListClear( %d )\n", 1 );
            fprintf( log, "# pblListClear( %d )\n", 1 );

            pblListClear( testCollection );

            printf( "# ok" );
            fprintf( log, "# ok\n" );
        }

        else if( !strcmp( command, "clone" ) )
        {
            PblList * clone;

            if( !testCollection )
                continue;

            printf( "# pblListClone( %d )\n", 1 );
            fprintf( log, "# pblListClone( %d )\n", 1 );

            clone = pblListClone( testCollection );
            if( !clone )
            {
                printf( "# NULL, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n", pbl_errno );
                break;
            }

            printf( "# ok size %d", pblListSize( clone ) );
            fprintf( log, "# ok size %d\n", pblListSize( clone ) );

            pblListFree( clone );
        }

        else if( !strcmp( command, "cloneRange" ))
        {
            PblList * clone;
            char ** array;

            if( !testCollection )
                continue;

            printf( "# cloneRange from to \n" );
            fprintf( log, "# cloneRange from to\n" );

            getWord( buffer );
            ival = atoi( buffer );

            getWord( command );
            i = atoi( command );

            printf( "# pblListCloneRange( %d, %d )\n", ival, i );
            fprintf( log, "# pblListCloneRange( %d, %d )\n",ival, i );

            clone = pblListCloneRange( testCollection, ival, i );
            if( !clone )
            {
                printf( "# NULL, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n", pbl_errno );
                continue;
            }

            if( pblListSize( clone ) == 0 )
            {
                printf( "# OK size %d\n", pblListSize( clone ) );
                fprintf( log, "# OK size %d\n", pblListSize( clone ) );
                pblListFree( clone );
                continue;
            }

            printf( "# pblListToArray( clone )\n" );
            fprintf( log, "# pblListToArray( clone )\n" );

            array = (char**)pblListToArray( clone );
            if( array == NULL )
            {
                printf( "# NULL, pbl_errno %d\n",
                        pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n",
                        pbl_errno );
                pblListFree( clone );
                continue;
            }

            printf( "# OK size %d\n", pblListSize( clone ) );
            fprintf( log, "# OK size %d\n", pblListSize( clone ) );

            for( i = 0; i < pblListSize( clone ); i++ )
            {
                char * item =  array[i];

                printf( "# OK i %d, %s\n", i, item != NULL ? item : "NULL" );
                fprintf( log, "# OK i %d, %s\n", i, item != NULL ? item : "NULL" );
            }

            PBL_FREE(array );

            printf( "# Clone Iteration DONE\n" );
            fprintf( log, "# Clone Iteration DONE\n" );

            pblListFree( clone );
        }

        else if( !strcmp( command, "contains" ))
        {
            if( !testCollection )
                continue;

            printf( "# contains itemIndex\n" );
            fprintf( log, "# contains itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListContains( %s )\n",
                    ptr ? ptr : "NULL" );

            fprintf( log, "# pblListContains( %s )\n",
                     ptr ? ptr : "NULL" );

            rc = pblListContains( testCollection, ptr );

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "containsAll" ) )
        {
            PblList * clone;

            if( !testCollection )
                continue;

            printf( "# pblListClone( %d )\n", 1 );
            fprintf( log, "# pblListClone( %d )\n", 1 );

            clone = pblListClone( testCollection );
            if( !clone )
            {
                printf( "# NULL, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n", pbl_errno );
                break;
            }

            printf( "# ok size %d", pblListSize( clone ) );
            fprintf( log, "# ok size %d\n", pblListSize( clone ) );

            printf( "\n# pblListContainsAll( %d, YES )\n", 1 );
            fprintf( log, "# pblListContainsAll( %d, YES )\n", 1 );

            rc = pblListContainsAll( testCollection, clone );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );

            printf( "# pblListAdd( %d, NULL )\n", 1 );
            fprintf( log, "# pblListAdd( %d, NULL )\n", 1 );

            rc = pblListAdd(clone, NULL );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                break;
            }

            printf( "# pblListContainsAll( %d, NO )\n", 1 );
            fprintf( log, "# pblListContainsAll( %d, NO )\n", 1 );

            rc = pblListContainsAll( testCollection, clone );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );

            pblListFree( clone );
        }

        else if( !strcmp( command, "element" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListElement( %d )\n", 1 );
            fprintf( log, "# pblListElement( %d )\n", 1 );

            ptr = (char*)pblListElement( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# first %s", ptr ? ptr : "NULL" );
                fprintf( log, "# first %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "ensureCapacity" ))
        {
            if( !testCollection )
                continue;

            printf( "# ensureCapacity minCapacity\n" );
            fprintf( log, "# ensureCapacity minCapacity\n" );

            getWord( buffer );
            ival = atoi( buffer );

            printf( "# ensureCapacity( %d )\n",
                    ival );

            fprintf( log, "# ensureCapacity( %d )\n",
                     ival );

            rc = pblListEnsureCapacity( testCollection, ival );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "equals" ) )
        {
            PblList * clone;

            if( !testCollection )
                continue;

            printf( "# pblListClone( %d )\n", 1 );
            fprintf( log, "# pblListClone( %d )\n", 1 );

            clone = pblListClone( testCollection );
            if( !clone )
            {
                printf( "# NULL, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n", pbl_errno );
                break;
            }

            printf( "# ok size %d", pblListSize( clone ) );
            fprintf( log, "# ok size %d\n", pblListSize( clone ) );

            printf( "\n# pblListEquals( %d, YES )\n", 1 );
            fprintf( log, "# pblListEquals( %d, YES )\n", 1 );

            rc = pblListEquals( testCollection, testCollection );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );

            printf( "\n# pblListEquals( %d, YES )\n", 1 );
            fprintf( log, "# pblListEquals( %d, YES )\n", 1 );

            rc = pblListEquals( testCollection, clone );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );

            printf( "# pblListAdd( %d, NULL )\n", 1 );
            fprintf( log, "# pblListAdd( %d, NULL )\n", 1 );

            rc = pblListAdd(clone, NULL );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                break;
            }

            printf( "# pblListEquals( %d, NO )\n", 1 );
            fprintf( log, "# pblListEquals( %d, NO )\n", 1 );

            rc = pblListEquals( testCollection, clone );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n", rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );

            pblListFree( clone );
        }

        else if( !strcmp( command, "get" ))
        {
            if( !testCollection )
                continue;

            printf( "# get index\n" );
            fprintf( log, "# get index\n" );

            getWord( buffer );
            ival = atoi( buffer );

            printf( "# pblListGet( %d )\n", ival );
            fprintf( log, "# pblListGet( %d )\n", ival );

            ptr = (char*)pblListGet( testCollection, ival );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# get( %d )  %s", ival, ptr ? ptr : "NULL" );
                fprintf( log, "# get( %d ) %s\n", ival, ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "getCapacity" ))
        {
            if( !testCollection )
                continue;

            printf( "# getCapacity\n" );
            fprintf( log, "# getCapacity\n" );

            rc = pblListGetCapacity( testCollection );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "getCompareFunction" ))
        {
            if( !testCollection )
                continue;

            printf( "# getCompareFunction\n" );
            fprintf( log, "# getCompareFunction\n" );

            ptr = (char*)pblListGetCompareFunction( testCollection );

            printf( "# %s\n", ptr ? "FUNCTION SET" : "NULL" );
            fprintf( log, "# %s\n", ptr ? "FUNCTION SET" : "NULL" );
        }

        else if( !strcmp( command, "getFirst" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListGetFirst( %d )\n", 1 );
            fprintf( log, "# pblListGetFirst( %d )\n", 1 );

            ptr = (char*)pblListGetFirst( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# first %s", ptr ? ptr : "NULL" );
                fprintf( log, "# first %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "getLast" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListGetLast(  )\n" );
            fprintf( log, "# pblListGetLast(  )\n" );

            ptr = (char*)pblListGetLast( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# last %s", ptr ? ptr : "NULL" );
                fprintf( log, "# last %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "IndexOf" ))
        {
            if( !testCollection )
                continue;

            printf( "# IndexOf itemIndex\n" );
            fprintf( log, "# IndexOf itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListIndexOf( %s )\n",
                    ptr ? ptr : "NULL" );

            fprintf( log, "# pblListIndexOf( %s )\n",
                     ptr ? ptr : "NULL" );

            rc = pblListIndexOf( testCollection, ptr );

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "LastIndexOf" ))
        {
            if( !testCollection )
                continue;

            printf( "# LastIndexOf itemIndex\n" );
            fprintf( log, "# LastIndexOf itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListLastIndexOf( %s )\n",
                    ptr ? ptr : "NULL" );

            fprintf( log, "# pblListLastIndexOf( %s )\n",
                     ptr ? ptr : "NULL" );

            rc = pblListLastIndexOf( testCollection, ptr );

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "IsArrayList" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListIsArrayList( %d )\n", 1 );
            fprintf( log, "# pblListIsArrayList( %d )\n", 1 );

            rc = pblListIsArrayList( testCollection );

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "IsEmpty" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListIsEmpty( %d )\n", 1 );
            fprintf( log, "# pblListIsEmpty( %d )\n", 1 );

            rc = pblListIsEmpty( testCollection );

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "IsLinkedList" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListIsLinkedList( %d )\n", 1 );
            fprintf( log, "# pblListIsLinkedList( %d )\n", 1 );

            rc = pblListIsLinkedList( testCollection );

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "IsList" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListIsList( %d )\n", 1 );
            fprintf( log, "# pblListIsList( %d )\n", 1 );

            rc = pblListIsList( testCollection );

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "offer" ))
        {
            if( !testCollection )
                continue;

            printf( "# offer itemIndex\n" );
            fprintf( log, "# offer itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListOffer( %s )\n",
                    ptr ? ptr : "NULL" );

            fprintf( log, "# pblListOffer( %s )\n",
                     ptr ? ptr : "NULL" );

            rc = pblListOffer( testCollection, ptr );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "peek" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListPeek( %d )\n", 1 );
            fprintf( log, "# pblListPeek( %d )\n", 1 );

            ptr = (char*)pblListPeek( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# first %s", ptr ? ptr : "NULL" );
                fprintf( log, "# first %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "poll" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListPoll( %d )\n", 1 );
            fprintf( log, "# pblListPoll( %d )\n", 1 );

            ptr = (char*)pblListPoll( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# polled first %s", ptr ? ptr : "NULL" );
                fprintf( log, "# polled first %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "pop" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListPop( %d )\n", 1 );
            fprintf( log, "# pblListPop( %d )\n", 1 );

            ptr = (char*)pblListPop( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# popped last %s", ptr ? ptr : "NULL" );
                fprintf( log, "# popped last %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "push" ))
        {
            if( !testCollection )
                continue;

            printf( "# push itemIndex\n" );
            fprintf( log, "# push itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListPush( %s )\n",
                    ptr ? ptr : "NULL" );

            fprintf( log, "# pblListPush( %s )\n",
                     ptr ? ptr : "NULL" );

            rc = pblListPush( testCollection, ptr );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "remove" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListRemove( %d )\n", 1 );
            fprintf( log, "# pblListRemove( %d )\n", 1 );

            ptr = (char*)pblListRemove( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# removed %s", ptr ? ptr : "NULL" );
                fprintf( log, "# removed %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "removeAll" ))
        {
            PblList * clone = ( PblList *) 0;

            if( !testCollection )
                continue;

            printf( "# pblListRemoveAll( %d )\n", 1 );
            fprintf( log, "# pblListRemoveAll( %d )\n", 1 );

            clone = pblListClone( testCollection );
            if( !clone )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        (long)-1, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        (long)-1, pbl_errno );
                break;
            }

            rc = pblListRemoveAll( testCollection, clone );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                pblListFree( clone );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
            pblListFree( clone );
        }

        else if( !strcmp( command, "removeAt" ))
        {
            if( !testCollection )
                continue;

            printf( "# removeAt index\n" );
            fprintf( log, "# removeAt index\n" );

            getWord( buffer );
            ival = atoi( buffer );

            printf( "# pblListRemoveAt( %d )\n", ival );
            fprintf( log, "# pblListRemoveAt( %d )\n", ival );

            ptr = (char*)pblListRemoveAt( testCollection, ival );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# removed %d %s", ival, ptr ? ptr : "NULL" );
                fprintf( log, "# removed %d %s\n", ival, ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "removeElement" ))
        {
            if( !testCollection )
                continue;

            printf( "# removeElement itemIndex\n" );
            fprintf( log, "# removeElement itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListRemoveElement( %s )\n",
                    ptr ? ptr : "NULL" );

            fprintf( log, "# pblListRemoveElement( %s )\n",
                     ptr ? ptr : "NULL" );

            rc = pblListRemoveElement( testCollection, ptr );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "removeFirst" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListRemoveFirst( %d )\n", 1 );
            fprintf( log, "# pblListRemoveFirst( %d )\n", 1 );

            ptr = (char*)pblListRemoveFirst( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# removed first %s", ptr ? ptr : "NULL" );
                fprintf( log, "# removed first %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "removeLast" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListRemoveLast( %d )\n", 1 );
            fprintf( log, "# pblListRemoveLast( %d )\n", 1 );

            ptr = (char*)pblListRemoveLast( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# removed last %s", ptr ? ptr : "NULL" );
                fprintf( log, "# removed last %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "removeRange" ))
        {
            if( !testCollection )
                continue;

            printf( "# removeRange from to \n" );
            fprintf( log, "# removeRange from to\n" );

            getWord( buffer );
            ival = atoi( buffer );

            getWord( command );
            i = atoi( command );

            printf( "# pblListRemoveRange( %d, %d )\n", ival, i );
            fprintf( log, "# pblListRemoveRange( %d, %d )\n",ival, i );

            rc = pblListRemoveRange( testCollection, ival, i );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
            }
            else
            {
                printf( "# rc %ld\n", rc );
                fprintf( log, "# rc %ld\n", rc );
            }
        }

        else if( !strcmp( command, "retainAll" ))
        {
            PblList * clone = ( PblList *) 0;

            if( !testCollection )
                continue;

            printf( "# pblListClone( %d )\n", 1 );
            fprintf( log, "# pblListClone( %d )\n", 1 );

            clone = pblListClone( testCollection );
            if( !clone )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        (long)-1, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        (long)-1, pbl_errno );
                break;
            }

            printf( "# pblListRetainAll( %d )\n", 1 );
            fprintf( log, "# pblListRetainAll( %d )\n", 1 );

            rc = pblListRetainAll( testCollection, clone );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                pblListFree( clone );
                break;
            }


            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );

            printf( "# pblListRemoveAt( clone, 3 )\n" );
            fprintf( log, "# pblListRemoveAt( clone, 3 )\n" );

            ptr = (char*)pblListRemoveAt( clone, 3 );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# removed 3rd %s\n", ptr ? ptr : "NULL" );
                fprintf( log, "# removed 3d %s\n", ptr ? ptr : "NULL" );
            }

            printf( "# pblListRemoveFirst( clone )\n" );
            fprintf( log, "# pblListRemoveFirst( clone )\n" );

            ptr = (char*)pblListRemoveFirst( clone );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# removed first %s\n", ptr ? ptr : "NULL" );
                fprintf( log, "# removed first %s\n", ptr ? ptr : "NULL" );
            }

            printf( "# pblListRemoveFirst( clone )\n" );
            fprintf( log, "# pblListRemoveFirst( clone )\n" );

            ptr = (char*)pblListRemoveLast( clone );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# removed last %s\n", ptr ? ptr : "NULL" );
                fprintf( log, "# removed last %s\n", ptr ? ptr : "NULL" );
            }

            printf( "# pblListRetainAll( %d )\n", 1 );
            fprintf( log, "# pblListRetainAll( %d )\n", 1 );

            rc = pblListRetainAll( testCollection, clone );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                pblListFree( clone );
                break;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );

            pblListFree( clone );
        }

        else if( !strcmp( command, "reverse" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListReverse( %d )\n", 1 );
            fprintf( log, "# pblListReverse( %d )\n", 1 );

            pblListReverse( testCollection );

            printf( "# ok" );
            fprintf( log, "# ok\n" );
        }

        else if( !strcmp( command, "set" ))
        {
            if( !testCollection )
                continue;

            printf( "# set index itemIndex\n" );
            fprintf( log, "# set index itemIndex\n" );

            getWord( command );
            i = atoi( command );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListSet( %d, %s )\n",
                    i, ptr ? ptr : "NULL" );

            fprintf( log, "# pblListSet( %d, %s )\n",
                    i, ptr ? ptr : "NULL" );

            ptr = (char*)pblListSet( testCollection, i, ptr );
            if( ptr == (char *)-1 )
            {
                printf( "# SET ERROR, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# SET ERROR, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# set returned %s\n", ptr ? ptr : "NULL" );
                fprintf( log, "# set returned %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "setFirst" ))
        {
            if( !testCollection )
                continue;

            printf( "# setFirst itemIndex\n" );
            fprintf( log, "# setFirst itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListSetFirst( %s )\n", ptr ? ptr : "NULL" );

            fprintf( log, "# pblListSetFirst( %s )\n", ptr ? ptr : "NULL");

            ptr = (char*)pblListSetFirst( testCollection, ptr ? ptr : "NULL" );
            if( ptr == (char *)-1 )
            {
                printf( "# SET ERROR, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# SET ERROR, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# set returned %s\n", ptr ? ptr : "NULL" );
                fprintf( log, "# set returned %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "setLast" ))
        {
            if( !testCollection )
                continue;

            printf( "# setLast itemIndex\n" );
            fprintf( log, "# setLast itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblListSetLast( %s )\n", ptr ? ptr : "NULL" );

            fprintf( log, "# pblListSetLast( %s )\n", ptr ? ptr : "NULL");

            ptr = (char*)pblListSetLast( testCollection, ptr );
            if( ptr == (char *)-1 )
            {
                printf( "# SET ERROR, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# SET ERROR, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# set returned %s\n", ptr ? ptr : "NULL" );
                fprintf( log, "# set returned %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "setSize" ))
        {
            if( !testCollection )
                continue;

            printf( "# setSize size\n" );
            fprintf( log, "# setSize size\n" );

            getWord( buffer );
            ival = atoi( buffer );

            printf( "# pblListSetSize( %d )\n", ival );
            fprintf( log, "# pblListSetSize( %d )\n",ival );

            rc = pblListSetSize( testCollection, ival );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
            }
            else
            {
                printf( "# rc %ld\n", rc );
                fprintf( log, "# rc %ld\n", rc );
            }
        }

        else if( !strcmp( command, "size" ))
        {
            if( !testCollection )
                continue;


            printf( "# pblListSize(  )\n"  );
            fprintf( log, "# pblListSize(  )\n" );

            rc = pblListSize( testCollection );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }
            else
            {
                printf( "# rc %ld\n", rc );
                fprintf( log, "# rc %ld\n", rc );
            }
        }

        else if( !strcmp( command, "sort" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListSort( %d, pblCollectionStringCompareFunction )\n", 1 );
            fprintf( log, "# pblListSort( %d, pblCollectionStringCompareFunction )\n", 1 );

            rc = pblListSort( testCollection, pblCollectionStringCompareFunction );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                break;
            }

            printf( "# ok" );
            fprintf( log, "# ok\n" );
        }

        else if( !strcmp( command, "strcmp" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListSetCompareFunction( %d )\n", 1 );
            fprintf( log, "# pblListSetCompareFunction( %d )\n", 1 );

            pblListSetCompareFunction( testCollection, pblCollectionStringCompareFunction );

            printf( "# ok" );
            fprintf( log, "# ok\n" );
        }

        else if( !strcmp( command, "toArray" ))
        {
            char ** array;
            if( !testCollection )
                continue;

            printf( "# pblListToArray( )\n" );
            fprintf( log, "# pblListToArray( )\n" );

            array = (char**)pblListToArray( testCollection );
            if( array == NULL )
            {
                printf( "# NULL, pbl_errno %d\n",
                        pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n",
                        pbl_errno );
                continue;
            }

            printf( "# OK\n" );
            fprintf( log, "# OK\n" );

            for( i = 0; i < pblListSize( testCollection ); i++ )
            {
                char * item =  array[i];

                printf( "# OK i %d, %s\n", i, item != NULL ? item : "NULL" );
                fprintf( log, "# OK i %d, %s\n", i, item != NULL ? item : "NULL" );
            }

            PBL_FREE(array );
            printf( "# Array Iteration DONE\n" );
            fprintf( log, "# Array Iteration DONE\n" );
        }

        else if( !strcmp( command, "top" ) )
        {
            if( !testCollection )
                continue;

            printf( "# pblListTop(  )\n" );
            fprintf( log, "# pblListTop(  )\n" );

            ptr = (char*)pblListTop( testCollection );
            if( ptr == (char *)-1 )
            {
                printf( "# NOT FOUND, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NOT FOUND, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# top %s", ptr ? ptr : "NULL" );
                fprintf( log, "# top %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "trim" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListTrimToSize( )\n" );
            fprintf( log, "# pblListTrimToSize( )\n" );

            rc = pblListTrimToSize( testCollection );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
            }
            else
            {
                printf( "# rc %ld\n", rc );
                fprintf( log, "# rc %ld\n", rc );
            }
        }

        else if( !strcmp( command, "iterate" ))
        {
            PblIterator  * iterator;

            i = 0;

            if( !testCollection )
                continue;

            printf( "# iterate maxIndex\n" );
            fprintf( log, "# iterate maxIndex\n" );
            getWord( buffer );
            ival = atoi( buffer );

            printf( "# pblListIterator( %d )\n",
                    ival );

            fprintf( log, "# pblListIterator( %d )\n",
                    ival );

            iterator = pblListIterator( testCollection );

            if( iterator == NULL )
            {
                printf( "# NULL, pbl_errno %d\n",
                        pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n",
                        pbl_errno );
                break;
            }

            printf( "# OK\n" );
            fprintf( log, "# OK\n" );


            while( ival-- > 0 && pblIteratorHasNext( iterator ))
            {
                char * item =  ( char *) pblIteratorNext( iterator );
                if( item == (void*)-1 )
                {
                    printf( "# FAILED i %d, pbl_errno %d\n", i, pbl_errno );
                    fprintf( log, "# FAILED i %d, pbl_errno %d\n",
                             i, pbl_errno );
                    break;
                }

                printf( "# OK i %d, %s\n", i, item != NULL ? item : "NULL" );
                fprintf( log, "# OK i %d, %s\n", i, item != NULL ? item : "NULL" );
                i++;
            }

            pblIteratorFree( iterator );
            printf( "# Iteration DONE\n" );
            fprintf( log, "# Iteration DONE\n" );

        }

        else if( !strcmp( command, "reverseIterate" ))
        {
            PblIterator  * iterator;

            i = 0;

            if( !testCollection )
                continue;

            printf( "# reverseIterate maxIndex\n" );
            fprintf( log, "# reverseIterate maxIndex\n" );
            getWord( buffer );
            ival = atoi( buffer );

            printf( "# pblListReverseIterator( %d )\n",
                    ival );

            fprintf( log, "# pblListReverseIterator( %d )\n",
                    ival );

            iterator = pblListReverseIterator( testCollection );

            if( iterator == NULL )
            {
                printf( "# NULL, pbl_errno %d\n",
                        pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n",
                        pbl_errno );
                break;
            }

            printf( "# OK\n" );
            fprintf( log, "# OK\n" );


            while( ival-- > 0 && pblIteratorHasPrevious( iterator ))
            {
                char * item =  ( char *) pblIteratorPrevious( iterator );
                if( item == (void*)-1 )
                {
                    printf( "# FAILED i %d, pbl_errno %d\n", i, pbl_errno );
                    fprintf( log, "# FAILED i %d, pbl_errno %d\n",
                             i, pbl_errno );
                    break;
                }

                printf( "# OK i %d, %s\n", i, item != NULL ? item : "NULL" );
                fprintf( log, "# OK i %d, %s\n", i, item != NULL ? item : "NULL" );
                i++;
            }

            pblIteratorFree( iterator );
            printf( "# Iteration DONE\n" );
            fprintf( log, "# Iteration DONE\n" );

        }

        else if( !strcmp( command, "iterator" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListIterator( )\n" );
            fprintf( log, "# pblListIterator()\n" );

            testIterator = pblListIterator( testCollection );
            if( testIterator == NULL )
            {
                printf( "# NULL, pbl_errno %d\n",
                        pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n",
                        pbl_errno );
                break;
            }

            printf( "# OK\n" );
            fprintf( log, "# OK\n" );
        }

        else if( !strcmp( command, "reverseIterator" ))
        {
            if( !testCollection )
                continue;

            printf( "# pblListReverseIterator( )\n" );
            fprintf( log, "# pblListReverseIterator()\n" );

            testIterator = pblListReverseIterator( testCollection );
            if( testIterator == NULL )
            {
                printf( "# NULL, pbl_errno %d\n",
                        pbl_errno );
                fprintf( log, "# NULL, pbl_errno %d\n",
                        pbl_errno );
                break;
            }

            printf( "# OK\n" );
            fprintf( log, "# OK\n" );
        }

        else if( !strcmp( command, "iteratorAdd" ))
        {
            if( !testIterator )
                continue;

            printf( "# iteratorAdd itemIndex\n" );
            fprintf( log, "# iteratorAdd itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblIteratorAdd( %s )\n",
                    ptr ? ptr : "NULL" );

            fprintf( log, "# pblIteratorAdd( %s )\n",
                     ptr ? ptr : "NULL" );

            rc = pblIteratorAdd( testIterator, ptr );

            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                continue;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "iteratorFree" ))
        {
            if( !testIterator )
                continue;

            printf( "# pblIteratorFree( )\n" );
            fprintf( log, "# pblIteratorFree()\n" );

            pblIteratorFree( testIterator );
            testIterator = NULL;

            printf( "# OK\n" );
            fprintf( log, "# OK\n" );
        }

        else if( !strcmp( command, "hasNext" ))
        {
            if( !testIterator )
                continue;

            printf( "# pblIteratorHasNext( )\n" );
            fprintf( log, "# pblIteratorHasNext()\n" );

            rc = pblIteratorHasNext( testIterator );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                continue;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "hasPrevious" ))
        {
            if( !testIterator )
                continue;

            printf( "# pblIteratorHasPrevious( )\n" );
            fprintf( log, "# pblIteratorHasPrevious()\n" );

            rc = pblIteratorHasPrevious( testIterator );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                continue;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "next" ))
        {
            if( !testIterator )
                continue;

            printf( "# pblIteratorNext( )\n" );
            fprintf( log, "# pblIteratorNext()\n" );

            ptr = (char*)pblIteratorNext( testIterator );
            if( ptr == (char *)-1 )
            {
                printf( "# NO NEXT, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NO NEXT, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# next %s", ptr ? ptr : "NULL" );
                fprintf( log, "# next %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "nextIndex" ))
        {
            if( !testIterator )
                continue;

            printf( "# pblIteratorNextIndex( )\n" );
            fprintf( log, "# pblIteratorNextIndex()\n" );

            rc = pblIteratorNextIndex( testIterator );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                continue;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "previous" ))
        {
            if( !testIterator )
                continue;

            printf( "# pblIteratorPrevious( )\n" );
            fprintf( log, "# pblIteratorPrevious()\n" );

            ptr = (char*)pblIteratorPrevious( testIterator );
            if( ptr == (char *)-1 )
            {
                printf( "# NO PREVIOUS, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# NO PREVIOUS, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# previous %s", ptr ? ptr : "NULL" );
                fprintf( log, "# previous %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "previousIndex" ))
        {
            if( !testIterator )
                continue;

            printf( "# pblIteratorPreviousIndex( )\n" );
            fprintf( log, "# pblIteratorPreviousIndex()\n" );

            rc = pblIteratorPreviousIndex( testIterator );

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "iteratorRemove" ))
        {
            if( !testIterator )
                continue;

            printf( "# pblIteratorRemove( )\n" );
            fprintf( log, "# pblIteratorRemove()\n" );

            rc = pblIteratorRemove( testIterator );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                continue;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else if( !strcmp( command, "iteratorSet" ))
        {
            if( !testIterator )
                continue;

            printf( "# iteratorSet itemIndex\n" );
            fprintf( log, "# iteratorSet itemIndex\n" );

            getWord( buffer );
            ival = atoi( buffer );
            if( ival < 0)
            {
                ptr = NULL;
            }
            else
            {
                ptr = items[ ival ];
            }

            printf( "# pblIteratorSet( %s )\n",
                    ptr ? ptr : "NULL" );

            fprintf( log, "# pblIteratorSet( %s )\n",
                     ptr ? ptr : "NULL" );

            ptr = (char*)pblIteratorSet( testIterator, ptr );
            if( ptr == (char *)-1 )
            {
                printf( "# SET FAILED, pbl_errno %d\n", pbl_errno );
                fprintf( log, "# SET FAILED, pbl_errno %d\n", pbl_errno );
            }
            else
            {
                printf( "# set returned %s", ptr ? ptr : "NULL" );
                fprintf( log, "# set returned %s\n", ptr ? ptr : "NULL" );
            }
        }

        else if( !strcmp( command, "iteratorSize" ))
        {
            if( !testIterator )
                continue;

            printf( "# pblIteratorSize( )\n" );
            fprintf( log, "# pblIteratorSize()\n" );

            rc = pblIteratorSize( testIterator );
            if( rc < 0 )
            {
                printf( "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                fprintf( log, "# rc %ld, pbl_errno %d\n",
                        rc, pbl_errno );
                continue;
            }

            printf( "# rc %ld\n", rc );
            fprintf( log, "# rc %ld\n", rc );
        }

        else
        {
            printf( "# q       FOR QUIT\n" );
        }
    }
	if( argc > 1 )
	{
		fflush( log );
		if( diffFiles( infile, logFileName ))
		{
			return -1;
		}
	}
    return 0;
}

/*
 * Eclipse CDT does not like more than one main,
 * therefore hide all but one main with this -D option
 */
#ifdef _WIN32
#define ALTST_SHOW_MAIN
#endif

#ifndef CDT_BUILD
#define ALTST_SHOW_MAIN
#endif

#ifdef PBLTEST
#undef ALTST_SHOW_MAIN
#define ALTST_SHOW_MAIN 1
#endif

#ifdef CDT_BUILD
#undef ALTST_SHOW_MAIN
#endif

#ifdef ALTST_SHOW_MAIN

int main( int argc, char * argv[] )
{
    return pblList_TestFrame( argc, argv );
}

#endif /* CDT_BUILD */
