/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_RTMR_LOG_H
#define _IMA_RTMR_LOG_H

int ima_rtmr_log_init(void);
void ima_rtmr_log_advance(void);
unsigned long ima_rtmr_log_extended_count(void);

#endif /* _IMA_RTMR_LOG_H */
