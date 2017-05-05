#!/bin/sh

for file in i/*.json; do
	test -e "$file" || continue

	if ../json_merger -p "$file" \
         | diff - "o/${file##*/}" >&2
	then
		printf "%s passed test\n" "$file"
	else
		>&2 printf "%s failed test\n" "$file"
		exit 1
	fi
done
