# Pumpkin --- V1 Project Plan

**Status:** V1 Planning\
**Primary language:** C++20/23\
**Target platforms:** macOS + Linux\
**Project type:** High-performance cache-heavy in-memory storage system
/ systems research platform

------------------------------------------------------------------------

## 1. Project Vision

Build **Pumpkin**, a production-oriented, high-performance in-memory
caching and key-value storage system from first principles.

Pumpkin is **not a clone of an existing cache system**. Existing systems
such as Redis and Memcached can be used as external performance and
design references, but Pumpkin owns its protocol, architecture,
execution model, and research direction. The longer-term goal is to
build a scalable cache engine that can serve as an experimental platform
for improvements involving:

-   Multicore scalability
-   CPU cache locality
-   NUMA-aware memory placement
-   Hot-key detection and replication
-   Software cache coherence
-   Adaptive coherence policies
-   Dynamic shard migration
-   Memory efficiency
-   Tail-latency optimization
-   Distributed scaling

The development strategy is:

> **First build a correct, scalable, measurable baseline. Then introduce
> advanced architecture ideas one at a time and benchmark them against
> the baseline.**

------------------------------------------------------------------------

## 1.1 Pumpkin Identity

Project name: **Pumpkin**

Initial component naming:

``` text
pumpkin-server     main server process
pumpkin-cli        interactive command-line client
pumpkin-bench      workload and performance benchmark tool

Pumpkin Protocol   native wire protocol
Pumpkin Node       one running server instance
Pumpkin Worker     execution worker
Pumpkin Shard      logical data partition
Pumpkin Cache      in-memory storage engine
Pumpkin Coherence  future replication/coherence subsystem
Pumpkin Cluster    future multi-node system
```

Pumpkin is cache-heavy by design. Persistent storage exists to support
recovery and durability where desired, but the primary data path is
optimized around memory-resident objects and low-latency access.

### Protocol philosophy

Pumpkin should define its own protocol rather than making another
system's wire protocol part of the core architecture.

The first protocol should prioritize:

-   Simple incremental parsing
-   Binary safety
-   Pipelining
-   Request IDs for matching responses
-   Explicit lengths rather than delimiter scanning for payloads
-   Low allocation/copy overhead
-   Easy implementation in C++ and future client SDKs
-   Versioning/extensibility
-   Batch operations

The exact wire format is a separate design task. V1 may begin with a
simple framed format and evolve before compatibility is frozen.

A future compatibility adapter may expose another ecosystem's protocol
without coupling the Pumpkin engine to that protocol.

------------------------------------------------------------------------

## 2. V1 Goals

V1 should establish a serious single-machine baseline suitable for
future research.

### Core goals

1.  Pumpkin's own request/response wire protocol: **Pumpkin Protocol
    (PP)**.
2.  High-performance Linux networking.
3.  Multicore execution.
4.  Logical sharding with single-owner shard execution.
5.  In-memory key-value storage.
6.  TTL/expiration support.
7.  Memory-management infrastructure.
8.  Persistence architecture with WAL/AOF and later snapshots.
9.  Strong benchmarking and profiling infrastructure.
10. Clear module boundaries so experimental implementations can replace
    baseline components.
11. First-class macOS and Linux support through an explicit OS/platform
    abstraction layer.

### Non-goals for initial V1

The following should **not** block the first usable baseline:

-   Raft
-   Multi-node consensus
-   Compatibility layers for third-party cache protocols
-   Advanced cache-coherence protocols
-   Hot-key replication
-   Automatic NUMA optimization
-   Online distributed resharding
-   SmartNIC/DPU/FPGA acceleration
-   CXL/tiered-memory support

These are later experiments built on top of V1.

------------------------------------------------------------------------

## 3. High-Level Architecture

``` text
                           CLIENTS
                              |
                              v
                  +-----------------------+
                  | Network / Connection  |
                  | epoll / later io_uring|
                  +-----------+-----------+
                              |
                              v
                  +-----------------------+
                  | Pumpkin Protocol Parser  |
                  +-----------+-----------+
                              |
                              v
                  +-----------------------+
                  | Command Dispatcher    |
                  +-----------+-----------+
                              |
                           hash(key)
                              |
                +-------------+-------------+
                |             |             |
                v             v             v
             Worker 0      Worker 1      Worker N
                |             |             |
                v             v             v
             Shards        Shards        Shards
                |             |             |
                v             v             v
            Hash Tables    Hash Tables   Hash Tables

                +-------------+-------------+
                              |
                    +---------+---------+
                    |                   |
                    v                   v
                 WAL/AOF            Metrics
                    |
                    v
                Snapshots
```

------------------------------------------------------------------------

## 4. Fundamental Execution Model

A central architectural principle is:

> **One logical shard has exactly one execution owner at a time.**

Avoid a design where every worker accesses one globally locked hash
table.

Instead:

``` text
key
 |
 v
hash(key)
 |
 v
logical shard
 |
 v
worker owning shard
 |
 v
local storage engine
```

Example:

``` cpp
uint64_t h = hash(key);
ShardID shard = h % NUM_LOGICAL_SHARDS;
WorkerID worker = shard_map[shard];
```

The number of **logical shards should be much larger than the number of
CPU workers**.

Example:

``` text
16 worker threads
1024 logical shards
```

This allows future:

-   shard migration
-   load balancing
-   NUMA placement
-   hot-shard splitting
-   dynamic worker assignment

without changing the fundamental key-routing abstraction.

------------------------------------------------------------------------

## 5. OS / Platform Abstraction

macOS and Linux are both first-class Pumpkin development and test
targets.

A core architectural rule is:

> **Pumpkin Core must not directly depend on `epoll`, `kqueue`, or other
> OS-specific facilities.**

Platform-specific functionality is implemented behind explicit
interfaces.

``` text
                         Pumpkin Core
                              |
                    Platform Interfaces
                              |
                +-------------+-------------+
                |                           |
              macOS                       Linux
                |                           |
              kqueue                      epoll
              mmap                        mmap
          pthread APIs                pthread APIs
         macOS timers                 Linux timers
                |                           |
                |                     future Linux:
                |                     io_uring
                |                     NUMA APIs
                |                     CPU affinity
                |                     huge pages
                |                     perf counters
                +-------------+-------------+
                              |
                       Same Pumpkin Core
```

The abstraction should eventually cover:

-   Event I/O
-   Sockets and connection primitives where abstraction is useful
-   Threads and CPU topology
-   CPU affinity
-   Timers and monotonic clocks
-   Memory mapping / virtual memory helpers
-   File I/O needed by persistence
-   NUMA facilities where available
-   Huge-page support where available
-   Performance-counter/profiling hooks

Platform APIs do **not** need to be artificially normalized when their
semantics differ. The common interface should expose capabilities
cleanly, and Pumpkin may query whether an optional feature exists.

Example:

``` cpp
class EventLoop {
public:
    virtual ~EventLoop() = default;

    virtual Result add(int fd, EventMask events) = 0;
    virtual Result modify(int fd, EventMask events) = 0;
    virtual Result remove(int fd) = 0;
    virtual int wait(Event* events, int max_events, Timeout timeout) = 0;
};
```

Implementations:

``` text
EventLoop
   |
   +-- KqueueEventLoop    macOS
   |
   +-- EpollEventLoop     Linux
   |
   +-- IoUringEventLoop   future Linux experiment
```

This lets Pumpkin compare platform implementations without coupling the
storage or command engine to one operating system.

### 5.1 Initial Networking Architecture

Use non-blocking TCP sockets with the native scalable event facility:

``` text
macOS                            Linux

TCP socket                      TCP socket
    |                               |
non-blocking                    non-blocking
    |                               |
  kqueue                          epoll
    |                               |
    +---------------+---------------+
                    |
            EventLoop interface
                    |
                    v
          Pumpkin Protocol parser
                    |
                    v
                 command
```

Initial priorities:

-   Correct connection lifecycle
-   Partial reads
-   Partial writes
-   Request pipelining
-   Backpressure
-   Connection timeouts
-   Efficient buffers
-   Avoid unnecessary allocations/copies

### 5.2 Future networking experiments

Once the baseline is measurable:

-   Linux `io_uring` behind the EventLoop abstraction
-   request batching
-   zero-copy techniques where applicable
-   per-core network queues
-   kernel-bypass networking as an advanced experiment

Networking optimizations must be benchmarked rather than assumed to be
improvements.

------------------------------------------------------------------------

## 6. Protocol Layer

Start with a practical Pumpkin Protocol command set.

Initial commands:

``` text
PING
GET
SET
DEL
EXISTS
INCR
DECR
MGET
MSET
EXPIRE
TTL
```

Potential later structures:

``` text
LIST
HASH
SET
SORTED SET
```

The protocol layer should produce a command representation independent
of networking:

``` cpp
struct Command {
    CommandType type;
    std::vector<Argument> args;
};
```

This allows the storage engine to be tested without TCP.

------------------------------------------------------------------------

## 7. Storage Engine

### Baseline implementation

A standard hash table may be used initially to bring up the complete
system quickly.

The baseline should then transition toward a custom storage
implementation suitable for experimentation.

Desired interface:

``` cpp
class StorageEngine {
public:
    Result get(KeyView key);
    Result set(KeyView key, ValueView value);
    bool erase(KeyView key);
    bool exists(KeyView key);
};
```

### Research-ready storage directions

Later compare:

-   chaining
-   open addressing
-   Robin Hood hashing
-   cache-line-conscious layouts
-   metadata/control-byte layouts
-   SIMD-assisted lookup
-   software prefetching
-   pointer-heavy vs contiguous layouts

The objective is not simply implementing a hash table. It is
understanding how layout affects:

-   L1/L2/L3 misses
-   memory bandwidth
-   branch prediction
-   pointer chasing
-   cache-line utilization
-   throughput
-   tail latency

------------------------------------------------------------------------

## 8. Memory Management

Memory allocation must eventually become a first-class subsystem.

Avoid permanent dependence on arbitrary `new`/`delete` calls throughout
the engine.

Potential design:

``` text
Memory Manager
      |
      +---- small-object slabs
      |
      +---- medium-object slabs
      |
      +---- large-object allocator
      |
      +---- arenas / shard-local allocation
```

Track at minimum:

-   bytes allocated
-   bytes logically used
-   fragmentation
-   allocation count
-   deallocation count
-   bytes/key
-   allocator latency

Future experiments can compare system allocators, jemalloc-style
behavior, slab allocation, arenas, and shard-local allocators.

------------------------------------------------------------------------

## 9. TTL and Expiration

Support both lazy and active expiration.

### Lazy expiration

``` text
GET key
   |
   v
check TTL
   |
   +--- expired ---> delete ---> NIL
   |
   +--- valid -----> return value
```

### Active expiration

Workers periodically sample/process expiration metadata and remove
expired objects.

The expiration design should avoid requiring a single global expiration
lock or timer structure.

------------------------------------------------------------------------

## 10. Persistence

Persistence should be introduced after the in-memory engine is stable.

### WAL / AOF

Mutating operations are represented in an append-only log.

Conceptually:

``` text
104892 SET foo bar
104893 DEL hello
104894 INCR counter
```

On restart:

``` text
WAL/AOF
   |
   v
replay
   |
   v
reconstruct in-memory state
```

Important topics:

-   ordering
-   buffering
-   fsync policies
-   group commit
-   crash consistency
-   torn/incomplete writes
-   recovery validation

### Snapshots

Later add point-in-time snapshots so startup does not require replaying
an indefinitely large log.

``` text
snapshot + WAL tail -> recovered database
```

------------------------------------------------------------------------

## 11. Benchmarking Is a Core Feature

The database should not be called "fast" without evidence.

Benchmarking infrastructure is part of V1, not an afterthought.

### Workloads

Measure:

-   GET-only
-   SET-only
-   80/20 GET/SET
-   50/50 GET/SET
-   MGET
-   MSET
-   small values
-   medium values
-   large values
-   uniform key distribution
-   Zipfian distribution
-   hot-key workloads

### CPU scaling

Test:

``` text
1 core
2 cores
4 cores
8 cores
16 cores
32+ cores where hardware permits
```

### Metrics

Measure at minimum:

``` text
operations/sec

p50 latency
p95 latency
p99 latency
p99.9 latency

CPU utilization
memory usage
bytes/key
allocator fragmentation

cache misses
branch misses
context switches

network RX/TX
queue depth
worker utilization
per-shard utilization
```

Use Linux profiling/performance tools where appropriate, including
`perf` and flame graphs.

Every future optimization should be compared against a reproducible
baseline.

------------------------------------------------------------------------

## 12. Advanced Research Track A --- Cache-Aware Storage

Once V1 is stable, investigate layouts designed around the CPU memory
hierarchy.

``` text
CPU
 |
L1
 |
L2
 |
L3
 |
local DRAM
 |
remote NUMA DRAM
```

Potential experiments:

-   contiguous key/value metadata
-   reducing pointer chasing
-   cache-line-aware buckets
-   prefetching
-   SIMD lookup
-   separating hot/cold metadata
-   false-sharing elimination

Compare each experiment against the V1 baseline.

------------------------------------------------------------------------

## 13. Advanced Research Track B --- NUMA Awareness

On multi-socket systems:

``` text
NUMA Node 0                     NUMA Node 1

Core 0 ... Core 15             Core 16 ... Core 31
       |                               |
local shards                       local shards
       |                               |
Memory Node 0                     Memory Node 1
```

Potential improvements:

-   pin workers to CPUs
-   allocate shard memory locally
-   NUMA-aware shard assignment
-   detect excessive remote accesses
-   migrate shards
-   replicate read-heavy data across NUMA nodes

Measure local vs remote memory behavior rather than assuming placement
is beneficial.

------------------------------------------------------------------------

## 14. Advanced Research Track C --- Cache-Coherence-Aware Shared State

Shared mutable metadata can create cache-line bouncing.

Bad pattern:

``` cpp
std::atomic<uint64_t> global_requests;
```

accessed continuously by every core.

Alternative:

``` text
Core 0       Core 1       Core 2       Core 3
counter0     counter1     counter2     counter3
    \            |           |           /
              aggregation
                   |
                   v
                 total
```

Research areas:

-   per-core statistics
-   hierarchical aggregation
-   cache-line padding
-   false-sharing avoidance
-   reduced atomic contention
-   coherence traffic measurement

------------------------------------------------------------------------

## 15. Advanced Research Track D --- Hot-Key Replication

A highly popular key can overload the worker owning its shard.

Baseline:

``` text
             hot key
                |
             Worker 2
          / / / | \ \ \
       many read requests
```

Experimental approach:

``` text
Core 0       Core 1       Core 2       Core 3
 copy         copy         copy         copy
```

Reads become local.

This creates a new problem:

> How are replicas kept consistent after writes?

That leads directly into software coherence.

------------------------------------------------------------------------

## 16. Advanced Research Track E --- Software Cache Coherence

Explore cache-coherence concepts at the database/object layer.

Possible simplified states:

``` text
OWNER
SHARED
INVALID
```

Potential strategies:

### Write-invalidate

A writer updates the owner and invalidates replicas.

### Write-update

A writer propagates the new value to replicas.

### Directory-based coherence

Track exactly which workers contain replicas.

### Broadcast

Notify all workers.

### Hierarchical dissemination

Example:

``` text
                 Owner
                /     \
              C1       C2
             / \       / \
           C3  C4    C5  C6
```

Potential research question:

> Can hierarchical or topology-aware software coherence reduce
> coordination overhead for replicated hot objects on high-core-count or
> NUMA systems?

Do not assume a tree is superior. Benchmark broadcast, directory, flat
invalidation, and hierarchical approaches across different core counts
and workloads.

------------------------------------------------------------------------

## 17. Advanced Research Track F --- Adaptive Coherence

Different keys have different access patterns.

Example:

``` text
Key A:
GET GET GET GET GET GET SET
-> strongly read-heavy

Key B:
GET SET GET SET SET GET SET
-> write-heavy
```

An adaptive policy could choose:

``` text
read-heavy + hot
       |
       v
replicated mode

write-heavy
       |
       v
single-owner mode
```

The system may transition dynamically:

``` text
Single Owner
     |
     | key becomes hot/read-heavy
     v
Replicated
     |
     | write frequency increases
     v
Single Owner
```

Possible inputs to the policy engine:

-   request frequency
-   read/write ratio
-   queue depth
-   worker utilization
-   cache misses
-   NUMA remote accesses
-   replica invalidation rate

This creates an adaptive storage system rather than a static one.

------------------------------------------------------------------------

## 18. Advanced Research Track G --- Dynamic Shard Migration

Logical sharding allows load imbalance to be corrected.

Example:

``` text
BEFORE

Core 0   95%
Core 1   22%
Core 2   31%
Core 3   18%
```

The runtime could detect imbalance and move logical shards:

``` text
AFTER

Core 0   45%
Core 1   42%
Core 2   39%
Core 3   40%
```

Important research/engineering problems:

-   migration while requests continue
-   ownership handoff
-   ordering
-   queued commands
-   avoiding lost updates
-   minimizing migration latency
-   deciding when migration cost exceeds its benefit

------------------------------------------------------------------------

## 19. Future Distributed Architecture

Multi-node distribution should come after the single-machine baseline
and initial architecture experiments are stable.

Future architecture:

``` text
                         Clients
                            |
                +-----------+-----------+
                |           |           |
                v           v           v
              Node A      Node B      Node C
                |           |           |
             shards      shards      shards
```

Potential components:

-   partition map
-   replication
-   failure detection
-   membership
-   leader/primary election
-   online rebalancing
-   node addition/removal
-   partial resynchronization
-   consensus/control plane

A key design principle to investigate:

> Keep the high-throughput data path separate from expensive
> cluster-control mechanisms wherever correctness permits.

------------------------------------------------------------------------

## 20. Proposed Repository Structure

``` text
project/
|
+-- src/
|   +-- server/
|   |   +-- tcp_server.cpp
|   |   +-- connection.cpp
|   |   +-- event_loop.cpp
|   |
|   +-- protocol/
|   |   +-- resp_parser.cpp
|   |   +-- command.cpp
|   |
|   +-- engine/
|   |   +-- database.cpp
|   |   +-- shard.cpp
|   |   +-- hash_table.cpp
|   |   +-- object.cpp
|   |   +-- expiration.cpp
|   |   +-- eviction.cpp
|   |
|   +-- memory/
|   |   +-- allocator.cpp
|   |   +-- slab.cpp
|   |   +-- arena.cpp
|   |
|   +-- persistence/
|   |   +-- wal.cpp
|   |   +-- snapshot.cpp
|   |   +-- recovery.cpp
|   |
|   +-- replication/
|   |
|   +-- cluster/
|   |
|   +-- metrics/
|   |
|   +-- platform/
|   |   +-- event_loop.hpp
|   |   +-- clock.hpp
|   |   +-- memory.hpp
|   |   +-- topology.hpp
|   |   +-- macos/
|   |   |   +-- kqueue_event_loop.cpp
|   |   |   +-- platform_macos.cpp
|   |   +-- linux/
|   |       +-- epoll_event_loop.cpp
|   |       +-- platform_linux.cpp
|   |
|   +-- util/
|
+-- client/
|   +-- cpp/
|   +-- python/
|
+-- tests/
|   +-- unit/
|   +-- integration/
|   +-- stress/
|   +-- recovery/
|   +-- chaos/
|
+-- benchmark/
|
+-- tools/
|
+-- docs/
|
+-- CMakeLists.txt
+-- README.md
```

------------------------------------------------------------------------

## 21. Development Milestones

### M0 --- Project Skeleton

-   CMake
-   formatting/linting
-   unit-test framework
-   CI
-   benchmark executable
-   basic documentation
-   macOS + Linux CI/build coverage
-   platform capability detection

### M1 --- Functional Single-Threaded Server

-   platform-neutral EventLoop interface
-   macOS `kqueue` backend
-   Linux `epoll` backend
-   non-blocking TCP
-   Pumpkin Protocol parser
-   GET
-   SET
-   DEL
-   basic storage
-   basic tests

**Goal:** end-to-end correctness.

### M2 --- Scalable Shard Architecture

-   logical shard abstraction
-   worker ownership
-   request routing
-   per-worker queues
-   multiple CPU cores
-   no global storage lock in normal key operations

**Goal:** architecture suitable for scaling experiments.

### M3 --- Production-Oriented Storage Baseline

-   improved/custom hash table
-   object representation
-   memory accounting
-   TTL
-   active/lazy expiration
-   eviction groundwork

### M4 --- Benchmarking & Profiling Baseline

-   reproducible workloads
-   latency histograms
-   throughput
-   CPU scaling
-   Zipf/hot-key tests
-   `perf` measurements
-   flame graphs
-   cache/branch statistics

**This milestone establishes the reference baseline for all later
experiments.**

### M4.5 --- Cross-Platform Validation

Run the same functional and benchmark suites on both macOS and Linux.

Validate:

-   protocol behavior is identical
-   storage semantics are identical
-   TTL behavior is identical
-   concurrency tests pass on both systems
-   event-loop implementations satisfy the same contract
-   performance differences are measured rather than hidden

macOS is a first-class development platform. Linux is also a first-class
target and becomes the primary environment for experiments requiring
Linux-specific facilities such as NUMA APIs, `io_uring`, advanced CPU
affinity, huge pages, and hardware performance counters.

### M5 --- Persistence

-   WAL/AOF
-   restart recovery
-   fsync policies
-   crash tests
-   snapshots

### M6 --- Architecture Experiment #1

Recommended first experiment:

**Cache-aware hash table/storage layout**

Compare against M4/M5 baseline.

### M7 --- Architecture Experiment #2

**NUMA-aware worker and memory placement**

### M8 --- Architecture Experiment #3

**Hot-key detection and replication**

### M9 --- Architecture Experiment #4

**Software coherence protocols**

Compare:

-   invalidate
-   update
-   directory
-   broadcast
-   hierarchical dissemination

### M10 --- Adaptive Runtime

-   workload monitoring
-   replication decisions
-   coherence-policy selection
-   shard migration/rebalancing

### M11+ --- Distributed System

-   multiple machines
-   replication
-   failure recovery
-   control plane
-   dynamic partition movement
-   distributed benchmarking
-   chaos testing

------------------------------------------------------------------------

## 22. Engineering Principles

### Keep OS-specific code at the platform boundary

Pumpkin's command, shard, storage, protocol, and policy logic should
remain portable. OS-specific optimization paths belong under
`src/platform/` or another explicit backend boundary.

Linux-only features are allowed and encouraged when they provide
measurable benefits, but the portable core must not silently become
Linux-specific.

### Test both operating systems

Functional correctness should be continuously exercised on both macOS
and Linux. Performance numbers must always identify the OS, hardware,
compiler, and backend used.

### Correctness before optimization

Every optimization must preserve clearly defined semantics.

### Measure before changing architecture

Do not optimize based solely on intuition.

### Benchmark against the baseline

Every experimental feature should answer:

``` text
What changed?
Why should it help?
What workload benefits?
What workload suffers?
What is the throughput change?
What is the p99/p99.9 change?
What is the memory cost?
What hardware behavior changed?
```

### Keep experiments replaceable

Networking, storage layout, allocator, scheduling, replication, and
coherence policies should have boundaries that allow A/B
implementations.

### Design for observability

Performance behavior should be explainable through metrics and profiling
rather than only end-to-end throughput numbers.

------------------------------------------------------------------------

## 23. Core Research Theme

The long-term system should investigate whether an in-memory database
can make better decisions by understanding both the **workload** and the
**underlying hardware topology**.

Conceptually:

``` text
                 Workload
                    |
        +-----------+-----------+
        |           |           |
     hot keys   R/W ratio   hot shards
        |           |           |
        +-----------+-----------+
                    |
                    v
              Policy Engine
                    |
        +-----------+-----------+
        |           |           |
    replicate     migrate     change
      object       shard      policy
        |           |           |
        +-----------+-----------+
                    |
                    v
               Hardware
                    |
        +-----------+-----------+
        |           |           |
     CPU cache     NUMA      memory
     hierarchy    topology    latency
```

The eventual research question is broader than external cache systems:

> **Can a high-performance in-memory database dynamically adapt data
> placement, replication, and coherence policy to workload behavior and
> modern multicore/NUMA hardware topology?**

------------------------------------------------------------------------

## 23.1 Project Positioning

A concise description for README/GitHub:

> **Pumpkin is a high-performance, cache-heavy in-memory storage system
> designed as a platform for exploring multicore scalability,
> hardware-aware data placement, NUMA locality, hot-key replication, and
> software cache coherence.**

V1 focuses on building a scalable, correct, measurable engine. Advanced
features are introduced only after the baseline is established so their
effects can be measured scientifically.

------------------------------------------------------------------------

## 24. Immediate Next Step

Do **not** begin with coherence, Raft, NUMA migration, or a custom
allocator.

Start by implementing:

``` text
pumpkin-server
      |
Platform EventLoop
   /        \
kqueue     epoll
macOS      Linux
   \        /
 non-blocking TCP
      |
Pumpkin Protocol parser
      |
command dispatcher
      |
logical shard routing
      |
worker-owned storage
      |
GET / SET / DEL
```

Once this path is correct and benchmarkable, establish the M4
performance baseline before introducing experimental architecture
changes.

------------------------------------------------------------------------

## 25. V1 Success Definition

V1 is successful when we have:

-   A reliable external cache systems-compatible subset.
-   A multicore logical-shard architecture.
-   The same Pumpkin core builds and runs on macOS and Linux.
-   `kqueue` and `epoll` are isolated behind a platform EventLoop
    interface.
-   No global lock on the normal GET/SET path.
-   Correct TTL behavior.
-   A storage/memory architecture that can be replaced experimentally.
-   Reproducible throughput and latency benchmarks.
-   Hardware-level profiling data.
-   A persistence/recovery path.
-   Clear experimental hooks for cache-aware, NUMA-aware, hot-key, and
    coherence research.

At that point the project stops being merely a database implementation
and becomes a **systems experimentation platform**.
