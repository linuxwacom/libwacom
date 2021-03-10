#!/bin/bash -x

# If called without arguments, just skip the rest
if [[ -z "$@" ]]; then
	exit
fi

# Don't care about these bits
echo 'path-exclude=/usr/share/doc/*' > /etc/dpkg/dpkg.cfg.d/99-exclude-cruft
echo 'path-exclude=/usr/share/locale/*' >> /etc/dpkg/dpkg.cfg.d/99-exclude-cruft
echo 'path-exclude=/usr/share/man/*' >> /etc/dpkg/dpkg.cfg.d/99-exclude-cruft

# Something about the postgres repo is weird - it randomly returns 404 for the
# Release file and breaks the build
mv /etc/apt/sources.list.d/pgdg.list /etc/apt/sources.list.d/pgdg.list.backup

apt-get update
apt-get install -yq --no-install-suggests --no-install-recommends $@

mv /etc/apt/sources.list.d/pgdg.list.backup /etc/apt/sources.list.d/pgdg.list
