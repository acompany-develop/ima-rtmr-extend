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

#include <crypto/hash_info.h>

static const struct hash_alg_info supported_algs[] = {
    {"sha1", TPM_ALG_SHA1, 20, HASH_ALGO_SHA1},
    {"sha256", TPM_ALG_SHA256, 32, HASH_ALGO_SHA256},
    {"sha384", TPM_ALG_SHA384, 48, HASH_ALGO_SHA384},
    {"sha512", TPM_ALG_SHA512, 64, HASH_ALGO_SHA512},
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

/* Read an int kernel symbol via kallsyms and bound-check it. The range guards
 * against an upstream type change silently feeding us a bogus value. */
static int __init read_ksym_int(const char* name, int lo, int hi, int* out) {
    unsigned long addr = ima_rtmr_ksym_lookup(name);
    int v;

    if (!addr)
        return -ENOENT;

    v = *(const int*)addr;
    if (v < lo || v > hi)
        return -ERANGE;
    *out = v;
    return 0;
}

int __init ima_rtmr_read_extra_slots(int* out) {
    /* ima_extra_slots is incremented at most twice in ima_init_crypto(). */
    return read_ksym_int("ima_extra_slots", 0, 2, out);
}

int __init ima_rtmr_read_hash_algo(int* out) {
    return read_ksym_int("ima_hash_algo", 0, HASH_ALGO__LAST - 1, out);
}

int __init ima_rtmr_read_hash_algo_idx(int* out) {
    /* Bounded loosely here; the caller rejects an index past the digest array. */
    return read_ksym_int("ima_hash_algo_idx", 0, 63, out);
}
