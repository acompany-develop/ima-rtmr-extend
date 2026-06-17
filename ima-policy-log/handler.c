// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * - parse kretprobe captures each rule string ima_parse_add_rule() saw.
 * - update kretprobe wakes the workqueue once the rules are committed.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "handler.h"

#include <linux/mm.h>
#include <linux/ptrace.h>
#include <linux/string.h>

#include "measure.h"

/*
 * ima_write_policy() caps each write at PAGE_SIZE-1 bytes and a single
 * ima_parse_add_rule() call consumes only the first '\n'-separated rule, so a
 * rule string never exceeds PAGE_SIZE. Size the capture buffer to that cap so
 * normal long rules fit instead of being truncated. struct parse_data backs
 * the kretprobe per-instance data (allocated from the slab via data_size, not
 * on the stack), so a page-sized buffer here is fine.
 */
struct parse_data {
    char rule[PAGE_SIZE];
};

static int parse_entry_handler(struct kretprobe_instance* ri, struct pt_regs* regs) {
    struct parse_data* d = (void*)ri->data;
    const char* rule = (const char*)regs_get_kernel_argument(regs, 0);

    d->rule[0] = '\0';
    if (rule && strscpy(d->rule, rule, sizeof(d->rule)) == -E2BIG) {
        /*
         * Fail-closed: the rule did not fit the capture buffer, so hashing
         * the truncated copy would diverge from the policy IMA committed.
         */
        d->rule[0] = '\0';
        ima_policy_log_invalidate();
    }
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
    /*
     * Fail-closed against missed parse-probe returns: if the parse kretprobe
     * dropped any instance since the last drain, a rule went uncaptured and
     * the accumulator no longer holds the complete policy. Sample nmissed and
     * invalidate before queuing so measure_work_fn() refuses to hash a
     * partial policy. ima_parse_add_rule() runs under ima_write_mutex and
     * ima_update_policy() under the same release path, so by the time this
     * fires every parse return for the load has already executed.
     */
    ima_policy_log_check_missed();

    /*
     * A single shared work_struct on an ordered workqueue means two policy
     * updates that land before measure_work_fn() runs coalesce into one
     * measurement. Each drain consumes the accumulator atomically, so the
     * surviving run still measures a complete, self-consistent policy; the
     * only effect is one fewer IMA log entry for back-to-back updates, which
     * is acceptable and not worth a per-load work item.
     */
    queue_work(ima_policy_log_wq, &ima_policy_log_work);
    return 0;
}

static int delete_pre_handler(struct kprobe* kp, struct pt_regs* regs) {
    /*
     * ima_delete_rules() runs only when a policy load was rejected (parse
     * failure or ima_check_policy() < 0); the kernel never calls
     * ima_update_policy() in that case. Discard the accumulated rules without
     * measuring so a rejected load cannot prepend to the next one.
     */
    ima_policy_log_reset();
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

struct kprobe ima_policy_log_delete_kprobe = {
    .pre_handler = delete_pre_handler,
    .symbol_name = "ima_delete_rules",
};
