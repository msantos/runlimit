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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

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

#define VERBOSE(__n, ...) do { \
  if (verbose >= __n) { \
    (void)fprintf(stderr, __VA_ARGS__); \
  } \
} while (0)

#define EXIT_ERRNO (127+errno)

extern char *__progname;

typedef struct {
  u_int32_t intensity;
  struct timespec now;
} runlimit_t;

enum {
  OPT_DRYRUN = 1,
  OPT_WAIT   = 2,
  OPT_PRINT  = 4,
  OPT_FILE   = 8,
  OPT_ZERO   = 16,
};

static const struct option long_options[] =
{
  {"intensity",   required_argument,  NULL, 'i'},
  {"period",      required_argument,  NULL, 'p'},
  {"directory",   required_argument,  NULL, 'd'},
  {"dryrun",      no_argument,        NULL, 'n'},
  {"print",       no_argument,        NULL, 'P'},
  {"verbose",     no_argument,        NULL, 'v'},
  {"wait",        no_argument,        NULL, 'w'},
  {"zero",        no_argument,        NULL, 'z'},
  {"help",        no_argument,        NULL, 'h'},
  {NULL,          0,                  NULL, 0}
};

static int state_open(char *name, int opt);
static int shmem_open(char *name);
static int shmem_create(char *name);
static int file_open(char *name);
static int file_create(char *name);
static int check_state(int fd);
static void usage();

  int
main(int argc, char *argv[])
{
  int fd;
  runlimit_t *ap;
  char name[MAXPATHLEN] = {0}; /* NAME_MAX-1 */
  char *path = NULL;
  u_int32_t intensity = 1;
  int period = 1;
  struct timespec now;
  int diff;
  int remaining;
  int opt = 0;
  int verbose = 0;
  int ch;
  int n = 0;
  int rv = 0;
  const char *errstr = NULL;

  /* initialize local time before entering sandbox */
  if (clock_gettime(RUNLIMIT_CLOCK_MONOTONIC, &now) < 0)
    err(EXIT_ERRNO, "clock_gettime(CLOCK_MONOTONIC)");

  while ((ch = getopt_long(argc, argv, "d:hi:nPp:vwz",
          long_options, NULL)) != -1) {
    switch (ch) {
      case 'd':
        opt |= OPT_FILE;
        path = strdup(optarg);
        if (path == NULL)
          err(EXIT_FAILURE, "strdup");
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

      case 'w':
        opt |= OPT_WAIT;
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

  n = snprintf(name, sizeof(name), "%s/%s-%d-%s",
      path == NULL ? "" : path,
      __progname,
      getuid(),
      argv[optind]);

  if (n < 0 || n >= sizeof(name))
    usage();

  fd = state_open(name, opt);

  if (fd < 0)
    err(EXIT_ERRNO, "state_open");

  ap = mmap(NULL, sizeof(runlimit_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

  if (ap == MAP_FAILED)
    err(EXIT_ERRNO, "mmap");

  diff = now.tv_sec - ap->now.tv_sec;
  if (diff < 0)
    diff = 0;

  VERBOSE(1,
      "now=%lu\n"
      "last=%lu\n"
      "diff=%u\n"
      "intensity=%u\n"
      "period=%d\n"
      "count=%u\n",
      now.tv_sec,
      ap->now.tv_sec,
      diff,
      intensity,
      period,
      ap->intensity);

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
        VERBOSE(1, "error: threshold reached\n");
        if (opt & OPT_WAIT) {
          int sleepfor = remaining;
          while (sleepfor > 0)
            sleepfor = sleep(sleepfor);
          if (execvp(argv[0], argv) < 0)
            err(EXIT_ERRNO, "execve");
        }
        rv = 111;
      }
      else {
        ap->intensity++;
      }
      break;
  }

RUNLIMIT_SYNC:
  if (msync(ap, sizeof(runlimit_t), MS_SYNC|MS_INVALIDATE) < 0)
    err(EXIT_ERRNO, "msync");

  return rv;
}

  static int
state_open(char *name, int opt)
{
  return (opt & OPT_FILE) ? file_open(name) : shmem_open(name);
}

  static int
shmem_open(char *name)
{
  int fd;

  fd = shm_open(name, O_RDWR, 0);

  if (fd > -1)
    return check_state(fd);

  if (errno == ENOENT)
    return shmem_create(name);

  return -1;
}

  static int
file_open(char *name)
{
  int fd;

  fd = open(name, O_RDWR);

  if (fd > -1 && check_state(fd) > -1)
    return fd;

  switch (errno) {
    case EFAULT:
      if (unlink(name) < 0)
        return -1;
      /* fall through */
    case ENOENT:
      return file_create(name);

    default:
      break;
  }

  return -1;
}


  static int
shmem_create(char *name)
{
  int fd;

  fd = shm_open(name, O_RDWR|O_CREAT|O_EXCL, 0600);

  if (fd < 0)
    return -1;

  if (ftruncate(fd, sizeof(runlimit_t)) < 0) {
    int oerrno = errno;
    (void)close(fd);
    (void)shm_unlink(name);
    errno = oerrno;
    return -1;
  }

  return fd;
}

  static int
file_create(char *name)
{
  int fd;

  fd = open(name, O_RDWR|O_CREAT|O_EXCL, 0600);

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

  static int
check_state(int fd)
{
  struct stat buf = {0};
  int oerrno;

  if (fstat(fd, &buf) < 0)
    goto RUNLIMIT_ERR;

  if (buf.st_uid != getuid()) {
    errno = EPERM;
    goto RUNLIMIT_ERR;
  }

  if (buf.st_size != sizeof(runlimit_t)) {
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

  static void
usage()
{
  errx(EXIT_FAILURE, "[OPTION] <TAG>\n"
      "version: %s (using %s sandbox)\n\n"
      "-i, --intensity <count>  number of restarts\n"
      "-p, --period <seconds>   time period\n"
      "-w, --wait               wait until period expires\n"
      "-n, --dryrun             do nothing\n"
      "-P, --print              print remaining time\n"
      "-v, --verbose            verbose mode\n",
      RUNLIMIT_VERSION,
      RUNLIMIT_SANDBOX);
}
