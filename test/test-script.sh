#!/usr/bin/env bash -x

# parameter:

# don't know yet how to check the width of the output
# width="$( ${1} | head -1 | tr '\x1b' '\n' | wc -l )"
#width="$( ${1} | awk -F'\x1b' '{print NF; exit}')"
#test ${width} -eq $2 || exit 1
test 1

height=$(${1} | wc -l)
test ${height} -eq $3 || exit 2
