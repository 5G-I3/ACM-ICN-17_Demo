#!/bin/bash -ex

SERIAL=""
PORT=""

if [ $1 == "A" ]; then
	SERIAL=02000203C3604E173E9CB3EF
elif [ $1 == "B" ]; then
	SERIAL=02000203C3004E753EFCB38D
elif [ $1 == "C" ]; then
	SERIAL=02000203C31C4E173EE0B3EF
fi

PORT=$(make list-ttys | sed -n "s/.*serial: '${SERIAL}', tty(s): \(.*\)/\1/p")
if [ -n "${PORT}" ]; then
	make all flash term NODE_"${1}"=1 SERIAL="${SERIAL}" PORT=/dev/"${PORT}"
fi
