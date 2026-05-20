// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Hash algorithm helpers.
 */

#include "utils.h"

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
