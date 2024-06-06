#!/bin/bash -x

# If called without arguments, just skip the rest
if [[ -z "$@" ]]; then
	exit
fi

FLAGS=""
if ls /usr/lib/python3.*/EXTERNALLY-MANAGED >/dev/null 2>&1; then
	FLAGS="--break-system-packages"
fi

python -m pip install --upgrade $FLAGS pip
python -m pip install --upgrade $FLAGS "$@"
