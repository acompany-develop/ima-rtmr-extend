// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Extend IMA measurements to Confidential Computing runtime measurement
 * registers (RTMRs) via the tsm-mr sysfs interface. Works with any CC
 * architecture (Intel TDX, ARM CCA, ...) that implements tsm-mr.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/magic.h>
#include <linux/module.h>
#include <linux/tpm.h>

#include "consts.h"
#include "detect.h"
#include "extend.h"
#include "handler.h"
#include "seq.h"
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
    int rc;

    if (mr_path[0] != '\0') {
        mr_file = filp_open(mr_path, O_WRONLY, 0);
        if (IS_ERR(mr_file)) {
            pr_err("cannot open %s: %ld\n",
                   mr_path,
                   PTR_ERR(mr_file));
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

    chip = tpm_default_chip();
    if (!chip) {
        pr_err("no TPM chip available\n");
        rc = -ENODEV;
        goto err_close;
    }
    /* IMA allocates nr_allocated_banks + ima_extra_slots (at most 2) digests */
    num_banks = chip->nr_allocated_banks + 2;
    put_device(&chip->dev);

    extend_wq = alloc_ordered_workqueue("ima_rtmr", 0);
    if (!extend_wq) {
        rc = -ENOMEM;
        goto err_close;
    }

    ima_rtmr_extend_init(mr_file, alg->alg_id, alg->digest_size, num_banks);

    rc = register_kretprobe(&ima_rtmr_kretprobe);
    if (rc) {
        pr_err("cannot register kretprobe: %d\n", rc);
        goto err_destroy_wq;
    }

    rc = register_kprobe(&ima_rtmr_seq_kprobe);
    if (rc) {
        pr_warn("cannot register seq kprobe on ima_add_digest_entry: %d "
                "(ordering falls back to FIFO)\n",
                rc);
        /* non-fatal: module works without sequencing */
    } else {
        ima_rtmr_seq_activate();
        pr_info("sequencing enabled (kprobe on ima_add_digest_entry)\n");
    }

    pr_info("loaded (%s, digest %d bytes)\n", hash_name, alg->digest_size);
    return 0;

err_destroy_wq:
    destroy_workqueue(extend_wq);
err_close:
    filp_close(mr_file, NULL);
    return rc;
}

static void __exit ima_rtmr_exit(void) {
    if (ima_rtmr_seq_enabled())
        unregister_kprobe(&ima_rtmr_seq_kprobe);
    unregister_kretprobe(&ima_rtmr_kretprobe);

    flush_workqueue(extend_wq);
    destroy_workqueue(extend_wq);

    if (ima_rtmr_kretprobe.nmissed)
        pr_info("%d probe instances missed\n",
                ima_rtmr_kretprobe.nmissed);
    if (atomic_long_read(&ima_rtmr_drops))
        pr_info("%ld measurements dropped (FIFO full)\n",
                atomic_long_read(&ima_rtmr_drops));

    filp_close(mr_file, NULL);

    pr_info("unloaded\n");
}

module_init(ima_rtmr_init);
module_exit(ima_rtmr_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Acompany Co., Ltd.");
MODULE_DESCRIPTION("Extend IMA measurements to CC runtime measurement registers via tsm-mr");
