/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_POLICY_LOG_UTILS_H
#define _IMA_POLICY_LOG_UTILS_H

#include <linux/init.h>

/* Resolve a kernel symbol by name (kallsyms_lookup_name is unexported
 * since v5.7; this trampolines via a kprobe on the symbol itself).
 * Returns 0 if not found. Not concurrency-safe: callers must serialize. */
unsigned long __init ima_policy_log_ksym_lookup(const char* name);

#endif /* _IMA_POLICY_LOG_UTILS_H */
