#!/usr/bin/env python3
#
# This file is formatted with ruff format

import logging
import pytest

from . import WacomBuilder, WacomBustype, WacomDatabase, WacomDevice

logger = logging.getLogger(__name__)


def write_stylus_file(dir):
    from configparser import ConfigParser

    config = ConfigParser()
    config.optionxform = lambda option: option
    config["0xfffff"] = {
        "Name": "General Pen",
        "Group": "generic-with-eraser",
        "PairedStylusIds": "0xffffe;",
        "Buttons": "2",
        "Axes": "Tilt;Pressure;Distance;",
        "Type": "General",
    }

    config["0xffffe"] = {
        "Name": "General Pen Eraser",
        "Group": "generic-with-eraser",
        "PairedStylusIds": "0xfffff;",
        "EraserType": "Invert",
        "Buttons": "2",
        "Axes": "Tilt;Pressure;Distance;",
        "Type": "General",
    }

    with open(dir / "libwacom.stylus", "w") as fd:
        config.write(fd, space_around_delimiters=False)


def write_tablet_file(filename, devicename, matches):
    from configparser import ConfigParser

    config = ConfigParser()
    config.optionxform = lambda option: option
    config["Device"] = {
        "Name": devicename,
        "DeviceMatch": ";".join(matches),
        "Width": 9,
        "Height": 6,
        "IntegratedIn": "",
        "Class": "Bamboo",
        "Layout": "",
    }
    config["Features"] = {
        "Stylus": True,
        "Reversible": False,
    }

    with open(filename, "w") as fd:
        config.write(fd, space_around_delimiters=False)


@pytest.fixture()
def custom_datadir(tmp_path):
    write_stylus_file(tmp_path)
    write_tablet_file(tmp_path / "generic.tablet", "Generic", ["generic"])
    return tmp_path


def test_database_init(db):
    """Just a test to make sure it doesn't crash"""
    assert db is not None


def test_invalid_device(db):
    device = db.new_from_usbid(0x0, 0x0)
    assert device is None


def test_intuos4(db):
    device = db.new_from_usbid(0x056A, 0x00BC)
    assert device is not None

    assert device.name == "Wacom Intuos4 WL"
    assert device.get_class() == device.CLASS_INTUOS4
    assert device.vendor_id == 0x56A
    assert device.product_id == 0xBC
    assert device.bustype == WacomBustype.USB
    assert device.num_buttons == 9
    assert device.has_stylus()
    assert device.is_reversible()
    assert not device.has_touch()
    assert device.has_ring()
    assert not device.has_ring2()
    assert device.num_rings == 1
    assert not device.has_touchswitch()
    assert device.num_strips == 0
    assert device.num_dials == 0
    assert device.integration_flags == []
    assert device.width == 8
    assert device.height == 5

    matches = device.matches
    assert len(matches) == 2
    assert any(match.bustype == device.bustype for match in matches)
    assert any(match.vendor_id == device.vendor_id for match in matches)
    assert any(match.product_id == device.product_id for match in matches)


def test_intuos4_wl(db):
    device = db.new_from_usbid(0x056A, 0x00B9)
    assert device is not None

    assert WacomDevice.ButtonFlags.RING_MODESWITCH in device.button_flags("A")
    assert WacomDevice.ButtonFlags.OLED in device.button_flags("I")
    assert device.ring_num_modes == 4


def test_cintiq24hd(db):
    device = db.new_from_usbid(0x056A, 0x00F4)
    assert device is not None

    assert device.ring_num_modes == 3
    assert device.ring2_num_modes == 3


def test_cintiq21ux(db):
    device = db.new_from_usbid(0x056A, 0x00CC)
    assert device is not None

    assert device.num_strips == 2
    assert device.num_dials == 0


def test_wacf004(db):
    device = db.new_from_name("Wacom Serial Tablet WACf004")
    assert device is not None
    assert device.model_name is None
    assert device.integration_flags == [
        WacomDevice.IntegrationFlags.DISPLAY,
        WacomDevice.IntegrationFlags.SYSTEM,
    ]


def test_cintiq24hdt(db):
    device = db.new_from_usbid(0x056A, 0x00F8)
    assert device is not None

    match = device.paired_device
    assert match is not None
    assert match.vendor_id == 0x56A
    assert match.product_id == 0xF6
    assert match.bustype == device.BUSTYPE_USB


def test_cintiq13hd(db):
    libevdev = pytest.importorskip("libevdev")
    device = db.new_from_name("Wacom Cintiq 13HD")
    assert device is not None

    assert device.button_evdev_code("A") == libevdev.EV_KEY.BTN_0.value
    assert device.button_evdev_code("B") == libevdev.EV_KEY.BTN_1.value
    assert device.button_evdev_code("C") == libevdev.EV_KEY.BTN_2.value
    assert device.button_evdev_code("D") == libevdev.EV_KEY.BTN_3.value
    assert device.button_evdev_code("E") == libevdev.EV_KEY.BTN_4.value
    assert device.button_evdev_code("F") == libevdev.EV_KEY.BTN_5.value
    assert device.button_evdev_code("G") == libevdev.EV_KEY.BTN_6.value
    assert device.button_evdev_code("H") == libevdev.EV_KEY.BTN_7.value
    assert device.button_evdev_code("I") == libevdev.EV_KEY.BTN_8.value
    assert device.model_name == "DTK-1300"


def test_cintiqpro13(db):
    device = db.new_from_name("Wacom Cintiq Pro 13")
    assert device is not None
    assert device.num_keys == 5


def test_dell_canvas(db):
    device = db.new_from_name("Dell Canvas 27")
    assert device is not None
    assert device.integration_flags == [WacomDevice.IntegrationFlags.DISPLAY]


def test_bamboo_pen(db):
    libevdev = pytest.importorskip("libevdev")

    device = db.new_from_name("Wacom Bamboo Pen")
    assert device is not None
    assert device.button_evdev_code("A") == libevdev.EV_KEY.BTN_BACK.value
    assert device.button_evdev_code("B") == libevdev.EV_KEY.BTN_FORWARD.value
    assert device.button_evdev_code("C") == libevdev.EV_KEY.BTN_LEFT.value
    assert device.button_evdev_code("D") == libevdev.EV_KEY.BTN_RIGHT.value
    assert device.model_name == "MTE-450"


def test_isdv4_4800(db):
    device = db.new_from_usbid(0x56A, 0x4800)
    assert device is not None

    assert device.integration_flags == [
        WacomDevice.IntegrationFlags.DISPLAY,
        WacomDevice.IntegrationFlags.SYSTEM,
    ]
    assert device.model_name is None

    assert device.vendor_id == 0x56A
    assert device.product_id == 0x4800
    assert device.num_buttons == 0


@pytest.mark.parametrize(
    "bus,vid,pid",
    [
        (WacomBustype.USB, 0x56A, 0xBC),
        (WacomBustype.BLUETOOTH, 0x56A, 0xBD),
        (WacomBustype.UNKNOWN, 0x56A, 0xBD),
    ],
)
def test_new_from_builder_ids(db, bus, vid, pid):
    match = WacomBuilder.create(bus=bus, usbid=(vid, pid))
    device = db.new_from_builder(match)

    assert device is not None
    assert device.vendor_id == vid
    assert device.product_id == pid
    if bus != WacomBustype.UNKNOWN:
        assert device.bustype == bus
    else:
        # unkonwn bustype means "search for it" and
        # for this test that's bluetooth:
        # 0x56a/0bd is a bluetooth tablet
        assert device.bustype == WacomBustype.BLUETOOTH


def test_new_from_builder_empty(db):
    builder = WacomBuilder.create()
    device = db.new_from_builder(builder)
    assert device is None, f"Unexpected device: {device.name}"


def test_new_from_builder_device_name(db):
    builder = WacomBuilder.create(device_name="Wacom Bamboo Pen")
    device = db.new_from_builder(builder)
    assert device is not None

    # Fallback device with name override
    builder.device_name = "does not exist"
    device = db.new_from_builder(builder, fallback=WacomDatabase.Fallback.GENERIC)
    assert device is not None
    assert device.name == "does not exist"


def test_new_from_builder_uniq(db):
    builder = WacomBuilder.create(uniq="OEM02_T18e")
    device = db.new_from_builder(builder)
    assert device is not None
    assert device.name == "GAOMON S620"

    # uniq + match name triggers normal builder
    # but since vid/pid isn't set this does not find a match
    builder = WacomBuilder.create(uniq="OEM02_T18e")
    builder.match_name = "GAOMON Gaomon Tablet Pen"
    device = db.new_from_builder(builder)
    assert device is None

    # Once we set the vid/pid we get a match
    builder.usbid = (0x256C, 0x6D)
    device = db.new_from_builder(builder)
    assert device is not None
    assert device.name == "GAOMON S620"

    # uniq + name triggers normal builder but we don't have a code path
    # for that, it's a normal nameless match and
    # since vid/pid isn't set this does not find a match
    builder = WacomBuilder.create(uniq="OEM02_T18e")
    builder.device_name = "GAOMON S620"
    device = db.new_from_builder(builder)
    assert device is None


def test_exact_matches(custom_datadir):
    USBID = (0x1234, 0x5678)
    UNIQ = "uniqval"
    NAME = "nameval"

    # A device match with uniq but no name
    matches = ["usb|1234|5678||uniqval"]
    write_tablet_file(custom_datadir / "uniq.tablet", "UniqOnly", matches)

    # A device match with a name but no uniq
    matches = ["usb|1234|5678|nameval"]
    write_tablet_file(custom_datadir / "name.tablet", "NameOnly", matches)

    # A device match with both
    matches = ["usb|1234|5678|nameval|uniqval"]
    write_tablet_file(custom_datadir / "both.tablet", "Both", matches)

    db = WacomDatabase(path=custom_datadir)

    builder = WacomBuilder.create(usbid=USBID, uniq=UNIQ)
    device = db.new_from_builder(builder)
    assert device is not None
    assert device.name == "UniqOnly"

    builder = WacomBuilder.create(usbid=USBID, match_name=NAME)
    device = db.new_from_builder(builder)
    assert device is not None
    assert device.name == "NameOnly"

    builder = WacomBuilder.create(usbid=USBID, uniq=UNIQ, match_name=NAME)
    device = db.new_from_builder(builder)
    assert device is not None
    assert device.name == "Both"


def test_prefer_uniq_over_name(custom_datadir):
    USBID = (0x1234, 0x5678)
    UNIQ = "uniqval"
    NAME = "nameval"

    # A device match with uniq but no name
    matches = ["usb|1234|5678||uniqval"]
    write_tablet_file(custom_datadir / "uniq.tablet", "UniqOnly", matches)

    # A device match with a name but no uniq
    matches = ["usb|1234|5678|nameval"]
    write_tablet_file(custom_datadir / "name.tablet", "NameOnly", matches)

    db = WacomDatabase(path=custom_datadir)

    # name and uniq set in our match but we don't have a device with both.
    # Prefer the uniq match over the name match
    builder = WacomBuilder.create(usbid=USBID, uniq=UNIQ, match_name=NAME)
    device = db.new_from_builder(builder)
    assert device is not None
    assert device.name == "UniqOnly"

    # If we have a uniq in our match but none of the DeviceMatches
    # have that, fall back to name only
    builder = WacomBuilder.create(usbid=USBID, uniq="whatever", match_name=NAME)
    device = db.new_from_builder(builder)
    assert device is not None
    assert device.name == "NameOnly"

    # If we have a name in our match but none of the DeviceMatches
    # have that, fall back to uniq only
    builder = WacomBuilder.create(usbid=USBID, uniq=UNIQ, match_name="whatever")
    device = db.new_from_builder(builder)
    assert device is not None
    assert device.name == "UniqOnly"


def test_dont_ignore_exact_matches(custom_datadir):
    USBID = (0x1234, 0x5678)
    UNIQ = "uniqval"
    NAME = "nameval"

    # A device match with both
    matches = ["usb|1234|5678|nameval|uniqval"]
    write_tablet_file(custom_datadir / "both.tablet", "Both", matches)

    db = WacomDatabase(path=custom_datadir)

    builder = WacomBuilder.create(usbid=USBID, uniq=UNIQ, match_name=NAME)
    device = db.new_from_builder(builder)
    assert device is not None
    assert device.name == "Both"

    # Our DeviceMatch has both uniq and name set, so only match
    # when *both* match
    builder = WacomBuilder.create(usbid=USBID, uniq=UNIQ, match_name="whatever")
    device = db.new_from_builder(builder)
    assert device is None

    builder = WacomBuilder.create(usbid=USBID, uniq="whatever", match_name=NAME)
    device = db.new_from_builder(builder)
    assert device is None


# Emulates the behavior of new_from_path for an unknown device
@pytest.mark.parametrize("fallback", (WacomDatabase.Fallback.NONE, WacomDatabase.Fallback.GENERIC))
@pytest.mark.parametrize("bustype", (WacomBustype.USB, WacomBustype.BLUETOOTH))
def test_new_unknown_device_with_fallback(custom_datadir, fallback, bustype):
    USBID = (0x1234, 0x5678)
    NAME = "nameval"
    db = WacomDatabase(path=custom_datadir)
    builder = WacomBuilder.create(
        usbid=USBID, bus=bustype, match_name=NAME, device_name=NAME
    )

    device = db.new_from_builder(builder, fallback=fallback)
    if fallback:
        assert device is not None
        match = device.match
        assert match.decode("utf-8") == "generic"
        # Generic device always has 0, 0, 0 triple for bus/vid/pid
        assert device.bustype == WacomBustype.UNKNOWN
        assert device.vendor_id == 0
        assert device.product_id == 0
        assert device.name == NAME
    else:
        assert device is None
