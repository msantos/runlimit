.PHONY: all clean test

PROG=   runlimit
SRCS=   runlimit.c \
        strtonum.c \
        sandbox_null.c

UNAME_SYS := $(shell uname -s)
ifeq ($(UNAME_SYS), Linux)
    CFLAGS ?= -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-strong \
              -Wformat -Werror=format-security \
              -fno-strict-aliasing
		LDFLAGS ?= -lrt
	  RUNLIMIT_SANDBOX ?= seccomp
else ifeq ($(UNAME_SYS), OpenBSD)
    CFLAGS ?= -DHAVE_STRTONUM \
              -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-strong \
              -Wformat -Werror=format-security \
              -fno-strict-aliasing
	  RUNLIMIT_SANDBOX ?= pledge
else ifeq ($(UNAME_SYS), FreeBSD)
    CFLAGS ?= -DHAVE_STRTONUM \
              -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-strong \
              -Wformat -Werror=format-security \
              -fno-strict-aliasing
	  RUNLIMIT_SANDBOX ?= capsicum
endif

RM ?= rm

RUNLIMIT_SANDBOX ?= rlimit
RUNLIMIT_CFLAGS ?= -g -Wall -fwrapv

CFLAGS += $(RUNLIMIT_CFLAGS) \
          -DRUNLIMIT_SANDBOX=\"$(RUNLIMIT_SANDBOX)\" \
          -DRUNLIMIT_SANDBOX_$(RUNLIMIT_SANDBOX)

LDFLAGS += $(RUNLIMIT_LDFLAGS) -Wl,-z,relro,-z,now

all: $(PROG)

$(PROG):
	$(CC) $(CFLAGS) -o $(PROG) $(SRCS) $(LDFLAGS)

clean:
	-@$(RM) $(PROG)

test: $(PROG)
	@PATH=.:$(PATH) bats test
