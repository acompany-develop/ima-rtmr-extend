/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
/*
 * Sequence number assignment for IMA measurement ordering.
 *
 * A kprobe on ima_add_digest_entry() assigns a monotonic sequence number
 * while ima_extend_list_mutex is held, giving each measurement a position
 * that exactly matches the IMA log order.
 */

#ifndef _IMA_RTMR_SEQ_H
#define _IMA_RTMR_SEQ_H

#include <linux/kprobes.h>
#include <linux/types.h>

struct ima_template_entry;

extern struct kprobe ima_rtmr_seq_kprobe;

/* Returns 0 if sequencing is disabled or the entry was not found. */
u64 ima_rtmr_seq_consume(const struct ima_template_entry* entry);

bool ima_rtmr_seq_enabled(void);
void __init ima_rtmr_seq_activate(void);

#endif /* _IMA_RTMR_SEQ_H */
