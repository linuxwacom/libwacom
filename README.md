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

## To add support for a new tablet to libwacom git:

1. Create a new tablet definition file. See `data/wacom.example` in the source
   for a guideline on how to add a new tablet. For an installed version of
   libwacom, see the existing data files (e.g.
   `/usr/share/libwacom/cintiq-13hd.tablet`)
2. A new tablet description is enabled by adding and installing a new file with
   a `.tablet` suffix. Once installed the tablet is part of libwacom's
   database, no rebuild is neccessary
3. The tablet is then available through `libwacom-list-local-devices`

## Updating udev's hwdb

The new device must be added to the udev hwdb to ensure all required udev
properties are set. Without a hwdb entry, the device may not be detected as
tablet and may not work correctly.

### When building from source

Generate an updated hwdb with `tools/generate-hwdb.py` after adding the
tablet description to the `data/` directory. This is done automatically during
the build, look for the `65-libwacom.hwdb` file in the build tree.

### When updating an installed version of libwacom

Create a `/etc/udev/hwdb.d/99-libwacom-new-tablet.hwdb` file with the
following content:

```
libwacom:name:*:input:b0003v056Ap0084*
 ID_INPUT=1
 ID_INPUT_TABLET=1
 ID_INPUT_JOYSTICK=0

libwacom:name:* Finger:input:b0003v056Ap0084*:
 ID_INPUT_TOUCHPAD=1

libwacom:name:* Pad:input:b0003v056Ap0084*:
 ID_INPUT_TABLET_PAD=1
```

- Replace `0084` with your **uppercase** Product ID, see the device's entry in `/proc/bus/input/devices`
- Replace `056A` with your **uppercase** Vendor ID if the device is not from Wacom
- Replace the `b0003` with `b0005` if the device is connected via Bluetooth

Once the file is in place, run `sudo systemd-hwdb update` and disconnect +
reconnect the device.
