// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Extend IMA measurements to Confidential Computing runtime measurement
 * registers (RTMRs) via the tsm-mr sysfs interface.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/magic.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/tpm.h>

#include "detect.h"
#include "extend.h"
#include "handler.h"
#include "log.h"
#include "sysfs.h"
#include "utils.h"

static char* mr_path = "";
module_param(mr_path, charp, 0444);
MODULE_PARM_DESC(mr_path,
                 "sysfs path of the RTMR to extend "
                 "(e.g. /sys/class/misc/tdx_guest/measurements/rtmr2:sha384)");

static struct file* mr_file;

static int __init ima_rtmr_init(void) {
    const struct hash_alg_info* alg;
    struct tpm_chip* chip;
    const char* hash_name;
    const char* path;
    int num_banks;
    int extra_slots = 0;
    int target_idx;
    int i;
    int rc;

    if (mr_path[0] != '\0') {
        /* A tsm-mr RTMR attribute lives under a .../measurements/ directory and
         * carries a ":<hash>" suffix (e.g. .../measurements/rtmr2:sha384). Reject
         * obviously-wrong targets before opening; the SYSFS_MAGIC check below
         * still guards against a same-shaped path on another filesystem. The
         * default candidate path in detect.c also matches this pattern. */
        if (!strstr(mr_path, "/measurements/") || !strrchr(mr_path, ':')) {
            pr_err("mr_path does not look like a tsm-mr RTMR attribute: %s\n", mr_path);
            return -EINVAL;
        }
        mr_file = filp_open(mr_path, O_RDWR, 0);
        if (IS_ERR(mr_file)) {
            pr_err("cannot open %s: %ld\n", mr_path, PTR_ERR(mr_file));
            return PTR_ERR(mr_file);
        }
        path = mr_path;
    } else {
        mr_file = ima_rtmr_detect(&path);
        if (IS_ERR(mr_file)) {
            pr_err("no writable RTMR found (try mr_path=)\n");
            return PTR_ERR(mr_file);
        }
    }

    pr_info("using %s\n", path);

    if (mr_file->f_inode->i_sb->s_magic != SYSFS_MAGIC) {
        pr_err("mr_path must point to a sysfs attribute: %s\n", path);
        rc = -EINVAL;
        goto err_close;
    }

    hash_name = parse_hash_from_path(path);
    if (!hash_name) {
        pr_err("cannot determine hash algorithm from %s\n", path);
        rc = -EINVAL;
        goto err_close;
    }

    alg = lookup_alg(hash_name);
    if (!alg) {
        pr_err("unsupported hash algorithm: %s\n", hash_name);
        rc = -EINVAL;
        goto err_close;
    }

    /* Resolve which entry->digests[] slot holds the target algorithm and pin
     * it once. The kernel keys its own log export off this index, never off
     * alg_id (which it leaves unset on the ima_hash_algo extra slot and zeroes
     * entirely on violations), so we mirror the index instead of scanning.
     *
     * digests[] has NR_BANKS(chip) + ima_extra_slots entries; NR_BANKS() is 0
     * when no TPM is present. (Built-in init would need late_initcall_sync so
     * the kallsyms reads below see a populated IMA; insmod always does.) */
    chip = tpm_default_chip();
    num_banks = chip ? chip->nr_allocated_banks : 0;
    target_idx = -1;
    for (i = 0; i < num_banks; i++) {
        if (chip->allocated_banks[i].crypto_id == alg->hash_algo) {
            target_idx = i;
            break;
        }
    }
    if (chip)
        put_device(&chip->dev);

    rc = ima_rtmr_read_extra_slots(&extra_slots);
    if (rc) {
        if (num_banks == 0) {
            pr_err("no TPM and cannot resolve ima_extra_slots (%d)\n", rc);
            goto err_close;
        }
        pr_warn("cannot resolve ima_extra_slots (%d); scanning TPM banks only\n", rc);
    } else {
        num_banks += extra_slots;
    }

    /* No matching TPM bank: the digest can only live in IMA's ima_hash_algo
     * extra slot, and only when ima_hash_algo is our target algorithm. */
    if (target_idx < 0) {
        int kalgo, kidx;

        if (ima_rtmr_read_hash_algo(&kalgo) || kalgo != alg->hash_algo) {
            pr_err("IMA computes no %s digest; boot with ima_hash=%s\n", hash_name, hash_name);
            rc = -EINVAL;
            goto err_close;
        }
        if (ima_rtmr_read_hash_algo_idx(&kidx) || kidx >= num_banks) {
            pr_err("ima_hash_algo_idx out of range (max %d)\n", num_banks - 1);
            rc = -EINVAL;
            goto err_close;
        }
        target_idx = kidx;
    }

    extend_wq = alloc_ordered_workqueue("ima_rtmr", 0);
    if (!extend_wq) {
        rc = -ENOMEM;
        goto err_close;
    }

    ima_rtmr_extend_init(mr_file, alg->alg_id, alg->digest_size, target_idx);

    rc = ima_rtmr_log_init();
    if (rc) {
        pr_err("cannot init log walker: %d\n", rc);
        goto err_destroy_wq;
    }

    rc = ima_rtmr_sysfs_init(mr_file, alg->digest_size);
    if (rc) {
        pr_err("cannot init sysfs: %d\n", rc);
        goto err_destroy_wq;
    }

    rc = register_kretprobe(&ima_rtmr_kretprobe);
    if (rc) {
        pr_err("cannot register kretprobe: %d\n", rc);
        goto err_sysfs_exit;
    }

    /* Catch up entries added before the probe was armed. */
    queue_work(extend_wq, &extend_work);

    pr_info("loaded (%s, digest %d bytes)\n", hash_name, alg->digest_size);
    return 0;

err_sysfs_exit:
    ima_rtmr_sysfs_exit();
err_destroy_wq:
    destroy_workqueue(extend_wq);
    /* Symmetric with the success path's teardown: ima_rtmr_extend_init() ran
     * before this label, so tear it down before closing the file it referenced. */
    ima_rtmr_extend_exit();
err_close:
    filp_close(mr_file, NULL);
    return rc;
}

static void __exit ima_rtmr_exit(void) {
    unregister_kretprobe(&ima_rtmr_kretprobe);

    /* An entry appended between the last probe firing and unregistration may
     * never have queued a worker. Queue one final drain so the cursor reaches
     * the IMA-log tail; destroy_workqueue() then flushes it to completion.
     * Safe at exit: the probe is gone, we run in process context, and the
     * worker only walks the never-freed ima_measurements list. */
    queue_work(extend_wq, &extend_work);
    destroy_workqueue(extend_wq);

    if (ima_rtmr_kretprobe.nmissed)
        pr_info("%d probe instances missed\n", ima_rtmr_kretprobe.nmissed);

    ima_rtmr_sysfs_exit();
    filp_close(mr_file, NULL);
    ima_rtmr_extend_exit();

    pr_info("unloaded\n");
}

module_init(ima_rtmr_init);
module_exit(ima_rtmr_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Acompany Co., Ltd.");
MODULE_DESCRIPTION("Extend IMA measurements to CC runtime measurement registers via tsm-mr");
