// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * RTMR write path and ordered workqueue worker.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "extend.h"

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/string.h>

#include "ima.h"
#include "log.h"

struct workqueue_struct* extend_wq;
struct work_struct extend_work;

/* Written once in ima_rtmr_extend_init() before any reader runs (the kretprobe
 * and workqueue are armed afterwards), so they can live in .ro_after_init. */
static struct file* mr_file_ref __ro_after_init;
static u16 target_alg_id __ro_after_init;
static int target_digest_size __ro_after_init;
static int target_idx __ro_after_init;
static bool extend_disabled;

bool ima_rtmr_extend_disabled(void) {
    return READ_ONCE(extend_disabled);
}

bool ima_rtmr_do_extend(const struct ima_template_entry* entry) {
    const struct tpm_digest* slot = &entry->digests[target_idx];
    const u8* digest = slot->digest;
    u8 ff[TPM2_MAX_DIGEST_SIZE];
    loff_t pos = 0;
    ssize_t ret;

    if (READ_ONCE(extend_disabled))
        return false;

    /* target_idx is pinned at load time. A bank slot is tagged with our alg_id;
     * an extra slot and a violation both leave it zero. Any other tag means the
     * index drifted from the kernel's layout; stop rather than extend a
     * wrong-algorithm digest. */
    if (slot->alg_id != 0 && slot->alg_id != target_alg_id) {
        pr_err("digest slot %d tagged 0x%04x, want 0x%04x; disabling\n",
               target_idx,
               slot->alg_id,
               target_alg_id);
        WRITE_ONCE(extend_disabled, true);
        return false;
    }

    /* IMA logs an all-zero digest for violations and extends 0xFF into the TPM
     * PCR to invalidate it. Mirror the 0xFF so a verifier that replaces all-zero
     * log lines with 0xFF replays the same chain. */
    if (!memchr_inv(digest, 0, target_digest_size)) {
        memset(ff, 0xff, target_digest_size);
        digest = ff;
    }

    ret = kernel_write(mr_file_ref, digest, target_digest_size, &pos);
    if (ret != target_digest_size) {
        /* Any failure diverges RTMR from the IMA log; future writes would compound. */
        pr_err("RTMR extend failed: %zd (expected %d), disabling\n", ret, target_digest_size);
        WRITE_ONCE(extend_disabled, true);
        return false;
    }
    return true;
}

static void extend_work_fn(struct work_struct* work) {
    ima_rtmr_log_advance();
}

void ima_rtmr_extend_init(struct file* mr_file, u16 alg_id, int digest_size, int slot) {
    mr_file_ref = mr_file;
    target_alg_id = alg_id;
    target_digest_size = digest_size;
    target_idx = slot;
    WRITE_ONCE(extend_disabled, false);
    INIT_WORK(&extend_work, extend_work_fn);
}

void ima_rtmr_extend_exit(void) {
    /* No state to drop here: mr_file_ref is __ro_after_init and the caller
     * owns mr_file's lifetime (filp_close). The probe is already unregistered
     * and the workqueue destroyed by the time we run, so no reader can observe
     * mr_file_ref. Kept as the symmetric counterpart to ima_rtmr_extend_init()
     * for the init error path. */
}
