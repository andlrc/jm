#!/bin/sh

# Expect to fail
exp="json_merger: expected ')' instead of EOF at 1:0"
if out=$(cat - << EOF | ../json_merger 2>&1
{"test": 1
EOF
)
then
	>&2 printf "%s Failed!\nExpected: \$? != 0\n" "$0"
else
	if test "$out" = "$exp"
	then
		printf "%s: Test passed\n" "$0"
	else
		>&2 printf "%s: Failed!\nExpected: %s\nGot: %s\n" "$0"
		exit 1
	fi
fi
