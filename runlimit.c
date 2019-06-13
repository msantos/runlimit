/*
 * Copyright (c) 2019, Michael Santos <michael.santos@gmail.com>
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

#include <sys/file.h>

#include <signal.h>
#include <sys/types.h>

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
  OPT_WAIT = 4,
  OPT_KILL = 8,
};

static const struct option long_options[] = {
    {"intensity", required_argument, NULL, 'i'},
    {"period", required_argument, NULL, 'p'},
    {"kill", required_argument, NULL, 'k'},
    {"file", no_argument, NULL, 'f'},
    {"dryrun", no_argument, NULL, 'n'},
    {"print", no_argument, NULL, 'P'},
    {"wait", no_argument, NULL, 'w'},
    {"verbose", no_argument, NULL, 'v'},
    {"zero", no_argument, NULL, 'z'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

static int runlimit_open(const char *name);
static int runlimit_create(const char *name);
static void usage();

int main(int argc, char *argv[]) {
  int fd;
  runlimit_t *ap;
  const char *name = "supervise/runlimit";
  u_int32_t intensity = 1;
  int period = 1;
  int sig = SIGTERM;
  struct timespec now;
  int diff;
  int remaining;
  unsigned int sleepfor;
  int opt = 0;
  int verbose = 0;
  int ch;
  const char *errstr = NULL;
  int lock_flag = LOCK_EX | LOCK_NB;

  if (setvbuf(stdout, NULL, _IOLBF, 0) < 0)
    err(EXIT_FAILURE, "setvbuf");

  if (clock_gettime(RUNLIMIT_CLOCK_MONOTONIC, &now) < 0)
    err(EXIT_ERRNO, "clock_gettime(CLOCK_MONOTONIC)");

  while ((ch = getopt_long(argc, argv, "f:hi:k:nPp:vw", long_options, NULL)) !=
         -1) {
    switch (ch) {
    case 'f':
      name = optarg;
      break;

    case 'i':
      intensity = strtonum(optarg, 1, UINT_MAX, &errstr);
      if (errno)
        err(EXIT_FAILURE, "strtonum: %s: %s", optarg, errstr);
      break;

    case 'k':
      opt |= OPT_KILL;
      sig = strtonum(optarg, 0, NSIG - 1, &errstr);
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

    case 'w':
      opt |= OPT_WAIT;
      lock_flag &= ~LOCK_NB;
      break;

    case 'h':
    default:
      usage();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc == 0)
    usage();

  fd = runlimit_open(name);

  if (fd < 0)
    err(EXIT_ERRNO, "runlimit_open: %s", name);

  if (!(opt & OPT_DRYRUN)) {
    if (flock(fd, lock_flag) < 0) {
      switch (errno) {
      case EWOULDBLOCK:
        VERBOSE(1, "error: flock: %s\n", strerror(errno));
        exit(EXIT_ERRNO);
      default:
        err(EXIT_ERRNO, "flock");
      }
    }
  }

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

  remaining = period <= diff ? 0 : period - diff;

  if (opt & OPT_PRINT) {
    (void)printf("%d\n", remaining);
  }

  if (remaining == 0) {
    if (opt & OPT_DRYRUN)
      exit(0);

    ap->intensity = 0;
    ap->now.tv_sec = now.tv_sec;
    ap->now.tv_nsec = 0;
  } else if (ap->intensity >= intensity) {
    VERBOSE(1, "error: threshold: %u/%u (%ds)\n", ap->intensity, intensity,
            period);

    if (opt & OPT_KILL) {
      if (kill(0, sig) < 0)
        err(EXIT_ERRNO, "kill");
    }

    if (!(opt & OPT_WAIT))
      exit(111);

    sleepfor = remaining;
    while (sleepfor > 0)
      sleepfor = sleep(sleepfor);

    if (opt & OPT_DRYRUN)
      exit(0);

    ap->intensity = 0;
    ap->now.tv_sec = now.tv_sec + remaining;
    ap->now.tv_nsec = 0;
  }

  if (opt & OPT_DRYRUN)
    exit(0);

  ap->intensity++;

  if (msync(ap, sizeof(runlimit_t), MS_SYNC | MS_INVALIDATE) < 0)
    err(EXIT_ERRNO, "msync");

  (void)execvp(argv[0], argv);

  exit(EXIT_ERRNO);
}

static int runlimit_open(const char *name) {
  int fd;
  struct stat buf = {0};
  int oerrno;

  fd = open(name, O_RDWR | O_CLOEXEC, 0);

  if (fd < 0) {
    switch (errno) {
    case ENOENT:
      return runlimit_create(name);
    default:
      return -1;
    }
  }

  if (fstat(fd, &buf) < 0) {
    oerrno = errno;
    (void)close(fd);
    errno = oerrno;
    return -1;
  }

  if (buf.st_size < sizeof(runlimit_t)) {
    (void)close(fd);
    (void)unlink(name);
    errno = EFAULT;
    return -1;
  }

  return fd;
}

static int runlimit_create(const char *name) {
  int fd;

  fd = open(name, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);

  if (fd < 0)
    return -1;

  if (ftruncate(fd, sizeof(runlimit_t)) < 0) {
    int oerrno = errno;
    (void)close(fd);
    (void)unlink(name);
    errno = oerrno;
    return -1;
  }

  return fd;
}

static void usage() {
  errx(EXIT_FAILURE, "[OPTION] <PATH>\n"
                     "version: %s\n\n"
                     "-i, --intensity <count>  number of restarts\n"
                     "-p, --period <seconds>   time period\n"
                     "-n, --dryrun             do nothing\n"
                     "-P, --print              print remaining time\n"
                     "-f, --file               save state in file\n"
                     "-v, --verbose            verbose mode\n",
       RUNLIMIT_VERSION);
}
