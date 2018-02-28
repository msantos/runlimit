#!/usr/bin/env bats

teardown() {
   rm -f "/dev/shm/runlimit-$UID-test"
   rm -f "runlimit-$UID-test"
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
  runlimit -f . -i 2 -p 10 test
  run runlimit -v -f . -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "0" ]
}

@test "runlimit: file: above threshold" {
  runlimit -f . -i 2 -p 10 test
  runlimit -f . -i 2 -p 10 test
  run runlimit -f . -v -i 2 -p 10 test
cat << EOF
$output
EOF
  [ "$status" == "111" ]
}
