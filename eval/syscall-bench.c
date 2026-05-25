// SPDX-License-Identifier: GPL-2.0-only
//
// rdtscp-based syscall microbenchmark. Emits "<kind> <cycles>" per iteration.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static inline unsigned long long rdtscp(void) {
    unsigned int aux, lo, hi;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return ((unsigned long long)hi << 32) | lo;
}

static void run_execve(unsigned long n, const char* exe) {
    for (unsigned long i = 0; i < n; i++) {
        unsigned long long t0 = rdtscp();
        pid_t pid = fork();
        if (pid == 0) {
            execl(exe, exe, (char*)NULL);
            _exit(127);
        }
        waitpid(pid, NULL, 0);
        printf("execve %llu\n", rdtscp() - t0);
    }
}

static void run_openat(unsigned long n, const char* path) {
    for (unsigned long i = 0; i < n; i++) {
        unsigned long long t0 = rdtscp();
        int fd = openat(AT_FDCWD, path, O_RDONLY);
        unsigned long long t1 = rdtscp();
        if (fd >= 0) close(fd);
        printf("openat %llu\n", t1 - t0);
    }
}

static void run_read(unsigned long n, const char* path) {
    char buf[4096];
    for (unsigned long i = 0; i < n; i++) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return;
        }
        unsigned long long t0 = rdtscp();
        read(fd, buf, sizeof(buf));
        unsigned long long t1 = rdtscp();
        close(fd);
        printf("read %llu\n", t1 - t0);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s {execve|openat|read} <iters> [path]\n", argv[0]);
        return 2;
    }
    unsigned long n = strtoul(argv[2], NULL, 10);
    if (!n) return 2;
    if (!strcmp(argv[1], "execve")) run_execve(n, argc >= 4 ? argv[3] : "/bin/true");
    else if (!strcmp(argv[1], "openat")) {
        if (argc < 4) return 2;
        run_openat(n, argv[3]);
    } else if (!strcmp(argv[1], "read")) {
        if (argc < 4) return 2;
        run_read(n, argv[3]);
    } else return 2;
    return 0;
}
