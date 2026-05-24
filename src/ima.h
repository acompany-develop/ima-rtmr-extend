/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Mirror of struct definitions from security/integrity/ima/ima.h
 * (not exported via public headers). Layouts stable since v5.6.
 */

#ifndef _IMA_RTMR_IMA_H
#define _IMA_RTMR_IMA_H

#include <linux/kconfig.h>

#if !IS_ENABLED(CONFIG_TSM_MEASUREMENTS)
#error "ima_rtmr requires CONFIG_TSM_MEASUREMENTS=y (tsm-mr framework)"
#endif

#include <linux/list.h>
#include <linux/tpm.h>
#include <linux/types.h>

struct ima_field_data {
    u8* data;
    u32 len;
};

struct ima_template_field;

struct ima_template_desc {
    struct list_head list;
    char* name;
    char* fmt;
    int num_fields;
    const struct ima_template_field** fields;
};

struct ima_template_entry {
    int pcr;
    struct tpm_digest* digests;
    struct ima_template_desc* template_desc;
    u32 template_data_len;
    struct ima_field_data template_data[];
};

struct ima_queue_entry {
    struct hlist_node hnext;
    struct list_head later;
    struct ima_template_entry* entry;
};

#endif /* _IMA_RTMR_IMA_H */
