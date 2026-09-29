/***************************************************************
 * SensorMesh Hub - Main Implementation
 *
 * A multi-threaded data pipeline simulating sensor ingestion,
 * processing, and monitoring using pthreads.
 *
 * Key Concepts:
 * - Producer / Consumer model
 * - Thread-safe ring buffer
 * - Mutex + condition variables
 * - Atomic statistics tracking
 *
 * Author: Abhishek
 ***************************************************************/

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "packet.h"   // :contentReference[oaicite:1]{index=1}
#include "crc32.h"    // :contentReference[oaicite:2]{index=2}

/***************************************************************
 * CONSTANTS
 ***************************************************************/
#define RING_CAPACITY 128

/***************************************************************
 * RING BUFFER (THREAD SAFE)
 ***************************************************************/
typedef struct {
    SensorPacket buffer[RING_CAPACITY];

    size_t head;   // next write index
    size_t tail;   // next read index
    size_t count;  // number of elements

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

} RingBuffer;

/***************************************************************
 * GLOBAL STATS (ATOMIC FOR THREAD SAFETY)
 ***************************************************************/
typedef struct {
    atomic_ullong produced;
    atomic_ullong consumed;
    atomic_ullong dropped_full;
    atomic_ullong dropped_oversize;
    atomic_ullong dropped_crc;
} Stats;

/***************************************************************
 * APPLICATION CONTEXT
 ***************************************************************/
typedef struct {
    RingBuffer rb;
    Stats stats;
    volatile int stop;

    pthread_mutex_t print_lock; // prevent mixed stdout logs

} AppContext;

static AppContext *g_ctx = NULL;

/***************************************************************
 * TIME UTILITIES
 ***************************************************************/
static uint64_t now_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}

/***************************************************************
 * RANDOM GENERATOR (simple LCG)
 ***************************************************************/
static uint32_t rand_u32(uint32_t *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

/***************************************************************
 * RING BUFFER OPERATIONS
 ***************************************************************/

/**
 * Initialize ring buffer
 */
static void rb_init(RingBuffer *rb)
{
    rb->head = rb->tail = rb->count = 0;

    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->not_empty, NULL);
    pthread_cond_init(&rb->not_full, NULL);
}

/**
 * Push packet into buffer (non-blocking)
 * Returns false if buffer full
 */
static bool rb_push(RingBuffer *rb, const SensorPacket *pkt)
{
    bool success = false;

    pthread_mutex_lock(&rb->mutex);

    if (rb->count < RING_CAPACITY) {
        rb->buffer[rb->head] = *pkt;

        rb->head = (rb->head + 1) % RING_CAPACITY;
        rb->count++;

        success = true;
        pthread_cond_signal(&rb->not_empty);
    }

    pthread_mutex_unlock(&rb->mutex);
    return success;
}

/**
 * Pop packet (blocking)
 */
static bool rb_pop(RingBuffer *rb, SensorPacket *out, volatile int *stop)
{
    pthread_mutex_lock(&rb->mutex);

    while (rb->count == 0 && !(*stop)) {
        pthread_cond_wait(&rb->not_empty, &rb->mutex);
    }

    if (rb->count == 0 && *stop) {
        pthread_mutex_unlock(&rb->mutex);
        return false;
    }

    *out = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_CAPACITY;
    rb->count--;

    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->mutex);

    return true;
}

/***************************************************************
 * VALIDATION (BUFFER OVERFLOW PROTECTION)
 ***************************************************************/
static bool validate_payload(uint16_t len, uint16_t *safe_len)
{
    if (len > MAX_PAYLOAD_BYTES) {
        *safe_len = MAX_PAYLOAD_BYTES;
        return false;
    }
    *safe_len = len;
    return true;
}

/***************************************************************
 * CRC WRAPPER
 ***************************************************************/
static uint32_t compute_crc(const SensorPacket *p, uint16_t len)
{
    return crc32_iterative((uint8_t *)p->payload, len);
}

/***************************************************************
 * PRODUCER THREAD
 ***************************************************************/
static void* producer(void *arg)
{
    AppContext *ctx = (AppContext*)arg;
    uint32_t seed = now_us();

    while (!ctx->stop) {

        SensorPacket pkt;
        memset(&pkt, 0, sizeof(pkt));

        pkt.sensor_id = SENSOR_ID_CAN;
        pkt.sequence_num = rand_u32(&seed);
        pkt.timestamp_us = now_us();

        // Generate random payload
        pkt.payload_len = rand_u32(&seed) % 300; // intentionally overflow

        for (int i = 0; i < MAX_PAYLOAD_BYTES; i++) {
            pkt.payload[i] = rand_u32(&seed) & 0xFF;
        }

        uint16_t safe_len;
        validate_payload(pkt.payload_len, &safe_len);

        pkt.checksum = compute_crc(&pkt, safe_len);

        if (!rb_push(&ctx->rb, &pkt)) {
            atomic_fetch_add(&ctx->stats.dropped_full, 1);
        } else {
            atomic_fetch_add(&ctx->stats.produced, 1);
        }

        usleep(10000);
    }

    return NULL;
}

/***************************************************************
 * CONSUMER THREAD
 ***************************************************************/
static void* consumer(void *arg)
{
    AppContext *ctx = (AppContext*)arg;

    while (1) {

        SensorPacket pkt;

        if (!rb_pop(&ctx->rb, &pkt, &ctx->stop))
            break;

        uint16_t safe_len;
        if (!validate_payload(pkt.payload_len, &safe_len)) {
            atomic_fetch_add(&ctx->stats.dropped_oversize, 1);
            continue;
        }

        uint32_t crc = compute_crc(&pkt, safe_len);
        if (crc != pkt.checksum) {
            atomic_fetch_add(&ctx->stats.dropped_crc, 1);
            continue;
        }

        atomic_fetch_add(&ctx->stats.consumed, 1);
    }

    return NULL;
}

/***************************************************************
 * MONITOR THREAD
 ***************************************************************/
static void* monitor(void *arg)
{
    AppContext *ctx = (AppContext*)arg;

    while (!ctx->stop) {

        sleep(2);

        pthread_mutex_lock(&ctx->print_lock);

        printf("\n===== STATS =====\n");
        printf("Produced : %llu\n", atomic_load(&ctx->stats.produced));
        printf("Consumed : %llu\n", atomic_load(&ctx->stats.consumed));
        printf("Dropped  : full=%llu oversize=%llu crc=%llu\n",
               atomic_load(&ctx->stats.dropped_full),
               atomic_load(&ctx->stats.dropped_oversize),
               atomic_load(&ctx->stats.dropped_crc));

        pthread_mutex_unlock(&ctx->print_lock);
    }

    return NULL;
}

/***************************************************************
 * SIGNAL HANDLER
 ***************************************************************/
static void handle_sigint(int sig)
{
    (void)sig;
    if (g_ctx) g_ctx->stop = 1;
}

/***************************************************************
 * MAIN
 ***************************************************************/
int main()
{
    AppContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    g_ctx = &ctx;

    rb_init(&ctx.rb);
    pthread_mutex_init(&ctx.print_lock, NULL);

    signal(SIGINT, handle_sigint);

    pthread_t prod, cons, mon;

    pthread_create(&prod, NULL, producer, &ctx);
    pthread_create(&cons, NULL, consumer, &ctx);
    pthread_create(&mon, NULL, monitor, &ctx);

    sleep(30); // run duration

    ctx.stop = 1;

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    pthread_join(mon, NULL);

    printf("\n=== FINAL STATS ===\n");
    printf("Produced: %llu\n", atomic_load(&ctx.stats.produced));

    return 0;
}
