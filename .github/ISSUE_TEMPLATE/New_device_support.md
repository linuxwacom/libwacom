---
name: New device support request
about: Request to add a new device to libwacom's database

---

<!--
**NOTE:** libwacom is a descriptive library and has no effect on the
tablet events. Only file a bug for libwacom if the tablet works but does not
show up in GNOME's configuration panel.

If your tablet does not work, please follow the instructions here:
https://github.com/linuxwacom/libwacom/wiki/Troubleshooting
-->

**Device name**
<!-- e.g. Wacom Intuos Pro Small -->
> ...

**Device model identifier**
<!-- e.g. CTH-680 -->
> ...


<!--
**NOTE:** please look at the data/ directory for existing tablet device
files. These are text files. For most tablets, adding a new device is a
simple as copying+renaming an existing file and modifying the entries.

If you do so, please submit a Pull Request instead of this issue.

You will also need a new (alphabetically sorted) entry in the meson.build
file.
-->

**udevadm info output**
<!-- udevadm info /sys/class/input/eventXXX where XXX is the event node for
your device -->

