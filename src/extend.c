// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * RTMR write path and ordered workqueue worker.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "extend.h"

#include <linux/fs.h>

#include "ima.h"
#include "log.h"

struct workqueue_struct* extend_wq;
struct work_struct extend_work;

static struct file* mr_file_ref;
static u16 target_alg_id;
static int target_digest_size;
static int max_digest_banks;
static bool extend_disabled;

bool ima_rtmr_extend_disabled(void) {
    return READ_ONCE(extend_disabled);
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

void ima_rtmr_do_extend(const struct ima_template_entry* entry) {
    const u8* digest;
    loff_t pos = 0;
    ssize_t ret;

    if (READ_ONCE(extend_disabled))
        return;

    digest = find_digest(entry);
    if (!digest) {
        pr_warn_ratelimited("no digest for alg_id 0x%04x in entry\n", target_alg_id);
        return;
    }

    ret = kernel_write(mr_file_ref, digest, target_digest_size, &pos);
    if (ret != target_digest_size) {
        /* Any failure diverges RTMR from the IMA log; future writes would compound. */
        pr_err("RTMR extend failed: %zd (expected %d), disabling\n", ret, target_digest_size);
        WRITE_ONCE(extend_disabled, true);
    }
}

static void extend_work_fn(struct work_struct* work) {
    ima_rtmr_log_advance();
}

void ima_rtmr_extend_init(struct file* mr_file, u16 alg_id, int digest_size, int num_banks) {
    mr_file_ref = mr_file;
    target_alg_id = alg_id;
    target_digest_size = digest_size;
    max_digest_banks = num_banks;
    WRITE_ONCE(extend_disabled, false);
    INIT_WORK(&extend_work, extend_work_fn);
}
