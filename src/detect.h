/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_RTMR_DETECT_H
#define _IMA_RTMR_DETECT_H

#include <linux/types.h>

struct file;

struct file* ima_rtmr_detect(const char** used_path);
int ima_rtmr_snapshot_initial(struct file* mr_file, int digest_size, u8* out);

#endif /* _IMA_RTMR_DETECT_H */
