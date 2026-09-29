Design



SensorMesh Hub — 



**System Architecture**



Overview



The SensorMesh Hub is a multi-threaded data pipeline designed using the Producer-Consumer model. Multiple producer threads simulate sensors, push data into a shared ring buffer, and multiple consumer threads process that data concurrently.



Architecture Components



1\. Producers

\- Simulate sensors (GPS, IMU, TEMP, CAN)

\- Generate SensorPacket structures

\- Compute CRC checksum

\- Push packets into ring buffer



2\. Ring Buffer (Core Component)



\- Fixed-size circular buffer (capacity = 128)

\- Shared across all threads

\- Stores SensorPacket objects



Key Fields:

\- `head` → next write position

\- `tail` → next read position

\- `count` → number of elements



Why Ring Buffer?

\- Constant memory usage (important for embedded systems)

\- No dynamic allocation per packet

\- Cache-friendly contiguous memory



3\. Consumers



Logger

\- Writes validated packets to `/tmp/sensormesh.log`



Analyzer

\- Processes packets for statistical insights



Forwarder

\- Prints formatted packet data to stdout



4\. Monitor Thread



\- Runs every 2 seconds

\- Displays:

&#x20; - Buffer occupancy

&#x20; - Produced/consumed packets

&#x20; - Dropped packets



Design Decisions



Why Multi-threading?

\- Simulates real embedded system concurrency

\- Separates data generation and processing



Why Mutex + Condition Variables?

\- Ensures safe access to shared buffer

\- Avoids busy waiting



Why Atomic Counters?

\- Lightweight thread-safe statistics

\- No locking overhead for counters



Key Properties



\- Thread-safe

\- Deterministic memory usage

\- Graceful shutdown supported

\- Scalable (thread counts configurable)



Conclusion



The architecture is designed to mimic real embedded telemetry systems where:

\- Data is continuously produced

\- Buffered safely

\- Processed asynchronously



This ensures robustness, scalability, and performance.

















**Concurrency \& Race Condition Handling**



Problem



Multiple producer and consumer threads access shared ring buffer variables:

\- head

\- tail

\- count



Without synchronization, this leads to race conditions.



Example Race Condition



Scenario without mutex:



1\. Two producers check buffer is not full

2\. Both read same `head` value

3\. Both write to same slot

4\. One packet overwrites another



Result:

\- Silent data corruption

\- No crash → hardest bug to detect



Solution



Mutex (pthread\_mutex\_t)



All buffer operations are protected using a mutex.



pthread\_mutex\_lock(\&rb->mutex);

/\* critical section \*/

pthread\_mutex\_unlock(\&rb->mutex);











**Memory\_layout**



Memory Layout Analysis



Overview



Understanding memory layout is critical in embedded systems to avoid:

\- Stack overflow

\- Memory corruption

\- Undefined behavior



\--Mapping of Program Components--



1\. RingBuffer



Location: Heap (allocated inside main context)



Reason:

\- Large structure (128 packets)

\- Avoid stack overflow

\- Shared across threads



2\. SensorPacket.payload\[]



Location:

\- Inside struct → depends on allocation context

\- When local → stack

\- When in ring buffer → heap



3\. Statistics (atomic variables)



Location:

\- Inside context struct → heap



Reason:

\- Shared across threads

\- Must persist throughout program



4\. Log File Path



Example:



Location: ROData



Reason:

\- String literal

\- Read-only

\- Attempting to modify causes crash



5\. Thread Stacks



Location: Stack segment



Used for:

\- Function calls

\- Local variables

\- Recursive calls





6\. CRC Function Call Stack



Location: Stack



Risk:

\- Recursive version → deep call stack

\- Can cause stack overflow



Solution:

\- Iterative CRC used

\- OR stack size increased



Key Observations



\- Heap used for large shared structures

\- Stack used for temporary data

\- ROData protects constants



Conclusion



Memory placement is intentionally designed to:

\- Prevent overflow

\- Ensure thread safety

\- Optimize performance



This reflects Embedded System constraints.











**Hazard Handling**



This system intentionally includes 4 critical hazards. 

Each is addressed explicitly.



Hazard 1: Race Condition



Problem

Multiple threads access shared ring buffer.



Solution

\- Mutex protects critical section

\- Condition variables synchronize threads



Why it works

\- Only one thread modifies buffer at a time

\- No inconsistent state possible



Hazard 2: Buffer Overflow



Problem

`payload\_len` can exceed MAX\_PAYLOAD\_BYTES



Risk

\- Memory corruption

\- Undefined behavior



Solution

if (len > MAX\_PAYLOAD\_BYTES)

&#x20;   clamp or drop





