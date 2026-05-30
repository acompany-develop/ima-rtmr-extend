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
#include <linux/mount.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include "utils.h"

#define ACC_SIZE (64 * 1024)

typedef int (*pbm_fn_t)(struct mnt_idmap*, struct inode*, const void*, int, const char*, int, int, const char*, bool, u8*, size_t);

extern struct mnt_idmap nop_mnt_idmap;

struct workqueue_struct* ima_policy_log_wq;
struct work_struct ima_policy_log_work;

static pbm_fn_t pbm_fn;

static char acc_buf[ACC_SIZE];
static size_t acc_len;
static DEFINE_SPINLOCK(acc_lock);

void ima_policy_log_accumulate(const char* rule) {
    size_t rlen = strnlen(rule, ACC_SIZE);
    unsigned long flags;

    spin_lock_irqsave(&acc_lock, flags);
    if (acc_len + rlen + 1 < ACC_SIZE) {
        memcpy(acc_buf + acc_len, rule, rlen);
        acc_buf[acc_len + rlen] = '\n';
        acc_len += rlen + 1;
    }
    spin_unlock_irqrestore(&acc_lock, flags);
}

static size_t acc_drain(char* out, size_t out_size) {
    unsigned long flags;
    size_t n;

    spin_lock_irqsave(&acc_lock, flags);
    n = min(acc_len, out_size);
    memcpy(out, acc_buf, n);
    acc_len = 0;
    spin_unlock_irqrestore(&acc_lock, flags);
    return n;
}

static void measure_work_fn(struct work_struct* work) {
    char* buf = kvmalloc(ACC_SIZE, GFP_KERNEL);
    size_t size;
    int rc;

    if (!buf)
        return;

    size = acc_drain(buf, ACC_SIZE);
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
        pr_info("measured %zu bytes of IMA policy\n", size);
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
