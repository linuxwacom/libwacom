# Overview

libwacom is a library to identify graphics tablets and their model-specific
features. It provides easy access to information such as "is this a built-in
on-screen tablet", "what is the size of this model", etc.

The name libwacom is historical - it was originally developed for Wacom devices
only but now supports any graphics tablet from any vendor.

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

See [this wiki page on adding a new device and how to test it](https://github.com/linuxwacom/libwacom/wiki/Adding-a-new-device).

# API Documentation

The API documentation is available at https://linuxwacom.github.io/libwacom/
