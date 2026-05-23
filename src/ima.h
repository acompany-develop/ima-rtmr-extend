/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * IMA internal structure definitions.
 *
 * Copied from security/integrity/ima/ima.h which is not exported via
 * public kernel headers. Must be kept in sync with the target kernel.
 *
 * Last synced with: Linux 6.17
 * Structure layout has been stable since v5.6 (tpm_digest *digests).
 */

#ifndef _IMA_RTMR_IMA_H
#define _IMA_RTMR_IMA_H

#include <linux/kconfig.h>

#if !IS_ENABLED(CONFIG_TSM_MEASUREMENTS)
#error "ima_rtmr requires CONFIG_TSM_MEASUREMENTS=y (tsm-mr framework)"
#endif

#if IS_ENABLED(CONFIG_LTO_CLANG)
#error "ima_rtmr cannot be built against CONFIG_LTO_CLANG kernels: ima_add_digest_entry gets inlined and the sequencing kprobe cannot be registered."
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

#endif /* _IMA_RTMR_IMA_H */
