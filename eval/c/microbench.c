// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Acompany Co., Ltd.
 *
 * Microbenchmark for syscall latency. Measures fork+exec, openat, and
 * read of a small file using rdtscp, emitting one TSC-cycle value per
 * iteration to stdout. Post-processing (TSC -> ns) is done in
 * eval/python/microbench-stats.py once the TSC frequency is known.
 *
 * Build: gcc -O2 -Wall -o microbench microbench.c
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static inline unsigned long long rdtscp(void) {
    unsigned int aux;
    unsigned int lo, hi;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return ((unsigned long long)hi << 32) | lo;
}

static unsigned long long bench_execve(unsigned long iters, const char *exe) {
    unsigned long long acc_min = ~0ULL;
    for (unsigned long i = 0; i < iters; i++) {
        unsigned long long t0 = rdtscp();
        pid_t pid = fork();
        if (pid == 0) {
            execl(exe, exe, (char *)NULL);
            _exit(127);
        }
        int wstatus;
        waitpid(pid, &wstatus, 0);
        unsigned long long t1 = rdtscp();
        unsigned long long d = t1 - t0;
        if (d < acc_min) acc_min = d;
        printf("execve %llu\n", d);
    }
    return acc_min;
}

static unsigned long long bench_openat(unsigned long iters, const char *path) {
    int dirfd = AT_FDCWD;
    for (unsigned long i = 0; i < iters; i++) {
        unsigned long long t0 = rdtscp();
        int fd = openat(dirfd, path, O_RDONLY);
        unsigned long long t1 = rdtscp();
        if (fd >= 0) close(fd);
        printf("openat %llu\n", t1 - t0);
    }
    return 0;
}

static unsigned long long bench_read(unsigned long iters, const char *path) {
    char buf[4096];
    for (unsigned long i = 0; i < iters; i++) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return 0;
        }
        unsigned long long t0 = rdtscp();
        ssize_t n = read(fd, buf, sizeof(buf));
        unsigned long long t1 = rdtscp();
        close(fd);
        if (n < 0) {
            perror("read");
            return 0;
        }
        printf("read %llu\n", t1 - t0);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
                "usage: %s {execve|openat|read} <iters> [path]\n"
                "  execve <iters>             - fork+execl /bin/true\n"
                "  openat <iters> <path>      - openat(AT_FDCWD, path)\n"
                "  read   <iters> <path>      - read 4 KiB from path\n",
                argv[0]);
        return 2;
    }
    const char *kind = argv[1];
    unsigned long iters = strtoul(argv[2], NULL, 10);
    if (!iters) return 2;

    if (!strcmp(kind, "execve")) {
        bench_execve(iters, argc >= 4 ? argv[3] : "/bin/true");
    } else if (!strcmp(kind, "openat")) {
        if (argc < 4) {
            fprintf(stderr, "openat requires path\n");
            return 2;
        }
        bench_openat(iters, argv[3]);
    } else if (!strcmp(kind, "read")) {
        if (argc < 4) {
            fprintf(stderr, "read requires path\n");
            return 2;
        }
        bench_read(iters, argv[3]);
    } else {
        fprintf(stderr, "unknown bench: %s\n", kind);
        return 2;
    }
    return 0;
}
