/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_RTMR_SYSFS_H
#define _IMA_RTMR_SYSFS_H

struct file;

int ima_rtmr_sysfs_init(struct file* mr_file, int digest_size);
void ima_rtmr_sysfs_exit(void);

#endif /* _IMA_RTMR_SYSFS_H */
