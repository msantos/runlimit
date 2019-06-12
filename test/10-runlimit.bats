#!/usr/bin/env bats

teardown() {
  rm -f runlimit-test
}

@test "runlimit: zero state" {
  rm -f runlimit-test
  runlimit -i 100 -p 120 -f runlimit-test -- true
  runlimit -i 100 -p 120 -f runlimit-test -- true
  runlimit -i 100 -p 120 -f runlimit-test -- true
  runlimit -i 100 -p 120 -f runlimit-test -- true
  run runlimit -vv -i 100 -p 120 -f runlimit-test -- true
cat << EOF
$output
EOF
  [ "${lines[5]}" = "count=4" ]

  rm -f runlimit-test
  run runlimit -vv -i 100 -p 120 -f runlimit-test -- true
cat << EOF
$output
EOF
  [ "${lines[5]}" = "count=0" ]
}

@test "runlimit: under threshold" {
  runlimit -i 2 -p 10 -f runlimit-test -- true
  run runlimit -vv -i 2 -p 10 -f runlimit-test -- true
cat << EOF
$output
EOF
  [ "$status" -eq 0 ]
}

@test "runlimit: above threshold" {
  runlimit -i 2 -p 10 -f runlimit-test -- true
  runlimit -i 2 -p 10 -f runlimit-test -- true
  run runlimit -vv -i 2 -p 10 -f runlimit-test -- true
cat << EOF
$output
EOF
  [ "$status" -eq 111 ]
}

@test "runlimit: open failure" {
  FILE="/tmp/runlimit-$UID-exists"
  mkdir -p "$FILE"
  run runlimit -vv -i 2 -p 10 -f "$FILE" -- true
cat << EOF
$output
EOF
  [ "$status" -gt 127 ]
}

@test "runlimit: invalid state" {
  > runlimit-test
  run runlimit -vv -i 2 -p 10 -f runlimit-test -- true
cat << EOF
$output
EOF
  [ "$status" -eq 141 ]
}

@test "runlimit: print remaining seconds" {
  run runlimit -n -P -f runlimit-test -- true 2>&1
cat << EOF
$output
EOF
  [ "$output" -eq 0 ]
  [ "$status" -eq 0 ]
}
