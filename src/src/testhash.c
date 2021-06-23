#include <stdio.h>
#include <memory.h>

#define PBL_HASHTABLE_SIZE      1019

int hash1( const unsigned char * key, size_t keylen )
{
    int ret = 104729;

    for( ; keylen-- > 0; key++ )
    {
        if( *key )
        {
            ret *= *key + keylen;
        }
    }

    return ( ret & 0x7fffffff ) % PBL_HASHTABLE_SIZE;
}

/*
 * I took the code from the net, it contained the following:
 * Author J. Zobel, April 2001.
   Permission to use this code is freely granted, provided that this
   statement is retained.
 */
int hash2( const unsigned char * key, size_t keylen )
{
    int ret = 104729;

    for( ; keylen-- > 0; key++ )
    {
	ret ^= ( (ret << 5) + *key + (ret >> 2) );
    }

    return ( ret & 0x7fffffff ) % PBL_HASHTABLE_SIZE;
}

/*
--------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.
For every delta with one or two bits set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
* If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
* If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
mix() was built out of 36 single-cycle latency instructions in a
  structure that could supported 2x parallelism, like so:
      a -= b;
      a -= c; x = (c>>13);
      b -= c; a ^= x;
      b -= a; x = (a<<8);
      c -= a; b ^= x;
      c -= b; x = (b>>13);
      ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
--------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  len     : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 6*len+35 instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

unsigned long hash3(
unsigned char * k,
unsigned long length
)
{
   register unsigned long a,b,c,len;

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = 104729;

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((unsigned long)k[1]<<8) +((unsigned long)k[2]<<16) +((unsigned long)k[3]<<24));
      b += (k[4] +((unsigned long)k[5]<<8) +((unsigned long)k[6]<<16) +((unsigned long)k[7]<<24));
      c += (k[8] +((unsigned long)k[9]<<8) +((unsigned long)k[10]<<16)+((unsigned long)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((unsigned long)k[10]<<24);
   case 10: c+=((unsigned long)k[9]<<16);
   case 9 : c+=((unsigned long)k[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((unsigned long)k[7]<<24);
   case 7 : b+=((unsigned long)k[6]<<16);
   case 6 : b+=((unsigned long)k[5]<<8);
   case 5 : b+=k[4];
   case 4 : a+=((unsigned long)k[3]<<24);
   case 3 : a+=((unsigned long)k[2]<<16);
   case 2 : a+=((unsigned long)k[1]<<8);
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);

   /*-------------------------------------------- report the result */
   return ( c & 0x7fffffff ) % PBL_HASHTABLE_SIZE;
}

unsigned long hash4(
    unsigned char * key,
    size_t          length
)
/*  =DocEnd= */
{
    unsigned long val = 0x9e3779b9;
    unsigned long v;
    int           c;

    while( length >= sizeof( v ))
    {
        memcpy( &v, key, sizeof( v ));
        key    += sizeof( v );
        length -= sizeof( v );

        val ^= v;
        val %= PBL_HASHTABLE_SIZE;
    }
    val %= PBL_HASHTABLE_SIZE;

    while( length-- > 0 )
    {
        if(( c = *key++ ))
        {
            val *= c;
            val %= PBL_HASHTABLE_SIZE;
        }
    }

    return val & 0x7fffffff;
}

typedef struct PBLBLOCKREF_s
{
    long           blockno;             /* block number in file               */
    long           bf;                  /* big file handle block belongs to   */

} PBLBLOCKREF_t;

/*
 * Eclipse CDT does not like more than one main,
 * therefore hide all but one main with this -D option
 */
#ifndef CDT_BUILD

main( int argc, char * argv[] )
{
    int i = 0;

    char * s1 = "110123456789abcdef";
    char * s2 = "ff0123456789abcdef";

    PBLBLOCKREF_t ref1;
    PBLBLOCKREF_t ref2;

    ref1.bf = 1;
    ref2.bf = 2;

/*
    for( i = 0; i < 10; i++ )
    {
        ref1.blockno = i;
        ref2.blockno = i;
        printf( "%4d %4d\n",
                hash3( (unsigned char *)&ref1, sizeof( ref1 )),
                hash3( (unsigned char *)&ref2, sizeof( ref2 )));
    }

*/
/*
    printf( "%d\n", hash3( s1, strlen( s1 )));
    printf( "%d\n", hash3( s2, strlen( s2 )));
*/

    for( i = 0; i < 100000000; i++ )
    {
        int j = hash4( "hello hello world\n", 18 );
        if( j < 0 )
        {
            exit( 0 );
        }
    }

    return 0;
}

#endif /* CDT_BUILD */
