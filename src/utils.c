// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Hash algorithm table and kernel-symbol resolution helpers.
 */

#include "utils.h"

#include <linux/errno.h>
#include <linux/kprobes.h>
#include <linux/string.h>
#include <linux/tpm.h>

static const struct hash_alg_info supported_algs[] = {
    {"sha1", TPM_ALG_SHA1, 20},
    {"sha256", TPM_ALG_SHA256, 32},
    {"sha384", TPM_ALG_SHA384, 48},
    {"sha512", TPM_ALG_SHA512, 64},
};

const char* parse_hash_from_path(const char* path) {
    const char* colon = strrchr(path, ':');

    return colon ? colon + 1 : NULL;
}

const struct hash_alg_info* lookup_alg(const char* name) {
    int i;

    for (i = 0; i < ARRAY_SIZE(supported_algs); i++) {
        if (strcmp(supported_algs[i].name, name) == 0)
            return &supported_algs[i];
    }
    return NULL;
}

unsigned long __init ima_rtmr_ksym_lookup(const char* name) {
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

int __init ima_rtmr_read_extra_slots(int* out) {
    unsigned long addr = ima_rtmr_ksym_lookup("ima_extra_slots");
    int v;

    if (!addr)
        return -ENOENT;

    /* ima_extra_slots is incremented at most twice in ima_init_crypto();
     * reject values outside that range to guard against an upstream type
     * change that would silently corrupt the digest array bound. */
    v = *(const int*)addr;
    if (v < 0 || v > 2)
        return -ERANGE;
    *out = v;
    return 0;
}
