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

#endif /* _IMA_POLICY_LOG_MEASURE_H */
