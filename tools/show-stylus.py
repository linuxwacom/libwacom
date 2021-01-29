#!/usr/bin/env python3
#
# Permission to use, copy, modify, distribute, and sell this software
# and its documentation for any purpose is hereby granted without
# fee, provided that the above copyright notice appear in all copies
# and that both that copyright notice and this permission notice
# appear in supporting documentation, and that the name of Red Hat
# not be used in advertising or publicity pertaining to distribution
# of the software without specific, written prior permission.  Red
# Hat makes no representations about the suitability of this software
# for any purpose.  It is provided "as is" without express or implied
# warranty.
#
# THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
# INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
# NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

import argparse
import configparser
import sys
from pathlib import Path

try:
    import libevdev
    import pyudev
except ModuleNotFoundError as e:
    print("Error: {}".format(str(e)), file=sys.stderr)
    print(
        "One or more python modules are missing. Please install those "
        "modules and re-run this tool."
    )
    sys.exit(1)


class Ansi:
    clearline = "\x1B[K"

    @classmethod
    def up(cls, count):
        return f"\x1B[{count}A"

    @classmethod
    def down(cls, count):
        return f"\x1B[{count}B"

    @classmethod
    def right(cls, count):
        return f"\x1B[{count}C"

    @classmethod
    def left(cls, count):
        return f"\x1B[{count}D"


def die(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


def select_device():
    context = pyudev.Context()
    for device in context.list_devices(subsystem="input"):
        if device.get("ID_INPUT_TABLET", 0) and (device.device_node or "").startswith(
            "/dev/input/event"
        ):
            name = device.get("NAME", None)
            if not name:
                name = next(
                    (p.get("NAME") for p in device.ancestors if p.get("NAME")),
                    "unknown",
                )

            print("Using {}: {}".format(name or "unknown", device.device_node))
            return device.device_node

    die("Unable to find a tablet device.")


def record_events(ns):
    with open(ns.device_path, "rb") as fd:
        d = libevdev.Device(fd)
        if not d.absinfo[libevdev.EV_ABS.ABS_MISC]:
            die("Device only supports generic styli")

        tool_bits = set(
            c for c in libevdev.EV_KEY.codes if c.name.startswith("BTN_TOOL_")
        )
        styli = {}  # dict of (type, serial) = proximity_state
        current_type, current_serial = 0, 0
        in_prox = False
        dirty = False

        print("Please put tool in proximity")

        try:
            while True:
                for event in d.events():
                    if event.matches(libevdev.EV_ABS.ABS_MISC):
                        if event.value != 0:
                            current_type = event.value
                            dirty = True
                    elif event.matches(libevdev.EV_MSC.MSC_SERIAL):
                        if event.value != 0:
                            current_serial = event.value & 0xFFFFFFFF
                            dirty = True
                    elif event.code in tool_bits:
                        # print(f'Current prox: {event.value}')
                        in_prox = event.value != 0
                        dirty = True
                    elif event.matches(libevdev.EV_SYN.SYN_REPORT) and dirty:
                        dirty = False
                        print(
                            f"{Ansi.up(len(styli))}{Ansi.left(10000)}{Ansi.clearline}",
                            end="",
                        )
                        styli[(current_type, current_serial)] = in_prox
                        for s, prox in styli.items():
                            tid, serial = s
                            print(
                                f"Tool id {tid:#x} serial {serial:#x} in-proximity: {prox} "
                            )
        except KeyboardInterrupt:
            print("Terminating")

        return [s[0] for s in styli.keys()]


def load_data_files():
    lookup_paths = (
        ("./data/",),
        ("@DATADIR@", "@ETCDIR@"),
        ("/usr/share/libwacom/", "/etc/libwacom/"),
    )
    stylusfiles = []
    for paths in lookup_paths:
        stylusfiles = []
        for p in paths:
            files = list(Path(p).glob("*.stylus"))
            if files:
                stylusfiles += files

        if any(stylusfiles):
            break
    else:
        die("Unable to find a libwacom.stylus data file")

    print(f'Using stylus file(s): {", ".join([str(s) for s in stylusfiles])}')

    styli = {}

    for path in stylusfiles:
        config = configparser.ConfigParser()
        config.read(path)
        for stylus_id in config.sections():
            sid = int(stylus_id, 16)
            styli[sid] = config[stylus_id].get("Group", sid)

    return styli


def main():
    parser = argparse.ArgumentParser(description="Tool to show tablet stylus ids")
    parser.add_argument(
        "device_path", nargs="?", default=None, help="Path to the /dev/input/event node"
    )

    ns = parser.parse_args()
    if not ns.device_path:
        ns.device_path = select_device()

    all_styli = load_data_files()
    styli = record_events(ns)
    groups = []
    for sid in styli:
        if sid in all_styli:
            groups.append(all_styli[sid])
        else:
            print(f"Unknown stylus id {sid:#x}. New entry needed")
    print("Suggested line for .tablet file:")
    print(f"Styli={';'.join(set(groups))}")


if __name__ == "__main__":
    try:
        main()
    except PermissionError:
        die("Insufficient permissions, please run me as root")
