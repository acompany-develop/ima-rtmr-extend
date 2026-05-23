// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Sequence number assignment via kprobe on ima_add_digest_entry().
 *
 * ima_add_digest_entry() is called inside ima_extend_list_mutex, so the
 * sequence number assigned here reflects the true IMA measurement list
 * order. The kretprobe return handler on ima_add_template_entry() later
 * consumes this number and attaches it to the extend request.
 *
 * The mapping is a small fixed-size table keyed by the ima_template_entry
 * pointer. It only needs to hold as many entries as there are CPUs in the
 * window between ima_add_digest_entry() and the kretprobe return.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "seq.h"

#include <linux/ptrace.h>
#include <linux/spinlock.h>

#include "ima.h"

#define SEQ_MAP_SIZE 32

struct seq_map_slot {
    const struct ima_template_entry* entry;
    u64 seq;
};

static atomic64_t seq_counter = ATOMIC64_INIT(0);
static struct seq_map_slot seq_map[SEQ_MAP_SIZE];
static DEFINE_SPINLOCK(seq_map_lock);
static bool seq_active;

/* Fires inside ima_extend_list_mutex, so the sequence number reflects IMA log order. */
static int seq_pre_handler(struct kprobe* p, struct pt_regs* regs) {
    struct ima_template_entry* entry =
        (struct ima_template_entry*)regs_get_kernel_argument(regs, 0);
    u64 seq = atomic64_inc_return(&seq_counter);
    unsigned long flags;
    int i;

    spin_lock_irqsave(&seq_map_lock, flags);
    for (i = 0; i < SEQ_MAP_SIZE; i++) {
        if (!seq_map[i].entry) {
            seq_map[i].entry = entry;
            seq_map[i].seq = seq;
            break;
        }
    }
    spin_unlock_irqrestore(&seq_map_lock, flags);

    if (i == SEQ_MAP_SIZE)
        pr_warn_ratelimited("seq map full, ordering may be lost\n");

    return 0;
}

u64 ima_rtmr_seq_consume(const struct ima_template_entry* entry) {
    unsigned long flags;
    u64 seq = 0;
    int i;

    if (!READ_ONCE(seq_active))
        return 0;

    spin_lock_irqsave(&seq_map_lock, flags);
    for (i = 0; i < SEQ_MAP_SIZE; i++) {
        if (seq_map[i].entry == entry) {
            seq = seq_map[i].seq;
            seq_map[i].entry = NULL;
            break;
        }
    }
    spin_unlock_irqrestore(&seq_map_lock, flags);

    return seq;
}

bool ima_rtmr_seq_enabled(void) {
    return READ_ONCE(seq_active);
}

struct kprobe ima_rtmr_seq_kprobe = {
    .pre_handler = seq_pre_handler,
    .symbol_name = "ima_add_digest_entry",
};

void __init ima_rtmr_seq_activate(void) {
    WRITE_ONCE(seq_active, true);
}

void __init ima_rtmr_seq_deactivate(void) {
    WRITE_ONCE(seq_active, false);
}
