// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * /sys/kernel/ima_rtmr/ - diagnostic interface for the verifier.
 *
 *   initial      pre-load RTMR snapshot (hex). The validator uses this as
 *                the replay baseline, recovering measurements lost before
 *                the module attached.
 *   disabled     1 if extend has been permanently disabled (RTMR sysfs
 *                returned -ENODEV/-ENXIO).
 *   drops        kfifo drop count (RTMR coverage gap if non-zero).
 *   nmissed      kretprobe miss count (same gap risk).
 *   seq_enabled  1 if the sequence kprobe is active (true IMA-log order),
 *                0 if degraded to FIFO order.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "sysfs.h"

#include <linux/atomic.h>
#include <linux/hex.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include "detect.h"
#include "extend.h"
#include "handler.h"
#include "seq.h"

static struct kobject* ima_rtmr_kobj;
static char* initial_hex;

static ssize_t initial_show(struct kobject* k,
                            struct kobj_attribute* a,
                            char* buf) {
    return sysfs_emit(buf, "%s\n", initial_hex ? initial_hex : "");
}

static ssize_t disabled_show(struct kobject* k,
                             struct kobj_attribute* a,
                             char* buf) {
    return sysfs_emit(buf, "%d\n", ima_rtmr_extend_disabled() ? 1 : 0);
}

static ssize_t drops_show(struct kobject* k,
                          struct kobj_attribute* a,
                          char* buf) {
    return sysfs_emit(buf, "%ld\n", atomic_long_read(&ima_rtmr_drops));
}

static ssize_t nmissed_show(struct kobject* k,
                            struct kobj_attribute* a,
                            char* buf) {
    return sysfs_emit(buf, "%u\n", ima_rtmr_kretprobe.nmissed);
}

static ssize_t seq_enabled_show(struct kobject* k,
                                struct kobj_attribute* a,
                                char* buf) {
    return sysfs_emit(buf, "%d\n", ima_rtmr_seq_enabled() ? 1 : 0);
}

static struct kobj_attribute initial_attr = __ATTR_RO(initial);
static struct kobj_attribute disabled_attr = __ATTR_RO(disabled);
static struct kobj_attribute drops_attr = __ATTR_RO(drops);
static struct kobj_attribute nmissed_attr = __ATTR_RO(nmissed);
static struct kobj_attribute seq_enabled_attr = __ATTR_RO(seq_enabled);

static struct attribute* ima_rtmr_attrs[] = {
    &initial_attr.attr,
    &disabled_attr.attr,
    &drops_attr.attr,
    &nmissed_attr.attr,
    &seq_enabled_attr.attr,
    NULL,
};

static const struct attribute_group ima_rtmr_attr_group = {
    .attrs = ima_rtmr_attrs,
};

int ima_rtmr_sysfs_init(struct file* mr_file, int digest_size) {
    u8* raw;
    int rc;

    raw = kmalloc(digest_size, GFP_KERNEL);
    initial_hex = kmalloc(2 * digest_size + 1, GFP_KERNEL);
    if (!raw || !initial_hex) {
        rc = -ENOMEM;
        goto err_free;
    }

    rc = ima_rtmr_snapshot_initial(mr_file, digest_size, raw);
    if (rc) {
        pr_warn("cannot snapshot initial RTMR value: %d\n", rc);
        goto err_free;
    }
    bin2hex(initial_hex, raw, digest_size);
    initial_hex[2 * digest_size] = '\0';
    kfree(raw);
    raw = NULL;

    ima_rtmr_kobj = kobject_create_and_add(KBUILD_MODNAME, kernel_kobj);
    if (!ima_rtmr_kobj) {
        rc = -ENOMEM;
        goto err_free;
    }

    rc = sysfs_create_group(ima_rtmr_kobj, &ima_rtmr_attr_group);
    if (rc) {
        kobject_put(ima_rtmr_kobj);
        ima_rtmr_kobj = NULL;
        goto err_free;
    }

    pr_info("sysfs ready (/sys/kernel/%s/), initial RTMR: %s\n",
            KBUILD_MODNAME,
            initial_hex);
    return 0;

err_free:
    kfree(raw);
    kfree(initial_hex);
    initial_hex = NULL;
    return rc;
}

void ima_rtmr_sysfs_exit(void) {
    if (ima_rtmr_kobj) {
        sysfs_remove_group(ima_rtmr_kobj, &ima_rtmr_attr_group);
        kobject_put(ima_rtmr_kobj);
        ima_rtmr_kobj = NULL;
    }
    kfree(initial_hex);
    initial_hex = NULL;
}
