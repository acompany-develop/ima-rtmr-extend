// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * kretprobe handlers for ima_add_template_entry().
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "handler.h"

#include <linux/ptrace.h>

#include "consts.h"
#include "extend.h"
#include "ima.h"

atomic_long_t ima_rtmr_drops;

struct ima_rtmr_probe_data {
    struct ima_template_entry* entry;
};

static int ima_rtmr_entry_handler(struct kretprobe_instance* ri,
                                  struct pt_regs* regs) {
    struct ima_rtmr_probe_data* data = (void*)ri->data;

    data->entry = (struct ima_template_entry*)
        regs_get_kernel_argument(regs, 0);
    return 0;
}

static int ima_rtmr_ret_handler(struct kretprobe_instance* ri,
                                struct pt_regs* regs) {
    struct ima_rtmr_probe_data* data = (void*)ri->data;
    struct ima_template_entry* entry = data->entry;
    struct extend_request req;

    if (regs_return_value(regs) != 0)
        return 0;

    if (!entry || !entry->template_desc)
        return 0;

    req.entry = entry;

    if (!ima_rtmr_fifo_in(&req)) {
        pr_warn_ratelimited("extend FIFO full, measurement dropped\n");
        atomic_long_inc(&ima_rtmr_drops);
    } else {
        queue_work(extend_wq, &extend_work);
    }

    return 0;
}

struct kretprobe ima_rtmr_kretprobe = {
    .handler = ima_rtmr_ret_handler,
    .entry_handler = ima_rtmr_entry_handler,
    .data_size = sizeof(struct ima_rtmr_probe_data),
    .maxactive = 0,
    .kp.symbol_name = "ima_add_template_entry",
};
