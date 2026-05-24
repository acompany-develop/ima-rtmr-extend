// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * /sys/kernel/ima_rtmr/ - diagnostic surface for the verifier.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "sysfs.h"

#include <linux/hex.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include "detect.h"
#include "extend.h"
#include "handler.h"
#include "log.h"

static struct kobject* ima_rtmr_kobj;
static char* initial_hex;

static ssize_t initial_show(struct kobject* k, struct kobj_attribute* a, char* buf) {
    return sysfs_emit(buf, "%s\n", initial_hex);
}

static ssize_t disabled_show(struct kobject* k, struct kobj_attribute* a, char* buf) {
    return sysfs_emit(buf, "%d\n", ima_rtmr_extend_disabled() ? 1 : 0);
}

static ssize_t extended_count_show(struct kobject* k, struct kobj_attribute* a, char* buf) {
    return sysfs_emit(buf, "%lu\n", ima_rtmr_log_extended_count());
}

static ssize_t skip_count_show(struct kobject* k, struct kobj_attribute* a, char* buf) {
    return sysfs_emit(buf, "%lu\n", ima_rtmr_log_skip_count());
}

static ssize_t nmissed_show(struct kobject* k, struct kobj_attribute* a, char* buf) {
    return sysfs_emit(buf, "%d\n", READ_ONCE(ima_rtmr_kretprobe.nmissed));
}

static struct kobj_attribute initial_attr = __ATTR_RO(initial);
static struct kobj_attribute disabled_attr = __ATTR_RO(disabled);
static struct kobj_attribute extended_count_attr = __ATTR_RO(extended_count);
static struct kobj_attribute skip_count_attr = __ATTR_RO(skip_count);
static struct kobj_attribute nmissed_attr = __ATTR_RO(nmissed);

static struct attribute* ima_rtmr_attrs[] = {
    &initial_attr.attr,
    &disabled_attr.attr,
    &extended_count_attr.attr,
    &skip_count_attr.attr,
    &nmissed_attr.attr,
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
        goto err;
    }

    rc = ima_rtmr_snapshot_initial(mr_file, digest_size, raw);
    if (rc)
        goto err;
    bin2hex(initial_hex, raw, digest_size);
    initial_hex[2 * digest_size] = '\0';
    kfree(raw);
    raw = NULL;

    ima_rtmr_kobj = kobject_create_and_add(KBUILD_MODNAME, kernel_kobj);
    if (!ima_rtmr_kobj) {
        rc = -ENOMEM;
        goto err;
    }

    rc = sysfs_create_group(ima_rtmr_kobj, &ima_rtmr_attr_group);
    if (rc) {
        kobject_put(ima_rtmr_kobj);
        ima_rtmr_kobj = NULL;
        goto err;
    }

    pr_info("sysfs ready (/sys/kernel/%s/), initial RTMR: %s\n",
            KBUILD_MODNAME,
            initial_hex);
    return 0;

err:
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
