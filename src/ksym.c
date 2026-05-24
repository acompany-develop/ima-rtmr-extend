// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * kallsyms_lookup_name() is unexported since v5.7; trampoline via a kprobe
 * on the symbol itself to recover the address from out-of-tree code.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "ksym.h"

#include <linux/kprobes.h>

static unsigned long (*kallsyms_lookup_name_fn)(const char*);

unsigned long ima_rtmr_ksym_lookup(const char* name) {
    if (!kallsyms_lookup_name_fn) {
        struct kprobe kp = {.symbol_name = "kallsyms_lookup_name"};

        if (register_kprobe(&kp) < 0)
            return 0;
        kallsyms_lookup_name_fn = (void*)kp.addr;
        unregister_kprobe(&kp);
    }
    return kallsyms_lookup_name_fn(name);
}
