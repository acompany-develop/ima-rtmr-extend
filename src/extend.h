/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_RTMR_EXTEND_H
#define _IMA_RTMR_EXTEND_H

#include <linux/types.h>
#include <linux/workqueue.h>

struct extend_request;
struct file;

extern struct workqueue_struct* extend_wq;
extern struct work_struct extend_work;

void ima_rtmr_extend_init(struct file* mr_file, u16 alg_id, int digest_size, int num_banks);
int ima_rtmr_fifo_in(const struct extend_request* req);
void ima_rtmr_seq_skip(u64 seq);

#endif /* _IMA_RTMR_EXTEND_H */
