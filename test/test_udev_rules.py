#!/usr/bin/env python3
#
# This test will only work where /dev/uinput is available.
# This test will reload the hwdb and udev rules
#
# Execute via pytest, it will:
# - load all data files and extract the matches
# - create a uinput device for each match
# - check if that device has the udev properties set we expect

import configparser
import os
from pathlib import Path
import pytest
import logging
import sys
import subprocess
import shutil


@pytest.fixture(scope="session", autouse=True)
def systemd_reload():
    """Make sure our hwdb and udev rules are up-to-date"""

    try:
        hwdb = os.environ.get("LIBWACOM_HWDB_FILE")
        target = Path("/etc/udev/hwdb.d/99-libwacom-pytest.hwdb")
        if hwdb:
            shutil.copyfile(hwdb, target)
        else:
            import warnings

            warnings.warn("LIBWACOM_HWDB_FILE is not set, using already installed hwdb")

        subprocess.run(["systemd-hwdb", "update"], check=True)
        subprocess.run(["systemctl", "daemon-reload"], check=True)

        yield

        if hwdb:
            os.unlink(target)

        subprocess.run(["systemd-hwdb", "update"], check=True)
        subprocess.run(["systemctl", "daemon-reload"], check=True)

    except (FileNotFoundError, subprocess.CalledProcessError):
        # If any of the commands above are not found (most likely the system
        # simply does not use systemd), just skip.
        raise pytest.skip()


def pytest_generate_tests(metafunc):
    # for any function that takes a "tablet" argument return a Tablet object
    # filled with exactly one DeviceMatch from the list of all .tablet files
    # in the data dir. Where the tablet also has touch/buttons generate an
    # extra Finger or Pad device
    if "tablet" in metafunc.fixturenames:
        datadir = Path(os.getenv("MESON_SOURCE_ROOT") or ".") / "data"
        tablets = []
        for f in datadir.glob("*.tablet"):
            config = configparser.ConfigParser()
            config.read(f)
            name = config["Device"]["Name"]
            want_pad = config["Device"].get("Buttons", 0)
            want_finger = config["Features"].get("Touch") == "true"
            integrated_in = config["Device"].get("IntegratedIn", "").split(";")
            is_touchscreen = set(integrated_in) & set(["Display", "System"])

            for match in config["Device"]["DeviceMatch"].split(";"):
                if not match or match == "generic":
                    continue

                bus, vid, pid = match.split("|")[:3]  # skip the name part of the match
                if bus not in ["usb", "bluetooth"]:
                    continue

                try:
                    vid = int(vid, 16)
                    pid = int(pid, 16)
                except ValueError as e:
                    print(f"Invalid vid/pid in {match} in {f}", file=sys.stderr)
                    raise e

                if bus == "usb":
                    bus = 0x3
                elif bus == "bluetooth":
                    bus = 0x5

                class Tablet(object):
                    def __init__(self, name, bus, vid, pid, is_touchscreen=False):
                        self.name = name
                        self.bus = bus
                        self.vid = vid
                        self.pid = pid
                        self.is_touchscreen = is_touchscreen

                tablets.append(Tablet(name, bus, vid, pid))

                if want_pad:
                    tablets.append(Tablet(name + " Pad", bus, vid, pid))

                if want_finger:
                    tablets.append(
                        Tablet(name + " Finger", bus, vid, pid, is_touchscreen)
                    )

        # our tablets list now becomes the list of arguments passed to the
        # test functions taking a 'tablet' argument - one-by-one. So where
        # tablets contains 10 entries, our test function will be called 10
        # times.
        metafunc.parametrize("tablet", tablets, ids=[t.name for t in tablets])


@pytest.mark.skipif(sys.platform != "linux", reason="This test requires udev")
def test_hwdb_files(tablet):
    # Note: the name doesn't matter, all our hwdb files use either "*"
    # or "* Finger", etc.
    query = f"libwacom:name:{tablet.name or '.'}:input:b{tablet.bus:04X}v{tablet.vid:04X}p{tablet.pid:04X}"
    logging.debug(query)

    r = subprocess.run(
        ["systemd-hwdb", "query", query], check=True, capture_output=True
    )
    logging.debug(r.stdout.decode("utf-8"))
    props = {}
    for l in filter(lambda l: len(l) > 1, r.stdout.decode("utf-8").strip().split("\n")):
        print(l)
        k, v = l.split("=")
        props[k] = v

    assert "ID_INPUT" in props
    assert props["ID_INPUT"] == "1"

    assert "ID_INPUT_TABLET" in props
    assert props["ID_INPUT_TABLET"] == "1"

    if "ID_INPUT_JOYSTICK" not in props:
        assert props["ID_INPUT_JOYSTICK"] == "0"

    if "Finger" in tablet.name:
        if tablet.is_touchscreen:
            assert "ID_INPUT_TOUCHSCREEN" in props
        else:
            assert "ID_INPUT_TOUCHPAD" in props

    # For the Wacom Bamboo Pad we check for "Pad Pad" in the device name
    if "Pad" in tablet.name:
        if "Wacom Bamboo Pad" not in tablet.name or "Pad Pad" in tablet.name:
            assert "ID_INPUT_TABLET_PAD" in props
