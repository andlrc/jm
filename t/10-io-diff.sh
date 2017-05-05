#!/bin/sh

for file in 10-i/*.json; do
	if ../json_merger -p "$file" \
         | diff - "10-o/${file##*/}" >&2
	then
		printf "%s: %s passed test\n" "$0" "$file"
	else
		>&2 printf "%s: %s failed test\n" "$0" "$file"
		exit 1
	fi
done
