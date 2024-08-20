#!/usr/bin/env python3
#
# This file is formatted with ruff format

from configparser import ConfigParser
from dataclasses import dataclass, field

import logging
import pytest

from . import WacomBuilder, WacomBustype, WacomDatabase, WacomDevice

logger = logging.getLogger(__name__)


@dataclass
class StylusEntry:
    id: str
    name: str
    group: str = "generic-with-eraser"
    paired_stylus_ids: list[str] = field(default_factory=list)
    buttons: int = 2
    axes: str = "Tilt;Pressure;Distance;"
    stylus_type: str = "General"
    eraser_type: str | None = None

    def add_to_config(self, config: ConfigParser):
        c = {
            "Name": self.name,
            "Group": self.group,
            "PairedStylusIds": ";".join(self.paired_stylus_ids),
            "Buttons": f"{self.buttons}",
            "Axes": self.axes,
            "Type": self.stylus_type,
        }
        if self.eraser_type is not None:
            c["EraserType"] = self.eraser_type
        config[self.id] = c

    @classmethod
    def generic_pen(cls) -> "StylusEntry":
        return cls(id="0xfffff", name="General Pen", paired_stylus_ids=["0xffffe"])

    @classmethod
    def generic_eraser(cls) -> "StylusEntry":
        return StylusEntry(
            id="0xffffe",
            name="General Pen Eraser",
            paired_stylus_ids=["0xfffff"],
            eraser_type="Invert",
        )


@dataclass
class StylusFile:
    entries: list[StylusEntry]

    @classmethod
    def default(cls) -> "StylusFile":
        return cls(
            entries=[
                StylusEntry.generic_pen(),
                StylusEntry.generic_eraser(),
            ]
        )

    def write_to_dir(self, dir, filename="libwacom.stylus"):
        config = ConfigParser()
        config.optionxform = lambda option: option

        for s in self.entries:
            s.add_to_config(config)

        with open(dir / filename, "w") as fd:
            config.write(fd, space_around_delimiters=False)


@dataclass
class TabletFile:
    name: str
    matches: list[str]
    width: int = 9
    height: int = 6
    integrated_in: str = ""
    klass: str = "Bamboo"
    layout: str = ""
    has_stylus: bool = True
    is_reversible: bool = False

    def write_to(self, filename):
        config = ConfigParser()
        config.optionxform = lambda option: option
        config["Device"] = {
            "Name": self.name,
            "DeviceMatch": ";".join(self.matches),
            "Width": self.width,
            "Height": self.height,
            "IntegratedIn": self.integrated_in,
            "Class": self.klass,
            "Layout": self.layout,
        }
        config["Features"] = {
            "Stylus": self.has_stylus,
            "Reversible": self.is_reversible,
        }
        with open(filename, "w") as fd:
            config.write(fd, space_around_delimiters=False)


@pytest.fixture()
def custom_datadir(tmp_path):
    StylusFile.default().write_to_dir(tmp_path)
    TabletFile(name="Generic", matches=["generic"]).write_to(
        tmp_path / "generic.tablet"
    )
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
    TabletFile(name="UniqOnly", matches=matches).write_to(
        custom_datadir / "uniq.tablet"
    )

    # A device match with a name but no uniq
    matches = ["usb|1234|5678|nameval"]
    TabletFile(name="NameOnly", matches=matches).write_to(
        custom_datadir / "name.tablet"
    )

    # A device match with both
    matches = ["usb|1234|5678|nameval|uniqval"]
    TabletFile(name="Both", matches=matches).write_to(custom_datadir / "both.tablet")

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
    TabletFile(name="UniqOnly", matches=matches).write_to(
        custom_datadir / "uniq.tablet"
    )

    # A device match with a name but no uniq
    matches = ["usb|1234|5678|nameval"]
    TabletFile(name="NameOnly", matches=matches).write_to(
        custom_datadir / "name.tablet"
    )

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
    TabletFile(name="Both", matches=matches).write_to(custom_datadir / "both.tablet")

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


# Emulates the behavior of new_from_path for an unknown device but without
# uinput devices
@pytest.mark.parametrize(
    "fallback", (WacomDatabase.Fallback.NONE, WacomDatabase.Fallback.GENERIC)
)
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


def create_uinput(name, vid, pid):
    libevdev = pytest.importorskip("libevdev")
    dev = libevdev.Device()
    dev.name = name
    dev.id = {"bustype": 0x3, "vendor": vid, "product": pid}
    dev.enable(
        libevdev.EV_ABS.ABS_X,
        libevdev.InputAbsInfo(minimum=0, maximum=10000, resolution=200),
    )
    dev.enable(
        libevdev.EV_ABS.ABS_Y,
        libevdev.InputAbsInfo(minimum=0, maximum=10000, resolution=200),
    )
    dev.enable(libevdev.EV_KEY.BTN_STYLUS)
    dev.enable(libevdev.EV_KEY.BTN_TOOL_PEN)
    try:
        return dev.create_uinput_device()
    except OSError as e:
        pytest.skip(f"Failed to create uinput device: {e}")


@pytest.mark.parametrize(
    "fallback", (WacomDatabase.Fallback.NONE, WacomDatabase.Fallback.GENERIC)
)
def test_new_from_path_known_device(db, fallback):
    name = "Wacom Intuos4 WL"
    vid = 0x056A
    pid = 0x00BC
    uinput = create_uinput(name, vid, pid)

    dev = db.new_from_path(
        uinput.devnode, fallback=fallback
    )  # fallback has no effect here
    assert dev is not None
    assert dev.name == name
    assert dev.vendor_id == vid
    assert dev.product_id == pid


@pytest.mark.parametrize(
    "fallback", (WacomDatabase.Fallback.NONE, WacomDatabase.Fallback.GENERIC)
)
def test_new_from_path_unknown_device(db, fallback):
    name = "Unknown device"
    vid = 0x1234
    pid = 0xABAC
    uinput = create_uinput(name, vid, pid)

    dev = db.new_from_path(
        uinput.devnode, fallback=fallback
    )  # fallback has no effect here
    if fallback == WacomDatabase.Fallback.NONE:
        assert dev is None
    else:
        assert dev is not None
        assert dev.name == name
        assert dev.vendor_id == 0
        assert dev.product_id == 0
