#!/usr/bin/env bats

teardown() {
  runlimit -z test
  runlimit -d . -z test
}

@test "runlimit: shmem: under threshold" {
  runlimit -i 2 -p 10 test
  run runlimit -v -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "0" ]
}

@test "runlimit: shmem: above threshold" {
  runlimit -i 2 -p 10 test
  runlimit -i 2 -p 10 test
  run runlimit -v -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "111" ]
}

@test "runlimit: file: under threshold" {
  runlimit -d . -i 2 -p 10 test
  run runlimit -v -d . -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "0" ]
}

@test "runlimit: file: above threshold" {
  runlimit -d . -i 2 -p 10 test
  runlimit -d . -i 2 -p 10 test
  run runlimit -d . -v -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "111" ]
}
