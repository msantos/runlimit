.PHONY: all clean test

PROG=   runlimit
SRCS=   runlimit.c \
        strtonum.c \
        sandbox_capsicum.c \
        sandbox_null.c \
        sandbox_pledge.c \
        sandbox_rlimit.c \
        sandbox_seccomp.c

UNAME_SYS := $(shell uname -s)
ifeq ($(UNAME_SYS), Linux)
    CFLAGS ?= -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-strong \
              -Wformat -Werror=format-security \
              -pie -fPIE \
              -fno-strict-aliasing
		LDFLAGS ?= -lrt -Wl,-z,relro,-z,now -Wl,-z,noexecstack
	  RUNLIMIT_SANDBOX ?= seccomp
else ifeq ($(UNAME_SYS), OpenBSD)
    CFLAGS ?= -DHAVE_STRTONUM \
              -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-strong \
              -Wformat -Werror=format-security \
              -pie -fPIE \
              -fno-strict-aliasing
	  LDFLAGS ?= -Wl,-z,relro,-z,now -Wl,-z,noexecstack
	  RUNLIMIT_SANDBOX ?= pledge
else ifeq ($(UNAME_SYS), FreeBSD)
    CFLAGS ?= -DHAVE_STRTONUM \
              -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-strong \
              -Wformat -Werror=format-security \
              -pie -fPIE \
              -fno-strict-aliasing
	  LDFLAGS ?= -Wl,-z,relro,-z,now -Wl,-z,noexecstack
	  RUNLIMIT_SANDBOX ?= capsicum
else ifeq ($(UNAME_SYS), Darwin)
    CFLAGS ?= -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-strong \
              -Wformat -Werror=format-security \
              -pie -fPIE \
              -fno-strict-aliasing
endif

RM ?= rm

RUNLIMIT_SANDBOX ?= rlimit
RUNLIMIT_CFLAGS ?= -g -Wall -fwrapv -pedantic

CFLAGS += $(RUNLIMIT_CFLAGS) \
          -DRUNLIMIT_SANDBOX=\"$(RUNLIMIT_SANDBOX)\" \
          -DRUNLIMIT_SANDBOX_$(RUNLIMIT_SANDBOX)

LDFLAGS += $(RUNLIMIT_LDFLAGS)

all: $(PROG)

$(PROG):
	$(CC) $(CFLAGS) -o $(PROG) $(SRCS) $(LDFLAGS)

clean:
	-@$(RM) $(PROG)

test: $(PROG)
	@PATH=.:$(PATH) bats test
