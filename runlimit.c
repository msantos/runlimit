/*
 * Copyright (c) 2018, Michael Santos <michael.santos@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>

#include <limits.h>
#include <sys/param.h>

#include "runlimit.h"

#define RUNLIMIT_VERSION "0.1.0"

#ifdef CLOCK_MONOTONIC_COARSE
#define RUNLIMIT_CLOCK_MONOTONIC CLOCK_MONOTONIC_COARSE
#else
#define RUNLIMIT_CLOCK_MONOTONIC CLOCK_MONOTONIC
#endif

#define VERBOSE(__n, ...)                                                      \
  do {                                                                         \
    if (verbose >= __n) {                                                      \
      (void)fprintf(stderr, __VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

#define EXIT_ERRNO (127 + errno)

extern char *__progname;

typedef struct {
  u_int32_t intensity;
  struct timespec now;
} runlimit_t;

enum {
  OPT_DRYRUN = 1,
  OPT_PRINT = 2,
  OPT_ZERO = 4,
};

static const struct option long_options[] = {
    {"intensity", required_argument, NULL, 'i'},
    {"period", required_argument, NULL, 'p'},
    {"file", no_argument, NULL, 'f'},
    {"dryrun", no_argument, NULL, 'n'},
    {"print", no_argument, NULL, 'P'},
    {"verbose", no_argument, NULL, 'v'},
    {"zero", no_argument, NULL, 'z'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

static int state_open(const char *name, int (*open)(const char *, int, mode_t),
                      int (*unlink)(const char *));

static int state_create(const char *name,
                        int (*open)(const char *, int, mode_t),
                        int (*unlink)(const char *));
static int check_state(int fd);
static void usage();

int file_open(const char *pathname, int flags, mode_t mode);

enum { RUNLIMIT_SHMEM, RUNLIMIT_FILE };

struct {
  int (*open)(const char *, int, mode_t);
  int (*unlink)(const char *);
} runlimit_fn[] = {
    {&shm_open, &shm_unlink}, {&file_open, &unlink}, {NULL, NULL}};

int main(int argc, char *argv[]) {
  int fd;
  runlimit_t *ap;
  const char *name = NULL;
  u_int32_t intensity = 1;
  int period = 1;
  struct timespec now;
  int diff;
  int remaining;
  int opt = 0;
  int verbose = 0;
  int type = RUNLIMIT_SHMEM;
  int ch;
  int rv = 0;
  const char *errstr = NULL;

  /* initialize local time before entering sandbox */
  if (clock_gettime(RUNLIMIT_CLOCK_MONOTONIC, &now) < 0)
    err(EXIT_ERRNO, "clock_gettime(CLOCK_MONOTONIC)");

  if (sandbox_init() < 0)
    err(3, "sandbox_init");

  while ((ch = getopt_long(argc, argv, "fhi:nPp:vz", long_options, NULL)) !=
         -1) {
    switch (ch) {
    case 'f':
      type = RUNLIMIT_FILE;
      break;

    case 'i':
      intensity = strtonum(optarg, 1, UINT_MAX, &errstr);
      if (errno)
        err(EXIT_FAILURE, "strtonum: %s: %s", optarg, errstr);
      break;

    case 'n':
      opt |= OPT_DRYRUN;
      break;

    case 'P':
      opt |= OPT_PRINT;
      break;

    case 'p':
      period = strtonum(optarg, 1, INT_MAX, &errstr);
      if (errno)
        err(EXIT_FAILURE, "strtonum: %s: %s", optarg, errstr);
      break;

    case 'v':
      verbose++;
      break;

    case 'z':
      opt |= OPT_ZERO;
      break;

    case 'h':
    default:
      usage();
    }
  }

  if (argc - optind == 0)
    usage();

  name = strdup(argv[optind]);

  if (name == NULL)
    err(EXIT_ERRNO, "strdup");

  fd = state_open(name, runlimit_fn[type].open, runlimit_fn[type].unlink);

  if (fd < 0)
    err(EXIT_ERRNO, "state_open: %s", name);

  if (sandbox_mmap() < 0)
    err(3, "sandbox_mmap");

  ap =
      mmap(NULL, sizeof(runlimit_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (ap == MAP_FAILED)
    err(EXIT_ERRNO, "mmap");

  diff = now.tv_sec - ap->now.tv_sec;
  if (diff < 0)
    diff = 0;

  VERBOSE(2, "now=%llu\n"
             "last=%llu\n"
             "diff=%u\n"
             "intensity=%u\n"
             "period=%d\n"
             "count=%u\n",
          (long long)now.tv_sec, (long long)ap->now.tv_sec, diff, intensity,
          period, ap->intensity);

  if (opt & OPT_ZERO) {
    ap->intensity = 0;
    ap->now.tv_sec = 0;
    ap->now.tv_nsec = 0;
    goto RUNLIMIT_SYNC;
  }

  remaining = period <= diff ? 0 : period - diff;

  if (opt & OPT_PRINT)
    (void)printf("%d\n", remaining);

  switch (remaining) {
  case 0:
    ap->intensity = 1;
    ap->now.tv_sec = now.tv_sec;
    ap->now.tv_nsec = 0;
    break;

  default:
    if (ap->intensity >= intensity) {
      VERBOSE(1, "error: threshold: %u/%u (%ds)\n", ap->intensity, intensity,
              period);
      rv = 111;
    } else {
      ap->intensity++;
    }
    break;
  }

RUNLIMIT_SYNC:
  if (msync(ap, sizeof(runlimit_t), MS_SYNC | MS_INVALIDATE) < 0)
    err(EXIT_ERRNO, "msync");

  return rv;
}

static int state_open(const char *name, int (*open)(const char *, int, mode_t),
                      int (*unlink)(const char *)) {
  int fd;

  fd = (*open)(name, O_RDWR, 0);

  if (fd > -1 && check_state(fd) > -1)
    return fd;

  switch (errno) {
  case EFAULT:
    if ((*unlink)(name) < 0)
      return -1;
    break;

  case ENOENT:
    return state_create(name, open, unlink);

  default:
    break;
  }

  return -1;
}

static int state_create(const char *name,
                        int (*open)(const char *, int, mode_t),
                        int (*unlink)(const char *)) {
  int fd;

  fd = (*open)(name, O_RDWR | O_CREAT | O_EXCL, 0600);

  if (fd < 0)
    return -1;

  if (ftruncate(fd, sizeof(runlimit_t)) < 0) {
    int oerrno = errno;
    (void)close(fd);
    (void)(*unlink)(name);
    errno = oerrno;
    return -1;
  }

  return fd;
}

static int check_state(int fd) {
  struct stat buf = {0};
  int oerrno;

  if (fstat(fd, &buf) < 0)
    goto RUNLIMIT_ERR;

  if (buf.st_size < sizeof(runlimit_t)) {
    errno = EFAULT;
    goto RUNLIMIT_ERR;
  }

  return fd;

RUNLIMIT_ERR:
  oerrno = errno;
  (void)close(fd);
  errno = oerrno;

  return -1;
}

int file_open(const char *pathname, int flags, mode_t mode) {
  return open(pathname, flags, mode);
}

static void usage() {
  errx(EXIT_FAILURE, "[OPTION] <PATH>\n"
                     "version: %s (using %s sandbox)\n\n"
                     "-i, --intensity <count>  number of restarts\n"
                     "-p, --period <seconds>   time period\n"
                     "-n, --dryrun             do nothing\n"
                     "-P, --print              print remaining time\n"
                     "-z, --zero               zero state\n"
                     "-f, --file               save state in file\n"
                     "-v, --verbose            verbose mode\n",
       RUNLIMIT_VERSION, RUNLIMIT_SANDBOX);
}
