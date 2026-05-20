// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Auto-detection of writable RTMR sysfs attributes.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "detect.h"

#include <linux/err.h>
#include <linux/fs.h>

/* Add new entries here as more CC architectures gain tsm-mr support */
static const char* const candidate_paths[] = {
    "/sys/class/misc/tdx_guest/measurements/rtmr2:sha384",
};

struct file* ima_rtmr_detect(const char** used_path) {
    struct file* f;
    int i;

    for (i = 0; i < ARRAY_SIZE(candidate_paths); i++) {
        f = filp_open(candidate_paths[i], O_WRONLY, 0);
        if (!IS_ERR(f)) {
            *used_path = candidate_paths[i];
            return f;
        }
    }

    return ERR_PTR(-ENODEV);
}
