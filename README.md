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

    # use a file
    mkdir -p /tmp/${USER}
    runlimit --intensity=5 --period=60 --file=/tmp/${USER}/state -- ls -al

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

-f, --file *path*
:   state file

-v, --verbose
:   verbose mode

Exit Status
-----------

111
:     threshold reached

128
:     EPERM

139
:     ENOMEM
