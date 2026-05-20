/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_RTMR_DETECT_H
#define _IMA_RTMR_DETECT_H

struct file;

struct file* ima_rtmr_detect(const char** used_path);

#endif /* _IMA_RTMR_DETECT_H */
