/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_POLICY_LOG_HANDLER_H
#define _IMA_POLICY_LOG_HANDLER_H

#include <linux/kprobes.h>

extern struct kretprobe ima_policy_log_parse_kretprobe;
extern struct kretprobe ima_policy_log_update_kretprobe;

#endif /* _IMA_POLICY_LOG_HANDLER_H */
