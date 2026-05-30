// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Kernel-symbol resolution helper (kallsyms_lookup_name is unexported).
 */

#include "utils.h"

#include <linux/kprobes.h>

unsigned long __init ima_policy_log_ksym_lookup(const char* name) {
    static unsigned long (*lookup_fn)(const char*);

    if (!lookup_fn) {
        struct kprobe kp = {.symbol_name = "kallsyms_lookup_name"};

        if (register_kprobe(&kp) < 0)
            return 0;
        lookup_fn = (void*)kp.addr;
        unregister_kprobe(&kp);
    }
    return lookup_fn(name);
}
