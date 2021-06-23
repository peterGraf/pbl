/*
 * qsortTest.c, tests gcc's memory allocation
 *
 * gcc-4.0.1 shows no difference, but starting from gcc-4.1.2
 *
 * If the items are printed before they are asigned to pointerArray they are allocated
 * in order of definition.
 *
 * gcc -O3 -DPRINT_ITEMS qsortTest.c ; ./a.out
 *
 * If the items are first asigned to pointerArray they are allocated in order of
 * assignment.
 *
 * gcc -O3 qsortTest.c ; ./a.out
 */

#include <stdio.h>
#include <stdlib.h>

static char * items[] = { "0", "1", "2" };

static int compare( const void *left, const void *right )
{
    char * leftPointer = *(char**)left;
    char * rightPointer = *(char**)right;

    /*
     * Uses the addresses of the objects for the compare!!!
     */
    if( leftPointer < rightPointer )
    {
        printf( "compare %p, %p rc -1\n", leftPointer, rightPointer);

        return -1;
    }

    if( leftPointer == rightPointer )
    {
        printf( "compare %p, %p rc 0\n", leftPointer, rightPointer);

        return 0;
    }

    printf( "compare %p, %p rc 1\n", leftPointer, rightPointer);

    return 1;
}

int qsortTest( int argc, char * argv[] )
{
    int i;
    char * pointerArray[3];

#ifdef PRINT_ITEMS
    for( i = 0; i < 3; i++ )
    {
        printf( "item %d: %p, %s\n", i, items[i], items[i]);
    }
    printf("\n");
#endif

    /*
     * Create an unsorted array
     */
    pointerArray[0] = items[2];
    pointerArray[1] = items[0];
    pointerArray[2] = items[1];

#ifdef PRINT_ITEMS
    for( i = 0; i < 3; i++ )
    {
        printf( "item %d: %p, %s\n", i, items[i], items[i]);
    }
    printf("\n");
#endif

    for( i = 0; i < 3; i++ )
    {
        printf( "pointerArray %d: %p, %s\n", i, pointerArray[i], pointerArray[i]);
    }
    printf("\n");

    /*
     * Sort the array according to the addresses of the items!
     */
    qsort( pointerArray, (size_t)3, (size_t)sizeof(char*), compare );
    printf("\n");

    /*
     * Print the result
     */
    for( i = 0; i < 3; i++ )
    {
        printf( "pointerArray %d: %p, %s\n", i, pointerArray[i], pointerArray[i]);
    }

    return 0;
}


#ifdef _CDT_BUILD

int main( int argc, char * argv[] )
{
    return qsortTest( arc, argv );
}
#endif
