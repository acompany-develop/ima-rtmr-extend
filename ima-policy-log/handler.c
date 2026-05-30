// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * - parse kretprobe captures each rule string ima_parse_add_rule() saw.
 * - update kretprobe wakes the workqueue once the rules are committed.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "handler.h"

#include <linux/ptrace.h>
#include <linux/string.h>

#include "measure.h"

struct parse_data {
    char rule[512];
};

static int parse_entry_handler(struct kretprobe_instance* ri, struct pt_regs* regs) {
    struct parse_data* d = (void*)ri->data;
    const char* rule = (const char*)regs_get_kernel_argument(regs, 0);

    d->rule[0] = '\0';
    if (rule)
        strscpy(d->rule, rule, sizeof(d->rule));
    return 0;
}

static int parse_ret_handler(struct kretprobe_instance* ri, struct pt_regs* regs) {
    struct parse_data* d = (void*)ri->data;

    /* ima_parse_add_rule returns ssize_t: >=0 on success. */
    if ((ssize_t)regs_return_value(regs) >= 0 && d->rule[0])
        ima_policy_log_accumulate(d->rule);
    return 0;
}

static int update_ret_handler(struct kretprobe_instance* ri, struct pt_regs* regs) {
    queue_work(ima_policy_log_wq, &ima_policy_log_work);
    return 0;
}

struct kretprobe ima_policy_log_parse_kretprobe = {
    .handler = parse_ret_handler,
    .entry_handler = parse_entry_handler,
    .data_size = sizeof(struct parse_data),
    .kp.symbol_name = "ima_parse_add_rule",
};

struct kretprobe ima_policy_log_update_kretprobe = {
    .handler = update_ret_handler,
    .kp.symbol_name = "ima_update_policy",
};
