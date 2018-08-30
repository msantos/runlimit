#!/usr/bin/env bats

teardown() {
  runlimit -z /runlimit-test
  runlimit -f -z runlimit-test
}

@test "runlimit: zero state" {
  runlimit -z /runlimit-test
  runlimit -i 100 -p 120 /runlimit-test
  runlimit -i 100 -p 120 /runlimit-test
  runlimit -i 100 -p 120 /runlimit-test
  runlimit -i 100 -p 120 /runlimit-test
  run runlimit -vv -i 100 -p 120 /runlimit-test
cat << EOF
$output
EOF
  [ "${lines[5]}" = "count=4" ]

  runlimit -z -i 100 -p 120 /runlimit-test
  run runlimit -vv -i 100 -p 120 /runlimit-test
cat << EOF
$output
EOF
  [ "${lines[5]}" = "count=0" ]
}

@test "runlimit: shmem: under threshold" {
  runlimit -i 2 -p 10 /runlimit-test
  run runlimit -vv -i 2 -p 10 /runlimit-test
cat << EOF
$output
EOF
  [ "$status" -eq 0 ]
}

@test "runlimit: shmem: above threshold" {
  runlimit -i 2 -p 10 /runlimit-test
  runlimit -i 2 -p 10 /runlimit-test
  run runlimit -vv -i 2 -p 10 /runlimit-test
cat << EOF
$output
EOF
  [ "$status" -eq 111 ]
}

@test "runlimit: file: under threshold" {
  runlimit -f -i 2 -p 10 runlimit-test
  run runlimit -vv -f -i 2 -p 10 runlimit-test
cat << EOF
$output
EOF
  [ "$status" -eq 0 ]
}

@test "runlimit: file: above threshold" {
  runlimit -f -i 2 -p 10 runlimit-test
  runlimit -f -i 2 -p 10 runlimit-test
  run runlimit -f -vv -i 2 -p 10 runlimit-test
cat << EOF
$output
EOF
  [ "$status" -eq 111 ]
}

@test "runlimit: file: open failure" {
  FILE="/tmp/runlimit-$UID-exists"
  mkdir -p "$FILE"
  run runlimit -f -vv -i 2 -p 10 "$FILE"
cat << EOF
$output
EOF
  [ "$status" -gt 127 ]
}

@test "runlimit: print remaining seconds" {
  run runlimit -P /runlimit-test 2>&1
cat << EOF
$output
EOF
  [ "$output" -eq 0 ]
  [ "$status" -eq 0 ]
}

@test "sandbox: write to /dev/null" {
  runlimit -P /runlimit-test > /dev/null
  [ "$?" -eq 0 ]
}
