// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * FIFO queue and workqueue for asynchronous RTMR extension.
 *
 * Concurrent IMA measurements on different CPUs may be inserted into the
 * FIFO in an order that differs from the canonical IMA measurement list.
 * This is an inherent limitation of the kretprobe approach. Attestation
 * verifiers must account for this when replaying the RTMR hash chain
 * against the IMA log.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "extend.h"

#include <linux/fs.h>
#include <linux/kfifo.h>

#include "consts.h"
#include "handler.h"
#include "ima.h"

struct workqueue_struct* extend_wq;
struct work_struct extend_work;

static struct file* mr_file_ref;
static u16 target_alg_id;
static int target_digest_size;
static int max_digest_banks;
static DEFINE_KFIFO(extend_fifo, struct extend_request, EXTEND_FIFO_SIZE);
static DEFINE_SPINLOCK(fifo_lock);

static bool extend_disabled;

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

void ima_rtmr_extend_drain(void) {
    struct extend_request req;

    while (kfifo_out_spinlocked(&extend_fifo, &req, 1, &fifo_lock)) {
        const u8* digest;
        loff_t pos = 0;
        ssize_t ret;

        if (extend_disabled)
            continue;

        digest = find_digest(req.entry);
        if (!digest) {
            pr_warn_ratelimited("no digest for alg_id 0x%04x in entry\n",
                                target_alg_id);
            continue;
        }

        ret = kernel_write(mr_file_ref, digest, target_digest_size, &pos);
        if (ret < 0) {
            pr_warn_ratelimited("RTMR extend failed: %zd\n", ret);
            if (ret == -ENODEV || ret == -ENXIO) {
                pr_err("RTMR device lost, disabling extension\n");
                extend_disabled = true;
            }
        }
    }
}

static void extend_work_fn(struct work_struct* work) {
    static unsigned int last_nmissed;
    unsigned int missed = ima_rtmr_kretprobe.nmissed;

    if (missed != last_nmissed) {
        pr_warn("%u probe events missed (total: %u)\n",
                missed - last_nmissed,
                missed);
        last_nmissed = missed;
    }

    ima_rtmr_extend_drain();
}

void ima_rtmr_extend_init(struct file* mr_file, u16 alg_id, int digest_size, int num_banks) {
    mr_file_ref = mr_file;
    target_alg_id = alg_id;
    target_digest_size = digest_size;
    max_digest_banks = num_banks;
    INIT_WORK(&extend_work, extend_work_fn);
}
