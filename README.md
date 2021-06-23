<A href="http://www.mission-base.com/peter/source/"><img src="http://www.mission-base.com/index-images/mb240.gif"></img></A> PBL - The Program Base Library 

* * * * *

The library PBL is published under the MIT License, see [https://opensource.org/licenses/MIT].

The test cases, which are not directly part of the library, are published under 
[The GNU General Public License][] 
as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

## PBL - Introduction

PBL is an MIT License open source C library of functions that can be used in a C
or C++ project. PBL is highly portable and compiles warning free on
Linux gcc, MAC OS X and Windows Microsoft Visual Studio.

The code of the PBL library includes the following modules:

###[**PBL BASE**][] - C Base Functions
Some base functions, see **pbl\_*** functions,

###[**PBL LIST**][] - C Array-List and Linked-List
An open source C implementation of array lists and
linked lists similar to the [Java List][] interface, see **pblList***
functions,

-   [**pblArrayList**][**PBL LIST**]:
    Open source C resizable-array implementation equivalent to the [Java
    ArrayList][] class.

    Implements most optional list operations, and permits all elements,
    including NULL. In addition to implementing the List operations,
    this module provides methods to manipulate the size of the array
    that is used internally to store the list.

    The size, isEmpty, get, set, iterator, and listIterator operations
    run in constant time. The add operation runs in amortized constant
    time, that is, adding n elements requires O(n) time. All of the
    other operations run in linear time (roughly speaking). The constant
    factor is low compared to that for the LinkedList implementation.

    Each pblArrayList instance has a capacity. The capacity is the size
    of the array used to store the elements in the list. It is always at
    least as large as the list size. As elements are added to an
    ArrayList, its capacity grows automatically. The details of the
    growth policy are not specified beyond the fact that adding an
    element has constant amortized time cost.

    An application can increase the capacity of an ArrayList instance
    before adding a large number of elements using the ensureCapacity
    operation. This may reduce the amount of incremental reallocation.
-   [**pblLinkedList**][**PBL LIST**]:
    Open source C linked list implementation equivalent to the [Java
    LinkedList][] class.

    Implements most optional list operations, and permits all elements
    (including null). In addition to implementing the List operations,
    this module provides uniformly named methods to get, remove and
    insert an element at the beginning and end of the list. These
    operations allow linked lists to be used as a stack, queue, or
    double-ended queue (deque).

    The module implements the Queue operations, providing
    first-in-first-out queue operations for add, poll, etc. Other stack
    and deque operations could be easily recast in terms of the standard
    list operations.

    All of the operations perform as could be expected for a
    doubly-linked list. Operations that index into the list will
    traverse the list from the beginning or the end, whichever is closer
    to the specified index.
-   [**pblIterator**][]:
    Open source C Iterator implementation equivalent to the [Java ListIterator][] interface.

    An iterator for lists that allows the programmer to traverse the
    list in either direction, modify the list during iteration, and
    obtain the iterator's current position in the list. A ListIterator
    has no current element; its cursor position always lies between the
    element that would be returned by a call to previous() and the
    element that would be returned by a call to next(). In a list of
    length n, there are n+1 valid index values, from 0 to n, inclusive.

    Note that the remove() and set(Object) methods are not defined in
    terms of the cursor position; they are defined to operate on the
    last element returned by a call to next() or previous().

-   [**pblCollection**][]:
    Open source C implementation of a collection used by the list and 
    set implementations implementation equivalent to the [Java Collection][] interface.

###[**PBL Set**][] - C Hash-Set and Tree-Set
An open source C implementation of hash sets and tree
sets similar to the [Java Set][] interface, see **pblSet*** functions,

-   [**pblHashSet**][**PBL Set**]:
    Open source C resizable hash set implementation equivalent to the
    [Java HashSet][] class.

    Hash sets make no guarantees as to the iteration order of the set;
    in particular, it does not guarantee that the order will remain
    constant over time. This module does not permit the NULL element.

    Hash sets offer constant time performance for the basic operations
    (add, remove, contains and size), assuming the hash function
    disperses the elements properly among the buckets. Iterating over
    this set requires time proportional to the sum of the HashSet
    instance's size (the number of elements) plus the "capacity" of the
    instance (the number of buckets). Thus, it's very important not to
    set the initial capacity too high (or the load factor too low) if
    iteration performance is important. [][**PBL Set**]
-   [**pblTreeSet**][**PBL Set**]:
    Open source C avl-tree-based balanced tree set implementation
    equivalent to the [Java TreeSet][] class.

    Tree sets guarantees that the sorted set will be in ascending
    element order, sorted according to the natural order of the
    elements, or by the comparator provided.

    This implementation provides guaranteed log(n) time cost for the
    basic operations (add, remove and contains).

###[**PBL Map**][] - C Hash-Map and Tree-Map
An open source C implementation of hash maps and tree
maps similar to the [Java Map][] interface, see **pblMap*** functions,

-   [**pblHashMap**][**PBL Map**]:
     Open source C resizable hash map implementation equivalent to the
    [Java HashMap][] class.
     Hash maps make no guarantees as to the iteration order of the set;
    in particular, it does not guarantee that the order will remain
    constant over time.
     Hash maps offer constant time performance for the basic operations
    (add, remove, contains and size), assuming the hash function
    disperses the elements properly among the buckets. Iterating over
    this map requires time proportional to the sum of the HashMap
    instance's size (the number of elements) plus the "capacity" of the
    instance (the number of buckets). Thus, it's very important not to
    set the initial capacity too high (or the load factor too low) if
    iteration performance is important.
    [][**PBL Map**]

-   [**pblTreeMap**][**PBL Map**]:
     Open source C avl-tree-based balanced tree map implementation
    equivalent to the [Java TreeMap][] class. 
     Tree maps guarantee that the sorted map will be in ascending
    element order, sorted according to the natural order of the
    elements, or by the comparator provided. 
     This implementation provides guaranteed log(n) time cost for the
    basic operations (add, remove and contains).

###[**PBL HEAP**][] - C Binary Min-Max Heap

###[**PBL PRIORITY QUEUE**][] - C Priority Queue

###[**PBL HASH**][] - C Memory Hash Table
An open source C memory hash table implementation, see **pblHt***
functions,

**Features**
-   random access lookups
-   sequential access
-   regression test frame

###[**PBL KEYFILE**][] - C Key-File
An open source C key file implementation, see **pblKf*** functions,

**Features**
-   ultra fast B\* tree implementation for random lookups
-   transaction handling
-   sequential access methods
-   embedable small footprint, < 35 Kb
-   arbitrary size files, up to 4 terrabytes
-   arbitrary number of records per file, up to 2 \^\^ 48 records
-   duplicate keys
-   advanced key compression for minimal size B trees
-   keylength up to 255 bytes
-   regression test frame

###[**PBL ISAM**][] - C ISAM-File
An open source C ISAM file implementation, see **pblIsam*** functions

**Features**
-   ultra fast B\* tree implementation for random lookups
-   transaction handling
-   sequential access methods
-   embedable small footprint, < 75 Kb
-   arbitrary size files, up to 4 terrabytes
-   arbitrary number of records per file, up to 2 \^\^ 48 records
-   duplicate keys and unique keys
-   advanced key compression for minimal size index files
-   keylength up to 255 bytes per index
-   keylength up to 1024 per record
-   regression test frame

###[**AvlDictionary\<TKey,TValue\>**][**AvlDictionary<TKey,TValue\>**] - C# .NET Avl-Tree 
C# .NET Avl-Tree based generic IDictionary\<TKey,TValue\>

AvlDictionary<TKey,TValue> is an open source C# Avl-Tree based generic IDictionary\<TKey,TValue\> implementation. See the [**AvlDictionary documentation**][].

**Features**
-   implements generic IDictionary<TKey, TValue> interface
-   implements generic ICollection<KeyValuePair<TKey, TValue>> interface
-   implements generic IEnumerable<KeyValuePair<TKey, TValue>> interface
-   [Serializable] 

In order to use AvlDictionary<TKey,TValue> copy [**AvlDictionary.cs**][] to your solution and use the AVL-Tree based generic [**AvlDictionary<TKey,TValue\>**][] like you use the hash based generic Dictionary\<TKey,TValue\>.

###[**PriorityQueue\<T\>**][**PriorityQueue<T\>**] - C# .NET Min-Heap
C# .NET List<T> based generic min-heap [**PriorityQueue<T\>**]. See the [**PriorityQueue documentation**][].

In order to use [**PriorityQueue\<T\>**][**PriorityQueue<T\>**] copy [**PriorityQueue.cs**][] to your solution and use the List<T\> based generic min-heap PriorityQueue<T\>. 

## MORE:

-   See the PBL [documentation][].
-   Take a look at [Spam Probe][], a project that uses PBL.

* * * * *

copyright (C) 2001 - 2015 by Peter Graf

  [The GNU General Public License]: http://www.gnu.org/licenses/licenses.html#GPL
  [**PBL BASE**]: http://www.mission-base.com/peter/source/pbl/doc/base.html
  [**PBL COLLECTION**]: http://www.mission-base.com/peter/source/pbl/doc/collection.html
  [**PBL LIST**]: http://www.mission-base.com/peter/source/pbl/doc/list.html
  [Java List]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/List.html
  [Java ArrayList]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/ArrayList.html
  [Java LinkedList]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/LinkedList.html
  [**pblIterator**]: http://www.mission-base.com/peter/source/pbl/doc/iterator.html
  [Java ListIterator]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/ListIterator.html
  [**pblCollection**]: http://www.mission-base.com/peter/source/pbl/doc/collection.html
  [Java Collection]:  http://docs.oracle.com/javase/1.5.0/docs/api/java/util/Collection.html
  [**PBL Set**]: http://www.mission-base.com/peter/source/pbl/doc/set.html
  [Java Set]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/Set.html
  [Java HashSet]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/HashSet.html
  [Java TreeSet]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/TreeSet.html
  [**PBL Map**]: http://www.mission-base.com/peter/source/pbl/doc/map.html
  [Java Map]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/Map.html
  [Java HashMap]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/HashMap.html
  [Java TreeMap]: http://java.sun.com/j2se/1.5.0/docs/api/java/util/TreeMap.html
  [**PBL HEAP**]: http://www.mission-base.com/peter/source/pbl/doc/heap.html
  [**PBL PRIORITY QUEUE**]: http://www.mission-base.com/peter/source/pbl/doc/priorityQueue.html
  [**PBL HASH**]: http://www.mission-base.com/peter/source/pbl/doc/hash.html
  [**PBL KEYFILE**]: http://www.mission-base.com/peter/source/pbl/doc/keyfile.html
  [**PBL ISAM**]: http://www.mission-base.com/peter/source/pbl/doc/isamfile.html
  [**AvlDictionary<TKey,TValue\>**]: ./src/dotNETsrc/AvlDictionary.cs
  [**AvlDictionary documentation**]: http://www.mission-base.com/peter/source/AvlDictionary/
  [**AvlDictionary.cs**]: ./src/dotNETsrc/AvlDictionary.cs
  [**PriorityQueue<T\>**]: ./src/dotNETsrc/PriorityQueue.cs  
  [**PriorityQueue documentation**]: http://www.mission-base.com/peter/source/html/_priority_queue_8cs.html
  [**PriorityQueue.cs**]: ./src/dotNETsrc/PriorityQueue.cs
  [documentation]: http://www.mission-base.com/peter/source/pbl/doc/
  [Version 1.04 tar source]: pbl_1_04.tar.gz
  [sources]: http://www.mission-base.com/peter/source/pbl/
  [Spam Probe]: http://spamprobe.sourceforge.net/
