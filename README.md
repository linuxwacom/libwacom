# Overview
libwacom is a library to identify Wacom tablets and their model-specific features. It provides easy access to information such as "is this a built-in on-screen tablet", "what is the size of this model", etc.

This functionality is currently used by e.g. GNOME to map built-in tablets to the correct screen.

# Debugging libwacom with uinput devices
libwacom by default will not recognise uinput devices. To debug and test, a physical device must be connected.

Custom udev rules are provided to help debug uinput device. Run `generate-udev-rules --with-uinput-rules` to generate these rules and apply them locally. Devices will then be tagged as required and can be debugged.

## Some limitations:
* For these rules to work, the device must be listed in the database
* libwacom will check UINPUT_* properties on the uinput device, if they do
  not get applied, the device will not be visible

**DO NOT USE THESE UINPUT RULES unless you are debugging with uinput devices.**
Remove the rules once debugging is done.

# Adding tablet descriptions to libwacom
A common indicator that a device is not supported by libwacom is that it works normally in a GNOME session, but the device is not correctly mapped to the screen.

Use the libwacom-list-local-devices tool to list all local devices recognized by libwacom. If your device is not listed, but it is available as an event device in the kernel (see /proc/bus/input/devices) and in the X session (see xinput list), the device is missing from libwacom's database.

## To add support for a new tablet to libwacom:
1. Create a new tablet definition file. See data/wacom.example in the source for a guideline on how to add a new tablet. For an installed version of libwacom, see the existing data files (e.g. /usr/share/libwacom/cintiq-13hd.tablet)
2. A new tablet description is enabled by adding and installing a new file with a .tablet suffix. Once installed the tablet is part of libwacom's database, no rebuild is neccessary
3. The tablet is then available through libwacom-list-local-devices

**The new device should also be added to the udev rule to ensure all required properties are set**
* ***When building from source*** generate an update ruleset with tools/generate-udev-rules after adding the tablet descripton to
* ***When updating an installed version of libwacom***, add it manually to the existing ruleset (/lib/udev/rules.d/65-libwacom.rules)