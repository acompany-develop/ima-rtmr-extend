// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Walk ima_measurements (append-only RCU list) to recover IMA-log order
 * without probing inside ima_extend_list_mutex.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "log.h"

#include <linux/errno.h>
#include <linux/rculist.h>

#include "extend.h"
#include "ima.h"
#include "utils.h"

static struct list_head* ima_log_head;
static struct list_head* cursor;
/* Single writer: the ordered workqueue worker in ima_rtmr_log_advance(), which
 * the ordered wq guarantees never runs concurrently with itself. Read lockless
 * from sysfs, so READ_ONCE/WRITE_ONCE suffice (matches skip_count). */
static unsigned long extended_count;
static unsigned long skip_count;

int __init ima_rtmr_log_init(void) {
    unsigned long addr = ima_rtmr_ksym_lookup("ima_measurements");
    struct list_head* last;
    struct list_head* p;
    unsigned long n = 0;

    if (!addr) {
        pr_err("cannot resolve ima_measurements symbol\n");
        return -ENOENT;
    }
    WRITE_ONCE(ima_log_head, (struct list_head*)addr);

    /* Start past the entries counted here so they are not re-extended; the
     * verifier skips ahead by this count instead of scanning. The cursor must
     * be the last counted node, not head->prev, which a concurrent IMA append
     * could have moved past the counted snapshot. */
    rcu_read_lock();
    last = ima_log_head;
    list_for_each_rcu(p, ima_log_head) {
        last = p;
        n++;
    }
    WRITE_ONCE(cursor, last);
    rcu_read_unlock();
    WRITE_ONCE(skip_count, n);
    return 0;
}

void ima_rtmr_log_advance(void) {
    struct list_head* tail;

    /* Snapshot the tail so a steady stream of new entries cannot pin this
     * worker indefinitely; the next kretprobe re-queues us for the rest. */
    rcu_read_lock();
    tail = rcu_dereference(READ_ONCE(ima_log_head)->prev);
    rcu_read_unlock();

    while (READ_ONCE(cursor) != tail) {
        struct list_head* next;
        struct ima_queue_entry* qe;

        if (ima_rtmr_extend_disabled())
            return;

        /* Entries stay allocated while extension is enabled: kernels with
         * ima_queue_stage() (v7.2+) can detach and free them, but the stage
         * guard disables us before the detach, and the check above precedes
         * every dereference. See docs/known-issues.md for the residual race. */
        next = rcu_dereference_check(READ_ONCE(cursor)->next, 1);

        qe = list_entry(next, struct ima_queue_entry, later);
        /* A failed entry was not written to the RTMR, so it must not count
         * as extended nor be skipped over. */
        if (!ima_rtmr_do_extend(qe->entry))
            return;
        WRITE_ONCE(cursor, next);
        WRITE_ONCE(extended_count, READ_ONCE(extended_count) + 1);
    }
}

unsigned long ima_rtmr_log_extended_count(void) {
    return READ_ONCE(extended_count);
}

unsigned long ima_rtmr_log_skip_count(void) {
    return READ_ONCE(skip_count);
}
