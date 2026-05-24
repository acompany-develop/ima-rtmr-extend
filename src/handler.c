// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * kretprobe on ima_add_template_entry: a successful return means the entry
 * is already on ima_measurements, so we just wake the workqueue.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "handler.h"

#include <linux/ptrace.h>

#include "extend.h"

static int ima_rtmr_ret_handler(struct kretprobe_instance* ri,
                                struct pt_regs* regs) {
    if (regs_return_value(regs) == 0)
        queue_work(extend_wq, &extend_work);
    return 0;
}

struct kretprobe ima_rtmr_kretprobe = {
    .handler = ima_rtmr_ret_handler,
    .kp.symbol_name = "ima_add_template_entry",
};
