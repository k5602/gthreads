#ifndef GTHREADS_TRACE_FORMAT_H
#define GTHREADS_TRACE_FORMAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Trace File Format v2
 *
 * Binary format for deterministic replay of green thread execution.
 * All multi-byte values are little-endian.
 *
 * File Structure:
 *   [Header: 32 bytes]
 *   [Event Records: 32 bytes each]
 *
 * Design goals:
 *   - Compact representation
 *   - Fast append-only writing
 *   - No parsing required during record
 *   - Sequential read during replay
 */

/* Magic identifier: "GTR2" (Green Thread Replay v2) */
#define GTH_TRACE_MAGIC 0x32525447U

/* Current format version */
#define GTH_TRACE_VERSION 2U

/* Size of trace record in bytes - power of 2 for alignment */
#define GTH_TRACE_RECORD_SIZE 32U

/* Maximum number of events to buffer before forced flush */
#define GTH_TRACE_BUFFER_COUNT 256U

    /*
     * Event Types
     *
     * All non-deterministic operations that affect scheduling
     * or inter-thread ordering must be recorded.
     */
    typedef enum
    {
        GTH_EVT_NONE = 0, /* Padding/invalid */

        /* Thread lifecycle */
        GTH_EVT_THREAD_CREATE = 1, /* tid created */
        GTH_EVT_THREAD_EXIT = 2,   /* tid exited with retval */
        GTH_EVT_THREAD_CANCEL = 3, /* tid was canceled */

        /* Scheduling */
        GTH_EVT_CONTEXT_SWITCH = 10, /* from_tid -> to_tid */
        GTH_EVT_THREAD_YIELD = 11,   /* tid yielded */

        /* Blocking/unblocking */
        GTH_EVT_THREAD_BLOCK = 20,   /* tid blocked (reason in data) */
        GTH_EVT_THREAD_UNBLOCK = 21, /* tid unblocked */

        /* Mutex operations */
        GTH_EVT_MUTEX_LOCK = 30,   /* tid locked mutex */
        GTH_EVT_MUTEX_UNLOCK = 31, /* tid unlocked mutex */
        GTH_EVT_MUTEX_WAIT = 32,   /* tid blocked waiting for mutex */
        GTH_EVT_MUTEX_WAKE = 33,   /* tid woken from mutex wait */

        /* Semaphore operations */
        GTH_EVT_SEM_WAIT = 40, /* tid waited, count before */
        GTH_EVT_SEM_POST = 41, /* tid posted, count after */
        GTH_EVT_SEM_WAKE = 42, /* tid woken from sem wait */

        /* Condition variable operations */
        GTH_EVT_COND_WAIT = 50,      /* tid started waiting */
        GTH_EVT_COND_SIGNAL = 51,    /* tid signaled, target woken */
        GTH_EVT_COND_BROADCAST = 52, /* tid broadcast, count woken */

        /* Runtime events */
        GTH_EVT_RUNTIME_INIT = 90,    /* runtime initialized */
        GTH_EVT_RUNTIME_SHUTDOWN = 91 /* runtime shutting down */
    } gth_event_type_t;

    /*
     * Thread block reasons (for GTH_EVT_THREAD_BLOCK data field)
     */
    typedef enum
    {
        GTH_BLOCK_REASON_NONE = 0,
        GTH_BLOCK_REASON_MUTEX = 1,
        GTH_BLOCK_REASON_SEM = 2,
        GTH_BLOCK_REASON_COND = 3,
        GTH_BLOCK_REASON_JOIN = 4,
        GTH_BLOCK_REASON_IO = 5
    } gth_block_reason_t;

    /*
     * Trace file header
     * Always 32 bytes - aligned to cache line
     */
    typedef struct __attribute__((packed))
    {
        uint32_t magic;          /* GTH_TRACE_MAGIC */
        uint32_t version;        /* GTH_TRACE_VERSION */
        uint64_t creation_time;  /* ns since epoch (CLOCK_REALTIME) */
        uint64_t timestamp_freq; /* timestamp ticks per second */
        uint32_t reserved[2];    /* Reserved - zero for forward compatibility */
    } gth_trace_header_t;

    _Static_assert(sizeof(gth_trace_header_t) == 32, "header must be 32 bytes");

    /*
     * Event-specific data (8 bytes)
     *
     * Different event types pack different data into the raw field.
     * Use accessor macros to pack/unpack specific event data.
     */
    typedef union
    {
        uint64_t raw; /* Generic 64-bit payload - used for all events */
    } gth_event_data_t;

    /*
     * Trace event record
     * Fixed 32 bytes for simple sequential I/O
     */
    typedef struct __attribute__((packed))
    {
        uint64_t timestamp;    /* Monotonic timestamp (rdtsc or clock) */
        uint8_t type;          /* gth_event_type_t */
        uint8_t reserved;      /* Padding */
        uint32_t tid;          /* Thread ID - widened to 32-bit in v2 */
        uint32_t slot_index;   /* Thread slot for internal use */
        gth_event_data_t data; /* Event-specific data (8 bytes) */
        uint16_t pad;          /* Padding to make record exactly 32 bytes */
        uint32_t checksum;     /* FNV-1a checksum of record */
    } gth_trace_record_t;

    _Static_assert(sizeof(gth_trace_record_t) == 32, "record must be 32 bytes");

    /*
     * Calculate FNV-1a checksum for record validation.
     * Hashes all bytes except the checksum field itself (last 4 bytes).
     * Uses byte-level access to avoid strict aliasing violations.
     */
    static inline uint32_t gth_trace_calc_checksum(const gth_trace_record_t *rec)
    {
        const uint8_t *bytes = (const uint8_t *)rec;
        uint32_t hash = 2166136261U; /* FNV offset basis (0x119DE1F3) */
        uint32_t i;

        /* Hash all bytes except the checksum field (last sizeof(uint32_t) bytes) */
        for (i = 0; i < (GTH_TRACE_RECORD_SIZE - sizeof(uint32_t)); ++i)
        {
            hash ^= (uint32_t)bytes[i];
            hash *= 16777619U; /* FNV prime (0x01000193) */
        }

        return hash;
    }

    /*
     * Validate a trace record checksum
     */
    static inline int gth_trace_validate_record(const gth_trace_record_t *rec)
    {
        return rec->checksum == gth_trace_calc_checksum(rec);
    }

#ifdef __cplusplus
}
#endif

#endif /* GTHREADS_TRACE_FORMAT_H */
