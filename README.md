runlimit: restart intensity limits for supervised Unix processes
================================================================

runlimit is a POC for implementing restart intensity limits for possibly
unrelated processes running under a supervisor such as daemontools'
svscan(8).

Usage
-----

    runlimit [*options*] *path*

Example
-------

    # 5 runs in 60 seconds

    # use shmem
    runlimit --intensity=5 --period=60 /state-${USER}

    # use a file
    mkdir -p /tmp/${USER}
    runlimit --intensity=5 --period=60 --file /tmp/${USER}/state


### Freeradius Exec Authenticator

~~~ shell
#!/bin/sh

# 6 login failures in 30 minutes
runlimit -i 6 -p 1800 "/runlimit-${USER_NAME}"
case $? in
  111) echo "Authentication failure limit reached"
       exit 6
       ;;
  0) ;;
  *) echo "Internal error: $?"
     exit 2
     ;;
esac
[ "$USER_NAME" = "username" ]
[ "$USER_PASSWORD" = "this-is-not-a-real-password" ]
runlimit -z ${USER_NAME}
~~~

Options
-------

-i, --intensity *count*
:   number of restarts

-p, --period *seconds*
:   time period in seconds

-n, --dryrun
:   do nothing

-P, --print
:   print remaining time

-z, --zero
:   zero state

-f, --file
:   save state in file

-v, --verbose
:   verbose mode
