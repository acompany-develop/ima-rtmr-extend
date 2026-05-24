/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_RTMR_EXTEND_H
#define _IMA_RTMR_EXTEND_H

#include <linux/types.h>
#include <linux/workqueue.h>

struct file;
struct ima_template_entry;

extern struct workqueue_struct* extend_wq;
extern struct work_struct extend_work;

void ima_rtmr_extend_init(struct file* mr_file, u16 alg_id, int digest_size, int num_banks);
void ima_rtmr_do_extend(const struct ima_template_entry* entry);
bool ima_rtmr_extend_disabled(void);

#endif /* _IMA_RTMR_EXTEND_H */
