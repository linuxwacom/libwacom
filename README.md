# Overview

libwacom is a library to identify Wacom tablets and their model-specific
features. It provides easy access to information such as "is this a built-in
on-screen tablet", "what is the size of this model", etc.

This functionality is currently used by e.g. GNOME to map built-in tablets to
the correct screen.

# Adding tablet descriptions to libwacom

A common indicator that a device is not supported by libwacom is that it works
normally in a GNOME session, but the device is not correctly mapped to the
screen.

Use the `libwacom-list-local-devices` tool to list all local devices recognized
by libwacom. If your device is not listed, but it is available as an event
device in the kernel (see `/proc/bus/input/devices`) and in the X session (see
`xinput list`), the device is missing from libwacom's database.
Use the `libwacom-list-devices` tool to list all known devices and verify
the tablet is not in that list.

## To add support for a new tablet to libwacom git:

1. Create a new tablet definition file. See `data/wacom.example` in the source
   for a guideline on how to add a new tablet. For an installed version of
   libwacom, see the existing data files (e.g.
   `/usr/share/libwacom/cintiq-13hd.tablet`)
2. A new tablet description is enabled by adding and installing a new file with
   a `.tablet` suffix. Once installed the tablet is part of libwacom's
   database, no rebuild is neccessary
3. The tablet is then available through `libwacom-list-local-devices`

You must update udev after installing the file, see below.

## To add support for a tablet to an older libwacom

If the system-provided libwacom does not include a `.tablet` file, it is
possible to "backport" that `.tablet` file to the system-provided libwacom.
Simply copy the `.tablet` file from the upstream git tree into the local
directory `/etc/libwacom/`. Create that directory if necessary.

For versions of libwacom older than 1.9, the file should be copied to
`/usr/share/libwacom`. It may be overwritten on updates.

You must update udev after installing the file, see below.

## Updating udev's hwdb

The new device must be added to the udev hwdb to ensure all required udev
properties are set. Without a hwdb entry, the device may not be detected as
tablet and may not work correctly.

### When building from source

Generate an updated hwdb with `tools/generate-hwdb.py` after adding the
tablet description to the `data/` directory. This is done automatically during
the build, look for the `65-libwacom.hwdb` file in the build tree.
This file is installed as part of `ninja install` or `make install`. Run the following
command to activate the new hwdb set:
```
$ sudo systemd-hwdb update
```
Now disconnect and reconnect the device and it should be detected by libwacom.

### When adding files to an installed version of libwacom

After installing the `.tablet` file in `/etc/libwacom/`, run
the [`generate-hwdb.py`](https://github.com/linuxwacom/libwacom/blob/master/tools/generate-hwdb.py) tool:
This tool can be run from the source tree.

```
$ generate-hwdb.py /etc/libwacom > 66-libwacom-local.hwdb
$ sudo cp 66-libwacom-local.hwdb /etc/udev/hwdb.d/
$ sudo systemd-hwdb update
```

Now disconnect and reconnect the device and it should be detected by libwacom.
