/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026 Acompany Co., Ltd. */
#ifndef _IMA_RTMR_UTILS_H
#define _IMA_RTMR_UTILS_H

#include <linux/types.h>

struct hash_alg_info {
    const char* name;
    u16 alg_id;
    int digest_size;
};

/* Returns the substring after the last ':' in the sysfs path, or NULL. */
const char* parse_hash_from_path(const char* path);

const struct hash_alg_info* lookup_alg(const char* name);

/* Read the runtime value of ima_extra_slots. Returns 0 or negative errno. */
int ima_rtmr_read_extra_slots(int* out);

#endif /* _IMA_RTMR_UTILS_H */
