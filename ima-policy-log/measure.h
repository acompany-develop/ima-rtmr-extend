/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_POLICY_LOG_MEASURE_H
#define _IMA_POLICY_LOG_MEASURE_H

#include <linux/workqueue.h>

extern struct workqueue_struct* ima_policy_log_wq;
extern struct work_struct ima_policy_log_work;

int __init ima_policy_log_measure_init(void);
void ima_policy_log_measure_exit(void);

/* Append one rule string to the accumulator (atomic-context safe). */
void ima_policy_log_accumulate(const char* rule);

/* Mark the in-flight accumulator as invalid so the next drain refuses to
 * emit a measurement. Sticky until the accumulator is reset (atomic-context
 * safe). Used by the fail-closed paths (truncation, overflow, missed probe
 * returns, scratch-alloc failure). */
void ima_policy_log_invalidate(void);

/* Discard the in-flight accumulator without measuring. Called from the
 * ima_delete_rules() abandon hook so a rejected policy load does not
 * contaminate the next one (atomic-context safe). */
void ima_policy_log_reset(void);

/* Fail-closed if the parse kretprobe dropped any return since the load began:
 * a missed return means a rule went uncaptured. Called from the update hook
 * before the measurement is queued (atomic-context safe). */
void ima_policy_log_check_missed(void);

#endif /* _IMA_POLICY_LOG_MEASURE_H */
