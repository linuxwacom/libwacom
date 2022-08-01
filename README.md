# Overview

libwacom is a library to identify Wacom tablets and their model-specific
features. It provides easy access to information such as "is this a built-in
on-screen tablet", "what is the size of this model", etc.

**libwacom does not make a tablet work.** libwacom is merely a database with a
C library wrapper for *information* about a tablet. It has no effect on whether
that tablet works.

libwacom is currently used by GUI toolkits (GNOME, KDE, others?) to map
built-in tablets to the correct screen and by libinput to determine configuration
options such as the left-handed settings. SVG layout files are used to describe
tablet visually.

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
   database, no rebuild is necessary
3. The tablet is then available through `libwacom-list-local-devices`

You must update udev after installing the file, see below.

## To add support for a tablet to an installed libwacom

If the system-provided libwacom does not include a `.tablet` file, it is
possible to "backport" that `.tablet` file to the system-provided libwacom.

### libwacom 1.10 and newer

Copy the `.tablet` file into `/etc/libwacom` and run the
`libwacom-update-db` tool. Copy the tablet's `.svg` layout file
to `/etc/libwacom/layouts`.

```
$ cp my-tablet-file-from-upstream.tablet /etc/libwacom/
$ cp my-tablet-file-layout.svg /etc/libwacom/layouts/
$ libwacom-update-db /etc/libwacom
```

The tool will take care of updating udev, the hwdb, etc.

### libwacom 1.9 and earlier

For versions of libwacom <= 1.9, the file must be copied to
`/usr/share/libwacom`. It may be overwritten on updates.

You must update udev after installing the file. The simplest (and broadest)
way to do this is outlined below:

```
# create the hwdb file
$ cat <EOF > /etc/udev/hwdb.d/66-libwacom.hwdb
# WARNING: change "Your Device Name" to the actual name of your device
libwacom:name:Your Device Name*:input:*
 ID_INPUT=1
 ID_INPUT_TABLET=1
 ID_INPUT_JOYSTICK=0

libwacom:name:Your Device Name Pad:input:*
 ID_INPUT_TABLET_PAD=1

# Use this if the device is an external tablet
libwacom:name:Your Device Name Finger:input:*
 ID_INPUT_TOUCHPAD=1

# Use this if the device is a screen tablet
libwacom:name:Your Device Name Finger:input:*
 ID_INPUT_TOUCHSCREEN=1

EOF
$ systemd-hwdb --update
```
Now disconnect and reconnect the device and it should be detected by libwacom.

# API Documentation

The API documentation is available at https://linuxwacom.github.io/libwacom/
