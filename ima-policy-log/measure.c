// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Accumulate rule strings captured by handler.c into a fixed buffer
 * and feed them into process_buffer_measurement() once the IMA policy
 * commit (ima_update_policy) returns.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "measure.h"

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mnt_idmapping.h>
#include <linux/mount.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include "handler.h"
#include "utils.h"

#define ACC_SIZE (64 * 1024)

typedef int (*pbm_fn_t)(struct mnt_idmap*, struct inode*, const void*, int, const char*, int, int, const char*, bool, u8*, size_t);

struct workqueue_struct* ima_policy_log_wq;
struct work_struct ima_policy_log_work;

static pbm_fn_t pbm_fn;

static char acc_buf[ACC_SIZE];
static size_t acc_len;
/*
 * Snapshot of the parse kretprobe's nmissed counter taken when the
 * accumulator was last reset (the start of the current load's accumulation).
 * ima_policy_log_check_missed() compares the live counter against this: any
 * increase means a parse return was dropped and a rule went uncaptured, so
 * the accumulator no longer holds the complete policy. Protected by acc_lock.
 */
static unsigned long acc_missed_base;
/*
 * Sticky fail-closed flag for the in-flight policy load. Once set (rule
 * truncation, accumulator overflow, a missed parse-probe return, or a
 * scratch-alloc failure) the accumulator can no longer be trusted to hold
 * the complete policy, so measure_work_fn() refuses to emit a measurement.
 * Cleared only when the accumulator is reset (successful drain or the
 * ima_delete_rules() abandon hook). Protected by acc_lock.
 */
static bool acc_invalid;
static DEFINE_SPINLOCK(acc_lock);

void ima_policy_log_accumulate(const char* rule) {
    size_t rlen = strnlen(rule, ACC_SIZE);
    unsigned long flags;

    spin_lock_irqsave(&acc_lock, flags);
    if (acc_len + rlen + 1 < ACC_SIZE) {
        memcpy(acc_buf + acc_len, rule, rlen);
        acc_buf[acc_len + rlen] = '\n';
        acc_len += rlen + 1;
    } else {
        /* Fail-closed: a dropped rule means the hash would diverge. */
        acc_invalid = true;
    }
    spin_unlock_irqrestore(&acc_lock, flags);
}

void ima_policy_log_invalidate(void) {
    unsigned long flags;

    spin_lock_irqsave(&acc_lock, flags);
    acc_invalid = true;
    spin_unlock_irqrestore(&acc_lock, flags);
}

void ima_policy_log_reset(void) {
    unsigned long flags;

    spin_lock_irqsave(&acc_lock, flags);
    acc_len = 0;
    acc_invalid = false;
    acc_missed_base = READ_ONCE(ima_policy_log_parse_kretprobe.nmissed);
    spin_unlock_irqrestore(&acc_lock, flags);
}

void ima_policy_log_check_missed(void) {
    unsigned long flags;

    spin_lock_irqsave(&acc_lock, flags);
    if (READ_ONCE(ima_policy_log_parse_kretprobe.nmissed) != acc_missed_base)
        acc_invalid = true;
    spin_unlock_irqrestore(&acc_lock, flags);
}

/*
 * Drain the accumulator into @out. Returns the number of bytes copied, or
 * a negative errno if the accumulator was marked invalid (in which case the
 * accumulator is still cleared so it cannot contaminate the next load).
 * The accumulator is always reset, so this doubles as the success-path
 * teardown.
 */
static ssize_t acc_drain(char* out, size_t out_size) {
    unsigned long flags;
    ssize_t n;

    spin_lock_irqsave(&acc_lock, flags);
    if (acc_invalid) {
        n = -EINVAL;
    } else {
        n = min(acc_len, out_size);
        memcpy(out, acc_buf, n);
    }
    acc_len = 0;
    acc_invalid = false;
    acc_missed_base = READ_ONCE(ima_policy_log_parse_kretprobe.nmissed);
    spin_unlock_irqrestore(&acc_lock, flags);
    return n;
}

static void measure_work_fn(struct work_struct* work) {
    char* buf;
    ssize_t size;
    int rc;

    /*
     * Drain before anything that can fail: even if the scratch buffer
     * cannot be allocated the accumulator must be reset, otherwise the
     * just-committed rules would be prepended to the next policy load.
     */
    buf = kvmalloc(ACC_SIZE, GFP_KERNEL);
    if (!buf) {
        ima_policy_log_reset();
        pr_err("scratch alloc failed; dropped policy measurement\n");
        return;
    }

    size = acc_drain(buf, ACC_SIZE);
    if (size < 0) {
        /* Fail-closed: an inconsistency was flagged during this load. */
        kvfree(buf);
        pr_err("accumulator invalid; refusing to measure policy\n");
        return;
    }
    if (size == 0) {
        kvfree(buf);
        return;
    }

    /* func=NONE skips ima_get_action(); eventname is the filename hint. */
    rc = pbm_fn(&nop_mnt_idmap, NULL, buf, (int)size, "ima-policy", 0, 0, "ima_policy_log", false, NULL, 0);
    kvfree(buf);

    if (rc < 0)
        pr_err("process_buffer_measurement failed: %d\n", rc);
    else
        pr_info("measured %zd bytes of IMA policy\n", size);
}

int __init ima_policy_log_measure_init(void) {
    unsigned long addr = ima_policy_log_ksym_lookup("process_buffer_measurement");

    if (!addr) {
        pr_err("cannot resolve process_buffer_measurement\n");
        return -ENOENT;
    }
    pbm_fn = (pbm_fn_t)addr;

    ima_policy_log_wq = alloc_ordered_workqueue("ima_policy_log", 0);
    if (!ima_policy_log_wq)
        return -ENOMEM;
    INIT_WORK(&ima_policy_log_work, measure_work_fn);
    return 0;
}

void ima_policy_log_measure_exit(void) {
    if (ima_policy_log_wq) {
        destroy_workqueue(ima_policy_log_wq);
        ima_policy_log_wq = NULL;
    }
}
