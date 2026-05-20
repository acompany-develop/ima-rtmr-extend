/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */

#ifndef _IMA_RTMR_CONSTS_H
#define _IMA_RTMR_CONSTS_H

#define EXTEND_FIFO_SIZE 256 /* must be power of 2 */

struct ima_template_entry;

struct extend_request {
    /* IMA entries are never freed during normal operation */
    struct ima_template_entry* entry;
};

#endif /* _IMA_RTMR_CONSTS_H */
