// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Walk ima_measurements (append-only RCU list) to recover IMA-log order
 * without probing inside ima_extend_list_mutex.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "log.h"

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/rculist.h>

#include "extend.h"
#include "ima.h"
#include "utils.h"

static struct list_head* ima_log_head;
static struct list_head* cursor;
static atomic_long_t extended_count;

int __init ima_rtmr_log_init(void) {
    unsigned long addr = ima_rtmr_ksym_lookup("ima_measurements");

    if (!addr) {
        pr_err("cannot resolve ima_measurements symbol\n");
        return -ENOENT;
    }
    ima_log_head = (struct list_head*)addr;

    /* Start past the current tail so pre-load entries are not re-extended. */
    rcu_read_lock();
    cursor = rcu_dereference(ima_log_head->prev);
    rcu_read_unlock();
    return 0;
}

void ima_rtmr_log_advance(void) {
    for (;;) {
        struct list_head* next;
        struct ima_queue_entry* qe;

        rcu_read_lock();
        next = rcu_dereference(cursor->next);
        rcu_read_unlock();

        if (next == ima_log_head)
            return;

        qe = list_entry(next, struct ima_queue_entry, later);
        ima_rtmr_do_extend(qe->entry);
        cursor = next;
        atomic_long_inc(&extended_count);
    }
}

unsigned long ima_rtmr_log_extended_count(void) {
    return (unsigned long)atomic_long_read(&extended_count);
}
