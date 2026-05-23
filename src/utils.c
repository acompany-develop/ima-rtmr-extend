// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Hash algorithm helpers.
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

/* kallsyms_lookup_name has been unexported since v5.7; trampoline via a kprobe on its symbol. */
int ima_rtmr_read_extra_slots(int* out) {
    struct kprobe kp = {.symbol_name = "kallsyms_lookup_name"};
    unsigned long (*lookup_fn)(const char*);
    unsigned long addr;
    int rc;

    rc = register_kprobe(&kp);
    if (rc < 0)
        return rc;
    lookup_fn = (void*)kp.addr;
    unregister_kprobe(&kp);

    addr = lookup_fn("ima_extra_slots");
    if (!addr)
        return -ENOENT;

    /* ima_extra_slots is incremented at most twice in ima_init_crypto(); reject
     * values outside that range to guard against an upstream type change. */
    {
        int v = *(const int*)addr;

        if (v < 0 || v > 2)
            return -ERANGE;
        *out = v;
    }
    return 0;
}
