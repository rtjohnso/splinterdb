# SplinterDB Architecture
This document describes the overall Architecture of SplinterDB.

# High-Level Architecture

At the heart of SplinterDB is the Size-Tiered Bε-tree (STBε-tree), a novel data structure that
combines designs from log-structured merge (LSM) trees and Bε-trees. The STBε-tree is the main
data structure where all key-value pairs are stored.

 * Offer single-row inserts / updates / deletes
 * Supports safe, concurrent operations writes ... gets/ updates ...

 * If you are using the system, what are the promises it is making. And what external behaviour
   users can se -- should be defined in some user-guide, in user-visible doc.

 * Explain in a page ... where properties that SplinterDB offers are explained. perf, consistency and
   semantics are described.

 * ? We are not ACID Compliant updates? ... as people think
 * ? Persistence - one line?

 * Visibility: Scan dynamics.

* Propopsed structure:
    * Basic APIs: put, scan : Describe guarantees of behaviour
    * In dev-docs: Describe how the guarantees are implemented.
    * Doc on concurrency
    * Doc on persistence
    * Users: Behaviour of concurrent writes
    * Devs: Complexity of concurrency implementation / designs

SplinterDB deployed on fast NVMe SSDs leverages the low I/O latency and high bandwidth of such
devices. Consequently, the performance bottlenecks shift to scalability concerns in multi-core
CPU processing. SplinterDB is architected to exploit this I/O and CPU concurrency to deliver
high performance.

# Components

At a high-level, SplinterDB is comprised of the following sub-systems:

## Disk-Resident Artifacts

* Single file for the Key-Value Store data (data device and log)

    * ? What do we want to say here about file-system devices? RAM-devices? Carving up NVMe into multiple devices and so on ...?

* Single file for transaction logging (WAL; log device)

* ? Default page size configured for all pages is 4KB

* ? Any other config block / metadata / super-block ?


## BTrees

BTrees form the bulk of the data structures used to store and navigate through
the KV-pairs. BTrees come in two flavours in this architecture:

* Dynamic BTrees - Memtable : Data (KV-pairs) can be modified (inserted / updated / deleted) in these trees

* Static BTrees - Data is immutable in these trees. This form of the BTree is used mainly to
  access KV-pairs that are unchanging.

Incorporation:
    Full memtable BTree is compacted ... Serialization. Generates finger-print list that is
    used to build the filter.

Compaction:
    Nodes in leafs are half-full. Split pages in Memtables may not be contiguous on-disk.
    takes all data, copied over to new sets of pages.

A conventional multi-level BTree design is used, consisting of a root node (page) and one or more
child pages. The root and intermediate nodes (pages) store the keys, whereas the leaf nodes (pages)
is where the actual KV-pair is stored. The fan-out of the BTree pages depends on the size of the
keys stored on each page.

## Memtables

Memtables are the in-memory storage structures which receive new inserts and updates to the KVS.

A Memtable is implemented as highly-concurrent dynamic BTree designed to avoid cache misses.
For practical purposes, a Memtable is of a finite capacity (default: 128 MB). When a Memtable is full,
it is swapped out, replaced by a new Memtable. The full Memtable is then stitched to the
underlying layer of the storage structure, the STBε-tree, as a branch node.

## Size-Tiered Bε-Tree

The STBε-tree is really a tree-of-trees. It has two main sub-components:

* The main backbone which organizes the KV-pairs is the Trunk tree, or simply trunks. The Trunk tree
   has a collection of trunk nodes, which help with navigating through the KV-pairs.

* A collection of B-trees, referred to as Branch trees, or simply branches, hanging off each trunk node

### Trunk Nodes

Each page in the Trunk tree is referred to as a Trunk node. It holds ...

### Branches, Branch Nodes (BTree pages)

A Branch is essentially what was previously a Memtable BTree, which got full, and then was
added to the trunk tree as a single unit.

## Routing Filters: Quotient Filters

## Memory Management

* Single user-level cache for all pages read from disk

* Small amount of memory allocated from the system (malloc()) to track metadata for caches and file systems objects

* Clock-based cache manager designed to improve concurrency

* ? Initializing cache mgr at bootstrap? Hash-table?
* 

## Checkpoint - not supported right now - Should be included for OSS-rollout

## Space Management: Extent, Page Allocation

* Extent Allocation, page allocation schemes
* 4KB pages
* 32 pages per extent (128 KB) - Unit of pre-allocation and bulk I/O

* ? Free / allocated space maps? Effectively a refcount table of extents allocated

* Space usage tracking - metrics: total / used / free space?

* ? How does space allocation work when device is getting full?

* ? How does page-deallocation work? Compaction?

* ? Do we reclaim space on-disk? Do we "punch holes" for an extent with all free pages?

* ? How does page allocation work for log-devices? Allocating pages at commit-time will cause
  commit latencies. Interleaving data and log-page allocation can be a problem

Space usage metrics:
    Allocation metrics basic ones
    Stats from cache can be used to compute these metrics.

## Locking and Concurrency Control

* Distributed Read-Write locks (Per-thread reader counter and a shared write-bit)
* Locking granularity is a page (? BTree page, memtable page? Trunk page?)

    * Read locks
    * "Claims" (intent) locks
    * Write locks

    Every fn in Sp has or will need a lock on it. Feature of the system.

### Threads and Parallelism

* Single-process with multi-threading support, using Unix pthreads
* Configuring threads for performance

Aspect of this which is front-end user-visible issue.
Appln gets to decide how these threads are allocated / managed.
Appln has an option to volunteer some threads to only do background work.


## Durability

* Per-thread log
* Single log-file, with multiple inserters
* ? Durability / recoverability details in paper are skimpy
 * Logged - sync is 4KB data / thread. Done asynchronously. Can have data loss.

 ? Need an API to do a manual sync that client can call.

 ? Logical logging - Log the puts


