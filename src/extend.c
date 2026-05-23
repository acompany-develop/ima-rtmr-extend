// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * FIFO queue, reorder buffer, and workqueue for asynchronous RTMR extension.
 *
 * When sequencing is enabled (kprobe on ima_add_digest_entry succeeded),
 * entries dequeued from the FIFO are placed into a small reorder buffer
 * and extended to RTMR in sequence-number order. This corrects the rare
 * case where kretprobe return handlers fire out of IMA-log order.
 *
 * When sequencing is unavailable, entries are extended in FIFO order.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "extend.h"

#include <linux/bitmap.h>
#include <linux/fs.h>
#include <linux/kfifo.h>

#include "consts.h"
#include "handler.h"
#include "ima.h"
#include "seq.h"

/*
 * Maximum number of out-of-order entries we buffer before giving up and
 * flushing in whatever order we have. Must be a power of 2 for kfifo.
 */
#define REORDER_BUF_SIZE 32

struct workqueue_struct* extend_wq;
struct work_struct extend_work;

static struct file* mr_file_ref;
static u16 target_alg_id;
static int target_digest_size;
static int max_digest_banks;
static DEFINE_KFIFO(extend_fifo, struct extend_request, EXTEND_FIFO_SIZE);
static DEFINE_SPINLOCK(fifo_lock);

static bool extend_disabled;

struct pending_extend {
    struct ima_template_entry* entry;
    u64 seq;
};

static struct pending_extend reorder_buf[REORDER_BUF_SIZE];
static int reorder_count;
static u64 next_expected_seq = 1; /* atomic64_inc_return starts at 1 */

/*
 * Bitmap of skipped sequence numbers (failed/duplicate entries).
 * Indexed by (seq - next_expected_seq). Allows the reorder flush to
 * skip over gaps without stalling.
 */
#define SKIP_BITMAP_BITS 64
static DECLARE_BITMAP(skip_bitmap, SKIP_BITMAP_BITS);
static DEFINE_SPINLOCK(skip_lock);

/*
 * Mark a sequence number as skipped (entry was not added to IMA log).
 * Called from kretprobe context (atomic) when ima_add_template_entry failed.
 */
void ima_rtmr_seq_skip(u64 seq) {
    unsigned long flags;

    spin_lock_irqsave(&skip_lock, flags);
    if (seq >= next_expected_seq &&
        seq - next_expected_seq < SKIP_BITMAP_BITS) {
        set_bit(seq - next_expected_seq, skip_bitmap);
    }
    spin_unlock_irqrestore(&skip_lock, flags);

    /* Kick the workqueue so reorder_flush can advance past the skip */
    queue_work(extend_wq, &extend_work);
}

/* Caller must hold skip_lock. */
static bool consume_skip(u64 seq) {
    if (seq >= next_expected_seq &&
        seq - next_expected_seq < SKIP_BITMAP_BITS)
        return test_and_clear_bit(seq - next_expected_seq, skip_bitmap);
    return false;
}

/* Caller must hold skip_lock. */
static void shift_skip_bitmap(int n) {
    bitmap_shift_right(skip_bitmap, skip_bitmap, n, SKIP_BITMAP_BITS);
}

/* Safe from atomic context (spinlocked kfifo). */
int ima_rtmr_fifo_in(const struct extend_request* req) {
    return kfifo_in_spinlocked(&extend_fifo, req, 1, &fifo_lock);
}

static const u8* find_digest(const struct ima_template_entry* entry) {
    int i;

    for (i = 0; i < max_digest_banks; i++) {
        if (entry->digests[i].alg_id == target_alg_id)
            return entry->digests[i].digest;
        if (entry->digests[i].alg_id == 0)
            break;
    }

    return NULL;
}

static bool do_extend(const struct ima_template_entry* entry) {
    const u8* digest;
    loff_t pos = 0;
    ssize_t ret;

    if (extend_disabled)
        return true;

    digest = find_digest(entry);
    if (!digest) {
        pr_warn_ratelimited("no digest for alg_id 0x%04x in entry\n",
                            target_alg_id);
        return true;
    }

    ret = kernel_write(mr_file_ref, digest, target_digest_size, &pos);
    if (ret != target_digest_size) {
        pr_warn_ratelimited("RTMR extend failed: %zd (expected %d)\n",
                            ret,
                            target_digest_size);
        if (ret == -ENODEV || ret == -ENXIO) {
            pr_err("RTMR device lost, disabling extension\n");
            extend_disabled = true;
        }
        return true;
    }

    return true;
}

/* Insertion sort by seq. */
static void reorder_insert(struct pending_extend* pe) {
    int i = reorder_count;

    while (i > 0 && reorder_buf[i - 1].seq > pe->seq) {
        reorder_buf[i] = reorder_buf[i - 1];
        i--;
    }
    reorder_buf[i] = *pe;
    reorder_count++;
}

static void reorder_pop_front(void) {
    reorder_count--;
    memmove(&reorder_buf[0],
            &reorder_buf[1],
            reorder_count * sizeof(reorder_buf[0]));
}

/*
 * Flush consecutive entries starting from next_expected_seq.
 *
 * The skip_lock is taken briefly to advance the sequence counter and
 * bitmap, then released before do_extend() which may sleep.
 */
static void reorder_flush(void) {
    for (;;) {
        unsigned long flags;
        bool should_extend = false;

        spin_lock_irqsave(&skip_lock, flags);

        while (consume_skip(next_expected_seq)) {
            shift_skip_bitmap(1);
            next_expected_seq++;
        }

        if (reorder_count > 0 &&
            reorder_buf[0].seq == next_expected_seq) {
            next_expected_seq++;
            shift_skip_bitmap(1);
            should_extend = true;
        }

        spin_unlock_irqrestore(&skip_lock, flags);

        if (!should_extend)
            break;

        /* do_extend may sleep — must be called without spinlock */
        do_extend(reorder_buf[0].entry);
        reorder_pop_front();
    }
}

/*
 * Force-flush the entire reorder buffer in seq order. Called when the
 * buffer is full and we cannot make progress (a sequence number was lost
 * due to a failed ima_add_digest_entry, a missed probe, or a full seq map).
 */
static void reorder_force_flush(void) {
    while (reorder_count > 0) {
        unsigned long flags;
        u64 head_seq = reorder_buf[0].seq;

        pr_warn_ratelimited(
            "forcing extend: expected seq %llu, got %llu\n",
            next_expected_seq,
            head_seq);

        spin_lock_irqsave(&skip_lock, flags);
        /* Stale head: extend it but don't rewind sequence/bitmap state. */
        if (head_seq >= next_expected_seq) {
            u64 jump = head_seq + 1 - next_expected_seq;

            next_expected_seq = head_seq + 1;
            if (jump < SKIP_BITMAP_BITS)
                shift_skip_bitmap(jump);
            else
                bitmap_zero(skip_bitmap, SKIP_BITMAP_BITS);
        }
        spin_unlock_irqrestore(&skip_lock, flags);

        do_extend(reorder_buf[0].entry);
        reorder_pop_front();
    }
}

static void drain_ordered(void) {
    struct extend_request req;

    while (kfifo_out_spinlocked(&extend_fifo, &req, 1, &fifo_lock)) {
        struct pending_extend pe;

        /* Unsequenced entries would wedge position 0 of the reorder buffer. */
        if (req.seq == 0) {
            do_extend(req.entry);
            continue;
        }

        pe.entry = req.entry;
        pe.seq = req.seq;

        if (reorder_count >= REORDER_BUF_SIZE)
            reorder_force_flush();

        reorder_insert(&pe);
    }

    reorder_flush();

    /*
     * Entries that remain are waiting for an earlier sequence number
     * that has not arrived yet. A future kretprobe firing will trigger
     * another drain attempt. If the buffer is full, force-flush to avoid
     * stalling.
     */
    if (reorder_count >= REORDER_BUF_SIZE)
        reorder_force_flush();
}

static void drain_fifo(void) {
    struct extend_request req;

    while (kfifo_out_spinlocked(&extend_fifo, &req, 1, &fifo_lock))
        do_extend(req.entry);
}

/*
 * Stall-recovery threshold: number of consecutive workqueue runs with no
 * progress and no new entries arriving before we assume the missing seq
 * will never come (typically a nmissed kretprobe) and force-flush.
 */
#define STALL_LIMIT 32

static void extend_work_fn(struct work_struct* work) {
    static u64 last_seen_next_expected;
    static unsigned int stall;
    static unsigned int last_nmissed;
    unsigned int missed = ima_rtmr_kretprobe.nmissed;

    if (missed != last_nmissed) {
        pr_warn("%u probe events missed (total: %u)\n",
                missed - last_nmissed,
                missed);
        last_nmissed = missed;
    }

    if (ima_rtmr_seq_enabled())
        drain_ordered();
    else
        drain_fifo();

    if (reorder_count > 0) {
        if (next_expected_seq != last_seen_next_expected) {
            stall = 0;
            last_seen_next_expected = next_expected_seq;
        } else if (kfifo_is_empty(&extend_fifo) && ++stall >= STALL_LIMIT) {
            /* A nmissed kretprobe leaves a permanent gap; advance past it. */
            pr_warn_ratelimited("stall recovery: force-flushing %d entries past missing seq %llu\n",
                                reorder_count,
                                next_expected_seq);
            reorder_force_flush();
            stall = 0;
            last_seen_next_expected = next_expected_seq;
        }
        queue_work(extend_wq, &extend_work);
    } else {
        stall = 0;
    }
}

void ima_rtmr_extend_init(struct file* mr_file, u16 alg_id, int digest_size, int num_banks) {
    mr_file_ref = mr_file;
    target_alg_id = alg_id;
    target_digest_size = digest_size;
    max_digest_banks = num_banks;
    INIT_WORK(&extend_work, extend_work_fn);
}
