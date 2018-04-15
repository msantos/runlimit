#!/usr/bin/env bats

teardown() {
  runlimit -z test
  runlimit -d . -z test
}

@test "runlimit: zero state" {
  runlimit -z test
  runlimit -i 100 -p 120 test
  runlimit -i 100 -p 120 test
  runlimit -i 100 -p 120 test
  runlimit -i 100 -p 120 test
  run runlimit -vv -i 100 -p 120 test
cat << EOF
$output
EOF
  [ "${lines[5]}" = "count=4" ]

  runlimit -z -i 100 -p 120 test
  run runlimit -vv -i 100 -p 120 test
cat << EOF
$output
EOF
  [ "${lines[5]}" = "count=0" ]
}

@test "runlimit: shmem: under threshold" {
  runlimit -i 2 -p 10 test
  run runlimit -vv -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "0" ]
}

@test "runlimit: shmem: above threshold" {
  runlimit -i 2 -p 10 test
  runlimit -i 2 -p 10 test
  run runlimit -vv -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "111" ]
}

@test "runlimit: file: under threshold" {
  runlimit -d . -i 2 -p 10 test
  run runlimit -vv -d . -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "0" ]
}

@test "runlimit: file: above threshold" {
  runlimit -d . -i 2 -p 10 test
  runlimit -d . -i 2 -p 10 test
  run runlimit -d . -vv -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "111" ]
}

@test "runlimit: file: open failure" {
  mkdir -p /tmp/runlimit-$UID-exists
  run runlimit -d /tmp -vv -i 2 -p 10 exists
cat << EOF
$output
EOF
  [ "$status" -gt 127 ]
}
