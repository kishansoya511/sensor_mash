# SensorMesh Hub

A multithreaded sensor data pipeline in C, built on POSIX threads, demonstrating
the classic bounded-buffer producer-consumer problem with a thread-safe ring
buffer, condition-variable synchronization, atomic statistics tracking, and
CRC-32 data integrity validation.

## Overview

SensorMesh Hub simulates a sensor ingestion pipeline using three cooperating
threads:

- **Producer** — generates synthetic sensor packets and pushes them into a
  shared ring buffer.
- **Consumer** — pops packets from the ring buffer, validates payload size
  and CRC-32 checksum, and tracks results.
- **Monitor** — periodically reports live pipeline statistics (produced,
  consumed, and dropped packet counts).

The project's core purpose is to correctly solve the bounded-buffer
producer-consumer problem: coordinating fast/slow producers and consumers
over a fixed-capacity shared buffer without race conditions, without
busy-waiting, and with clean shutdown behavior.

## Key Concepts Demonstrated

- POSIX threads (`pthread_create`, `pthread_join`)
- Mutex-protected critical sections
- Condition variables (`not_empty`, `not_full`) with correct `while`-loop
  wait semantics to avoid spurious-wakeup bugs
- C11 atomic operations (`stdatomic.h`) for lock-free statistics counters
- CRC-32 checksum validation for data integrity
- Graceful shutdown via `SIGINT` handling and a coordinated stop flag
- Fixed-capacity circular ring buffer implementation

## Project Structure

```
.
├── task3.c      # Main implementation: ring buffer, threads, signal handling
├── packet.h     # SensorPacket structure and sensor ID definitions
├── crc32.h      # CRC-32 checksum implementation (recursive and iterative)
├── Makefile     # Build configuration
└── README.md
```

## Build

Requires `gcc` and a POSIX-compliant environment (Linux, WSL, or similar)
with pthread support.

```bash
make
```

## Run

```bash
./sensormesh
```

The pipeline runs for a fixed duration, printing live statistics every 2
seconds. Press `Ctrl+C` at any time to trigger a graceful shutdown — all
threads will finish their current operation and exit cleanly, and final
statistics will be printed before the program terminates.

## Design Notes

**Why a ring buffer instead of a simple queue?**
A fixed-capacity circular buffer avoids dynamic allocation on the hot path
and gives predictable memory usage, an important property for real-time and
embedded-adjacent workloads.

**Why condition variables instead of polling?**
The consumer blocks on `not_empty` instead of spinning, so it consumes no
CPU while the buffer is empty. The `pthread_cond_wait` call is wrapped in a
`while` loop (not `if`) specifically to guard against spurious wakeups, a
detail that matters for correctness under POSIX semantics.

**Why atomics for statistics?**
The monitor thread reads counters that the producer and consumer update
concurrently. Using `stdatomic.h` operations avoids taking the ring buffer's
mutex just to update a counter, keeping the critical section focused on the
buffer itself.

## Known Limitations

- Single-process, multithreaded design — this project does not use
  inter-process communication (no `fork`, pipes, shared memory between
  processes, or sockets).
- CRC validation logic is in place on the consumer path, but the current
  producer does not intentionally inject corrupted payloads, so the CRC
  failure path is not yet exercised under test.
- Payload contents are randomly generated for simulation purposes and do not
  represent real sensor data.

## Possible Extensions

- Inject simulated data corruption to validate the CRC rejection path
- Add configurable buffer capacity and run duration via command-line
  arguments
- Extend statistics with per-sensor-type breakdowns

## Author

Abhishek Chawda
