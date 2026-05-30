// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Append the hash of a runtime-loaded IMA policy as a new IMA log entry.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>

#include "handler.h"
#include "measure.h"

static int __init ima_policy_log_init(void) {
    int rc;

    rc = ima_policy_log_measure_init();
    if (rc)
        return rc;

    rc = register_kretprobe(&ima_policy_log_parse_kretprobe);
    if (rc) {
        pr_err("cannot register parse kretprobe: %d\n", rc);
        goto err_measure;
    }

    rc = register_kretprobe(&ima_policy_log_update_kretprobe);
    if (rc) {
        pr_err("cannot register update kretprobe: %d\n", rc);
        unregister_kretprobe(&ima_policy_log_parse_kretprobe);
        goto err_measure;
    }

    pr_info("loaded\n");
    return 0;

err_measure:
    ima_policy_log_measure_exit();
    return rc;
}

static void __exit ima_policy_log_exit(void) {
    unregister_kretprobe(&ima_policy_log_update_kretprobe);
    unregister_kretprobe(&ima_policy_log_parse_kretprobe);

    if (ima_policy_log_update_kretprobe.nmissed)
        pr_info("%d update probe instances missed\n",
                ima_policy_log_update_kretprobe.nmissed);
    if (ima_policy_log_parse_kretprobe.nmissed)
        pr_info("%d parse probe instances missed\n",
                ima_policy_log_parse_kretprobe.nmissed);

    ima_policy_log_measure_exit();
    pr_info("unloaded\n");
}

module_init(ima_policy_log_init);
module_exit(ima_policy_log_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Acompany Co., Ltd.");
MODULE_DESCRIPTION("Append the loaded IMA policy hash as a new IMA log entry");
