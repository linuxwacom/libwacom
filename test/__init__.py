#!/usr/bin/env python3
#
# This file is formatted with ruff format
#
# Run with pytest:
#    $ export LD_LIBRARY_PATH=$PWD/builddir/
#    $ export MESON_SOURCE_ROOT=$PWD   # optional, defaults to $PWD.
#    $ pytest -v --log-level=DEBUG
#
# Introduction
# ============
#
# This is a Python-based test suite making use of ctypes to test the
# libwacom.so C library.
#
# The main components are:
# - LibWacom: the Python class wrapping libwacom.so via ctypes.
#   This is a manually maintained mapping, any API additions/changes must
#   updated here.
# - WacomDevice, WacomDatabase, ...: pythonic wrappers around the
#   underlying C object.

from ctypes import c_char_p, c_char, c_int, c_uint32, c_void_p
from typing import Optional, Tuple, Type, List
from dataclasses import dataclass
from pathlib import Path

import ctypes
import enum
import itertools
import logging


logger = logging.getLogger(__name__)


PREFIX = "libwacom_"


@dataclass
class _Api:
    name: str
    args: Tuple[Type[ctypes._SimpleCData], ...]
    return_type: Optional[Type[ctypes._SimpleCData]]

    @property
    def basename(self) -> str:
        return self.name.removeprefix(PREFIX)


@dataclass
class _Enum:
    name: str
    value: int

    @property
    def basename(self) -> str:
        return self.name.removeprefix("WACOM_").removeprefix("W")


class GlibC:
    _lib = None

    _api_prototypes: List[_Api] = [
        _Api(name="free", args=(c_void_p,), return_type=None),
    ]

    @staticmethod
    def _cdll():
        # BSD has 7, Linux has 6
        for libc in ("libc.so.6", "libc.so.7"):
            try:
                return ctypes.CDLL(libc, use_errno=True)
            except OSError:
                pass
        raise NotImplementedError("Not implemented for other libc.so")

    @classmethod
    def _load(cls):
        cls._lib = cls._cdll()
        for api in cls._api_prototypes:
            func = getattr(cls._lib, api.name)
            func.argtypes = api.args
            func.restype = api.return_type
            setattr(cls, api.basename, func)

    @classmethod
    def instance(cls):
        if cls._lib is None:
            cls._load()
        return cls


class LibWacom:
    """
    libwacom.so wrapper. This is a singleton ctypes wrapper into libwacom.so with
    minimal processing. Example:

    >>> lib = LibWacom.instance()
    >>> ctx = lib.database_new(None)
    >>> lib.wacom_unref(ctx)

    In most cases you probably want to use the ``WacomDevice`` class instead.
    """

    _lib = None

    @staticmethod
    def _cdll():
        return ctypes.CDLL("libwacom.so.9", use_errno=True)

    @classmethod
    def _load(cls):
        cls._lib = cls._cdll()
        for api in cls._api_prototypes:
            func = getattr(cls._lib, api.name)
            func.argtypes = api.args
            func.restype = api.return_type
            setattr(cls, api.basename, func)

        for e in cls._enums:
            setattr(cls, e.basename, e.value)

    @classmethod
    def instance(cls):
        if cls._lib is None:
            cls._load()
        return cls

    _api_prototypes: List[_Api] = [
        _Api(name="libwacom_error_new", args=(c_void_p,), return_type=c_void_p),
        _Api(name="libwacom_error_free", args=(c_void_p,), return_type=None),
        _Api(name="libwacom_error_get_code", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_error_get_message", args=(c_void_p,), return_type=c_char_p),
        _Api(name="libwacom_database_new", args=(), return_type=c_void_p),
        _Api(
            name="libwacom_database_new_for_path",
            args=(c_char_p,),
            return_type=c_void_p,
        ),
        _Api(name="libwacom_database_destroy", args=(c_void_p,), return_type=None),
        _Api(
            name="libwacom_new_from_builder",
            args=(c_void_p, c_void_p, c_int, c_void_p),
            return_type=c_void_p,
        ),
        _Api(
            name="libwacom_new_from_path",
            args=(c_void_p, c_char_p, c_int, c_void_p),
            return_type=c_void_p,
        ),
        _Api(
            name="libwacom_new_from_usbid",
            args=(c_void_p, c_int, c_int, c_void_p),
            return_type=c_void_p,
        ),
        _Api(
            name="libwacom_new_from_name",
            args=(c_void_p, c_char_p, c_void_p),
            return_type=c_void_p,
        ),
        _Api(
            name="libwacom_list_devices_from_database",
            args=(c_void_p, c_void_p),
            return_type=ctypes.POINTER(c_void_p),
        ),
        _Api(
            name="libwacom_print_device_description",
            args=(c_int, c_void_p),
            return_type=None,
        ),
        _Api(name="libwacom_destroy", args=(c_void_p,), return_type=None),
        _Api(
            name="libwacom_compare", args=(c_void_p, c_void_p, c_int), return_type=c_int
        ),
        _Api(name="libwacom_get_class", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_name", args=(c_void_p,), return_type=c_char_p),
        _Api(name="libwacom_get_model_name", args=(c_void_p,), return_type=c_char_p),
        _Api(
            name="libwacom_get_layout_filename", args=(c_void_p,), return_type=c_char_p
        ),
        _Api(name="libwacom_get_vendor_id", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_match", args=(c_void_p,), return_type=c_char_p),
        _Api(
            name="libwacom_get_matches",
            args=(c_void_p,),
            return_type=ctypes.POINTER(c_void_p),
        ),
        _Api(name="libwacom_get_paired_device", args=(c_void_p,), return_type=c_void_p),
        _Api(name="libwacom_get_product_id", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_width", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_height", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_has_stylus", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_has_touch", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_num_buttons", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_num_keys", args=(c_void_p,), return_type=c_int),
        _Api(
            name="libwacom_get_supported_styli",
            args=(c_void_p, c_void_p),
            return_type=c_void_p,
        ),
        _Api(
            name="libwacom_get_styli",
            args=(c_void_p, c_void_p),
            return_type=ctypes.POINTER(c_void_p),
        ),
        _Api(name="libwacom_has_ring", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_has_ring2", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_num_rings", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_has_touchswitch", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_ring_num_modes", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_ring2_num_modes", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_num_strips", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_strips_num_modes", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_num_dials", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_dial_num_modes", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_get_dial2_num_modes", args=(c_void_p,), return_type=c_int),
        _Api(
            name="libwacom_get_status_leds",
            args=(c_void_p, c_void_p),
            return_type=ctypes.POINTER(ctypes.c_int),
        ),
        _Api(
            name="libwacom_get_button_led_group",
            args=(c_void_p, c_char),
            return_type=c_int,
        ),
        _Api(name="libwacom_is_builtin", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_is_reversible", args=(c_void_p,), return_type=c_int),
        _Api(
            name="libwacom_get_integration_flags", args=(c_void_p,), return_type=c_int
        ),
        _Api(name="libwacom_get_bustype", args=(c_void_p,), return_type=c_int),
        _Api(
            name="libwacom_get_button_flag", args=(c_void_p, c_char), return_type=c_int
        ),
        _Api(
            name="libwacom_get_button_evdev_code",
            args=(c_void_p, c_char),
            return_type=c_int,
        ),
        _Api(
            name="libwacom_stylus_get_for_id",
            args=(c_void_p, c_int),
            return_type=c_void_p,
        ),
        _Api(name="libwacom_stylus_get_id", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_stylus_get_vendor_id", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_stylus_get_name", args=(c_void_p,), return_type=c_char_p),
        _Api(
            name="libwacom_stylus_get_paired_ids",
            args=(c_void_p, c_void_p),
            return_type=c_void_p,
        ),
        _Api(
            name="libwacom_stylus_get_num_buttons", args=(c_void_p,), return_type=c_int
        ),
        _Api(name="libwacom_stylus_has_eraser", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_stylus_is_eraser", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_stylus_has_lens", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_stylus_has_wheel", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_stylus_get_axes", args=(c_void_p,), return_type=c_int),
        _Api(name="libwacom_stylus_get_type", args=(c_void_p,), return_type=c_int),
        _Api(
            name="libwacom_stylus_get_eraser_type", args=(c_void_p,), return_type=c_int
        ),
        _Api(
            name="libwacom_stylus_get_paired_styli",
            args=(c_void_p, c_void_p),
            return_type=ctypes.POINTER(c_void_p),
        ),
        _Api(
            name="libwacom_print_stylus_description",
            args=(c_int, c_void_p),
            return_type=None,
        ),
        _Api(name="libwacom_builder_new", args=(), return_type=c_void_p),
        _Api(name="libwacom_builder_destroy", args=(c_void_p,), return_type=None),
        _Api(
            name="libwacom_builder_set_device_name",
            args=(c_void_p, c_char_p),
            return_type=None,
        ),
        _Api(
            name="libwacom_builder_set_match_name",
            args=(c_void_p, c_char_p),
            return_type=None,
        ),
        _Api(
            name="libwacom_builder_set_uniq",
            args=(c_void_p, c_char_p),
            return_type=None,
        ),
        _Api(
            name="libwacom_builder_set_bustype",
            args=(c_void_p, c_int),
            return_type=None,
        ),
        _Api(
            name="libwacom_builder_set_usbid",
            args=(c_void_p, c_int, c_int),
            return_type=None,
        ),
        _Api(name="libwacom_match_get_name", args=(c_void_p,), return_type=c_char_p),
        _Api(name="libwacom_match_get_uniq", args=(c_void_p,), return_type=c_char_p),
        _Api(name="libwacom_match_get_bustype", args=(c_void_p,), return_type=c_int),
        _Api(
            name="libwacom_match_get_product_id", args=(c_void_p,), return_type=c_uint32
        ),
        _Api(
            name="libwacom_match_get_vendor_id", args=(c_void_p,), return_type=c_uint32
        ),
        _Api(
            name="libwacom_match_get_match_string",
            args=(c_void_p,),
            return_type=c_char_p,
        ),
    ]

    _enums: List[_Enum] = [
        _Enum(name="WERROR_NONE", value=0),
        _Enum(name="WERROR_BAD_ALLOC", value=1),
        _Enum(name="WERROR_INVALID_PATH", value=2),
        _Enum(name="WERROR_INVALID_DB", value=3),
        _Enum(name="WERROR_BAD_ACCESS", value=4),
        _Enum(name="WERROR_UNKNOWN_MODEL", value=5),
        _Enum(name="WERROR_BUG_CALLER", value=6),
        _Enum(name="WBUSTYPE_UNKNOWN", value=0),
        _Enum(name="WBUSTYPE_USB", value=1),
        _Enum(name="WBUSTYPE_SERIAL", value=2),
        _Enum(name="WBUSTYPE_BLUETOOTH", value=3),
        _Enum(name="WBUSTYPE_I2C", value=4),
        _Enum(name="WACOM_DEVICE_INTEGRATED_NONE", value=0),
        _Enum(name="WACOM_DEVICE_INTEGRATED_DISPLAY", value=1),
        _Enum(name="WACOM_DEVICE_INTEGRATED_SYSTEM", value=2),
        _Enum(name="WCLASS_UNKNOWN", value=0),
        _Enum(name="WCLASS_INTUOS3", value=1),
        _Enum(name="WCLASS_INTUOS4", value=2),
        _Enum(name="WCLASS_INTUOS5", value=3),
        _Enum(name="WCLASS_CINTIQ", value=4),
        _Enum(name="WCLASS_BAMBOO", value=5),
        _Enum(name="WCLASS_GRAPHIRE", value=6),
        _Enum(name="WCLASS_ISDV4", value=7),
        _Enum(name="WCLASS_INTUOS", value=8),
        _Enum(name="WCLASS_INTUOS2", value=9),
        _Enum(name="WCLASS_PEN_DISPLAYS", value=10),
        _Enum(name="WCLASS_REMOTE", value=2),
        _Enum(name="WSTYLUS_UNKNOWN", value=0),
        _Enum(name="WSTYLUS_GENERAL", value=1),
        _Enum(name="WSTYLUS_INKING", value=2),
        _Enum(name="WSTYLUS_AIRBRUSH", value=3),
        _Enum(name="WSTYLUS_CLASSIC", value=4),
        _Enum(name="WSTYLUS_MARKER", value=5),
        _Enum(name="WSTYLUS_STROKE", value=6),
        _Enum(name="WSTYLUS_PUCK", value=7),
        _Enum(name="WSTYLUS_3D", value=8),
        _Enum(name="WSTYLUS_MOBILE", value=9),
        _Enum(name="WACOM_ERASER_UNKNOWN", value=0),
        _Enum(name="WACOM_ERASER_NONE", value=1),
        _Enum(name="WACOM_ERASER_INVERT", value=2),
        _Enum(name="WACOM_ERASER_BUTTON", value=3),
        _Enum(name="WACOM_BUTTON_NONE", value=0),
        _Enum(name="WACOM_BUTTON_POSITION_LEFT", value=1 << 1),
        _Enum(name="WACOM_BUTTON_POSITION_RIGHT", value=1 << 2),
        _Enum(name="WACOM_BUTTON_POSITION_TOP", value=1 << 3),
        _Enum(name="WACOM_BUTTON_POSITION_BOTTOM", value=1 << 4),
        _Enum(name="WACOM_BUTTON_RING_MODESWITCH", value=1 << 5),
        _Enum(name="WACOM_BUTTON_RING2_MODESWITCH", value=1 << 6),
        _Enum(name="WACOM_BUTTON_TOUCHSTRIP_MODESWITCH", value=1 << 7),
        _Enum(name="WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH", value=1 << 8),
        _Enum(name="WACOM_BUTTON_OLED", value=1 << 9),
        _Enum(name="WACOM_BUTTON_DIAL_MODESWITCH", value=1 << 10),
        _Enum(name="WACOM_BUTTON_DIAL2_MODESWITCH", value=1 << 11),
        _Enum(
            name="WACOM_BUTTON_MODESWITCH",
            value=1 << 5 | 1 << 6 | 1 << 7 | 1 << 8 | 1 << 10 | 1 << 11,
        ),
        _Enum(name="WACOM_BUTTON_DIRECTION", value=1 << 1 | 1 << 2 | 1 << 3 | 1 << 4),
        _Enum(name="WACOM_BUTTON_RINGS_MODESWITCH", value=1 << 5 | 1 << 6),
        _Enum(name="WACOM_BUTTON_TOUCHSTRIPS_MODESWITCH", value=1 << 7 | 1 << 8),
        _Enum(name="WACOM_BUTTON_DIALS_MODESWITCH", value=1 << 10 | 1 << 11),
        _Enum(name="WACOM_AXIS_TYPE_NONE", value=0),
        _Enum(name="WACOM_AXIS_TYPE_TILT", value=1 << 1),
        _Enum(name="WACOM_AXIS_TYPE_ROTATION_Z", value=1 << 2),
        _Enum(name="WACOM_AXIS_TYPE_DISTANCE", value=1 << 3),
        _Enum(name="WACOM_AXIS_TYPE_PRESSURE", value=1 << 4),
        _Enum(name="WACOM_AXIS_TYPE_SLIDER", value=1 << 5),
        _Enum(name="WFALLBACK_NONE", value=0),
        _Enum(name="WFALLBACK_GENERIC", value=1),
        _Enum(name="WCOMPARE_NORMAL", value=0),
        _Enum(name="WCOMPARE_MATCHES", value=1),
        _Enum(name="WACOM_STATUS_LED_UNAVAILABLE", value=0),
        _Enum(name="WACOM_STATUS_LED_RING", value=1),
        _Enum(name="WACOM_STATUS_LED_RING2", value=2),
        _Enum(name="WACOM_STATUS_LED_TOUCHSTRIP", value=3),
        _Enum(name="WACOM_STATUS_LED_TOUCHSTRIP2", value=4),
        _Enum(name="WACOM_STATUS_LED_DIAL", value=1),
        _Enum(name="WACOM_STATUS_LED_DIAL2", value=2),
    ]


class WacomBustype(enum.IntEnum):
    UNKNOWN = 0x0
    USB = 0x1
    I2C = 0x2
    BLUETOOTH = 0x3


class WacomMatch:
    def __init__(self, match):
        self.match = match
        lib = LibWacom.instance()

        def wrapper(func):
            return lambda *args, **kwargs: func(self.match, *args, **kwargs)

        # Map all device-specifice accessors into respective functions
        for api in lib._api_prototypes:
            allowlist = ["match"]
            if any(api.basename.startswith(n) for n in allowlist):
                func = getattr(lib, api.basename)
                setattr(self, api.basename.removeprefix("match_"), wrapper(func))

    @property
    def name(self) -> Optional[str]:
        name = self.get_name()
        return name.decode("utf-8") if name else None

    @property
    def uniq(self) -> Optional[str]:
        uniq = self.get_uniq()
        return uniq.decode("utf-8") if uniq else None

    @property
    def bustype(self) -> WacomBustype:
        return WacomBustype(self.get_bustype())

    @property
    def vendor_id(self) -> int:
        return self.get_vendor_id()

    @property
    def product_id(self) -> int:
        return self.get_product_id()


class WacomBuilder:
    def __init__(self, builder):
        self.builder = builder
        lib = LibWacom.instance()
        self._device_name = None
        self._match_name = None
        self._uniq = None
        self._usbid = None
        self._bustype = None

        def wrapper(func):
            return lambda *args, **kwargs: func(self.builder, *args, **kwargs)

        # Map all device-specifice accessors into respective functions
        for api in lib._api_prototypes:
            allowlist = ["builder"]
            if any(api.basename.startswith(n) for n in allowlist):
                func = getattr(lib, api.basename)
                setattr(self, api.basename.removeprefix("builder_"), wrapper(func))

    @property
    def device_name(self) -> Optional[str]:
        return self._device_name

    @property
    def match_name(self) -> Optional[str]:
        return self._match_name

    @property
    def uniq(self) -> Optional[str]:
        return self._uniq

    @property
    def bustype(self) -> Optional[WacomBustype]:
        return self._bustype

    @property
    def usbid(self) -> Optional[Tuple[int, int]]:
        return self._usbid

    @bustype.setter
    def bustype(self, bus: WacomBustype):
        self._bustype = bus
        self.set_bustype(bus.value)

    @usbid.setter
    def usbid(self, usbid: Tuple[int, int]):
        self._usbid = usbid
        self.set_usbid(usbid[0], usbid[1])

    @device_name.setter
    def device_name(self, name: str):
        self._device_name = name
        self.set_device_name(name.encode("utf-8"))

    @match_name.setter
    def match_name(self, name: str):
        self._match_name = name
        self.set_match_name(name.encode("utf-8"))

    @uniq.setter
    def uniq(self, uniq: str):
        self._uniq = uniq
        self.set_uniq(uniq.encode("utf-8"))

    @classmethod
    def create(
        cls,
        device_name: Optional[str] = None,
        match_name: Optional[str] = None,
        uniq: Optional[str] = None,
        usbid: Optional[Tuple[int, int]] = None,
        bus: Optional[WacomBustype] = None,
    ) -> "WacomBuilder":
        lib = LibWacom.instance()
        builder = WacomBuilder(lib.builder_new())
        if device_name is not None:
            builder.device_name = device_name
        if match_name is not None:
            builder.match_name = match_name
        if uniq is not None:
            builder.uniq = uniq
        if bus is not None:
            builder.bustype = bus
        if usbid is not None:
            builder.usbid = usbid
        return builder

    def __del__(self):
        lib = LibWacom.instance()
        lib.builder_destroy(self.builder)


class WacomStylusType(enum.IntEnum):
    UNKNOWN = 0
    GENERAL = 1
    INKING = 2
    AIRBRUSH = 3
    CLASSIC = 4
    MARKER = 5
    STROKE = 6
    PUCK = 7
    THREED = 8
    MOBILE = 9


class WacomEraserType(enum.IntEnum):
    UNKNOWN = 0
    NONE = 1
    INVERT = 2
    BUTTON = 3


class WacomStylus:
    def __init__(self, stylus):
        self.stylus = stylus
        lib = LibWacom.instance()

        def wrapper(func):
            return lambda *args, **kwargs: func(self.stylus, *args, **kwargs)

        # Map all device-specifice accessors into respective functions
        for api in lib._api_prototypes:
            allowlist = ["stylus"]
            if any(api.basename.startswith(n) for n in allowlist):
                denylist = ["stylus_get_paired_styli", "stylus_is_eraser"]
                if all(not api.basename.startswith(n) for n in denylist):
                    func = getattr(lib, api.basename)
                    setattr(self, api.basename.removeprefix("stylus_"), wrapper(func))

    @property
    def name(self):
        return self.get_name().decode("utf-8")

    @property
    def group(self):
        return self.get_group().decode("utf-8")

    @property
    def tool_id(self) -> int:
        return self.get_id()

    @property
    def vendor_id(self) -> int:
        return self.get_vendor_id()

    @property
    def num_buttons(self) -> int:
        return self.get_num_buttons()

    @property
    def is_eraser(self) -> bool:
        lib = LibWacom.instance()
        return lib.stylus_is_eraser(self.stylus) != 0

    @property
    def stylus_type(self) -> WacomEraserType:
        return WacomEraserType(self.get_eraser_type())

    @property
    def eraser_type(self) -> WacomEraserType:
        return WacomEraserType(self.get_eraser_type())

    def get_paired_styli(self) -> List["WacomStylus"]:
        lib = LibWacom.instance()
        paired = lib.stylus_get_paired_styli(self.stylus, None)
        styli = [
            WacomStylus(p)
            for p in itertools.takewhile(lambda ptr: ptr is not None, paired)
        ]
        GlibC.instance().free(paired)
        return styli


class WacomStatusLed(enum.IntEnum):
    UNAVAILABLE = -1
    RING = 0
    RING2 = 1
    TOUCHSTRIP = 2
    TOUCHSTRIP2 = 3
    DIAL = 4
    DIAL2 = 5


class WacomDevice:
    """
    Convenience wrapper to make using libwacom a bit more pythonic.
    """

    class IntegrationFlags(enum.IntEnum):
        DISPLAY = 1 << 0
        SYSTEM = 1 << 1

    class ButtonFlags(enum.IntEnum):
        NONE = 0
        POSITION_LEFT = 1 << 1
        POSITION_RIGHT = 1 << 2
        POSITION_TOP = 1 << 3
        POSITION_BOTTOM = 1 << 4
        RING_MODESWITCH = 1 << 5
        RING2_MODESWITCH = 1 << 6
        TOUCHSTRIP_MODESWITCH = 1 << 7
        TOUCHSTRIP2_MODESWITCH = 1 << 8
        OLED = 1 << 9
        DIAL_MODESWITCH = 1 << 10
        DIAL2_MODESWITCH = 1 << 11

        @staticmethod
        def modeswitch_flags() -> List["WacomDevice.ButtonFlags"]:
            return [
                WacomDevice.ButtonFlags.RING_MODESWITCH,
                WacomDevice.ButtonFlags.RING2_MODESWITCH,
                WacomDevice.ButtonFlags.TOUCHSTRIP_MODESWITCH,
                WacomDevice.ButtonFlags.TOUCHSTRIP2_MODESWITCH,
                WacomDevice.ButtonFlags.DIAL_MODESWITCH,
                WacomDevice.ButtonFlags.DIAL2_MODESWITCH,
            ]

    def __init__(self, device, destroy=True):
        self.device = device
        self._destroy_on_del = destroy

        lib = LibWacom.instance()

        def wrapper(func):
            return lambda *args, **kwargs: func(self.device, *args, **kwargs)

        # Map all device-specifice accessors into respective functions
        for api in lib._api_prototypes:
            allowlist = ["get_", "is_", "has_"]
            if any(api.basename.startswith(n) for n in allowlist):
                denylist = ["get_paired_device", "get_matches", "get_styli"]
                if all(not api.basename.startswith(n) for n in denylist):
                    func = getattr(lib, api.basename)
                    setattr(self, api.basename, wrapper(func))

        # This mashes all enums into the same namespace but oh well
        for e in lib._enums:
            val = getattr(lib, e.basename)
            setattr(self, e.basename, val)

    def get_paired_device(self) -> Optional[WacomMatch]:
        lib = LibWacom.instance()
        match = lib.get_paired_device(self.device)
        return WacomMatch(match) if match else None

    def get_matches(self) -> List[WacomMatch]:
        lib = LibWacom.instance()
        matches = lib.get_matches(self.device)

        return [
            WacomMatch(m)
            for m in itertools.takewhile(lambda ptr: ptr is not None, matches)
        ]

    def get_styli(self) -> List[WacomStylus]:
        lib = LibWacom.instance()
        styli = lib.get_styli(self.device, None)

        return [
            WacomStylus(m)
            for m in itertools.takewhile(lambda ptr: ptr is not None, styli)
        ]

    def __del__(self):
        if self._destroy_on_del:
            lib = LibWacom.instance()
            lib.destroy(self.device)

    @property
    def paired_device(self) -> Optional[WacomMatch]:
        return self.get_paired_device()

    @property
    def name(self):
        return self.get_name().decode("utf-8")

    @property
    def model_name(self):
        model = self.get_model_name()
        return model.decode("utf-8") if model else None

    @property
    def layout_filename(self):
        f = self.get_layout_filename()
        return f.decode("utf-8") if f else None

    @property
    def bustype(self):
        return self.get_bustype()

    @property
    def vendor_id(self):
        return self.get_vendor_id()

    @property
    def product_id(self):
        return self.get_product_id()

    @property
    def width(self):
        return self.get_width()

    @property
    def height(self):
        return self.get_height()

    @property
    def num_buttons(self):
        return self.get_num_buttons()

    @property
    def num_keys(self):
        return self.get_num_keys()

    @property
    def num_rings(self):
        return self.get_num_rings()

    @property
    def num_strips(self):
        return self.get_num_strips()

    @property
    def num_dials(self):
        return self.get_num_dials()

    @property
    def ring_num_modes(self):
        return self.get_ring_num_modes()

    @property
    def ring2_num_modes(self):
        return self.get_ring2_num_modes()

    @property
    def strip_num_modes(self):
        return self.get_strip_num_modes()

    @property
    def dial_num_modes(self):
        return self.get_dial_num_modes()

    @property
    def match(self):
        return self.get_match()

    @property
    def matches(self):
        return self.get_matches()

    @property
    def integration_flags(self) -> List[IntegrationFlags]:
        flags = self.get_integration_flags()
        return [f for f in WacomDevice.IntegrationFlags if f & flags != 0]

    def button_flags(self, button: str) -> List[ButtonFlags]:
        flags = self.get_button_flag(button.encode("utf-8"))
        return [f for f in WacomDevice.ButtonFlags if f & flags != 0]

    def button_evdev_code(self, button: str) -> int:
        return self.get_button_evdev_code(button.encode("utf-8"))

    def button_led_group(self, button: str) -> List[ButtonFlags]:
        return self.get_button_led_group(button.encode("utf-8"))

    @property
    def status_leds(self) -> List["WacomStatusLed"]:
        nleds = c_int()
        leds = self.get_status_leds(ctypes.byref(nleds))

        return [WacomStatusLed(l) for l in leds[: nleds.value]]


class WacomDatabase:
    """
    Convenience wrapper to make using libwacom a bit more pythonic.
    """

    class Fallback(enum.IntEnum):
        NONE = 0x0
        GENERIC = 0x1

    def __init__(self, path: Optional[Path] = None):
        lib = LibWacom.instance()
        if path is None:
            self.db = lib.database_new()  # type: ignore
        else:
            self.db = lib.database_new_for_path(str(path).encode("utf-8"))  # type: ignore
        assert self.db is not None

        def wrapper(func):
            return lambda *args, **kwargs: func(self.db, *args, **kwargs)

        for api in lib._api_prototypes:
            prefixes = ["new_from_", "list_devices_from_database"]
            if any(api.basename.startswith(prefix) for prefix in prefixes):
                func = getattr(lib, api.basename)
                setattr(self, api.name, wrapper(func))

    def __del__(self):
        db = getattr(self, "db", None)
        if db is not None:
            lib = LibWacom.instance()
            lib.database_destroy(db)

    def new_from_name(self, name: str) -> Optional[WacomDevice]:
        device = self.libwacom_new_from_name(name.encode("utf-8"), 0)
        return WacomDevice(device) if device else None

    def new_from_path(
        self, path: str, fallback: Fallback = Fallback.NONE
    ) -> Optional[WacomDevice]:
        device = self.libwacom_new_from_path(path.encode("utf-8"), fallback, 0)
        return WacomDevice(device) if device else None

    def new_from_usbid(self, vid: int, pid: int) -> Optional[WacomDevice]:
        device = self.libwacom_new_from_usbid(vid, pid, 0)
        return WacomDevice(device) if device else None

    def new_from_builder(
        self, builder: WacomBuilder, fallback: Fallback = Fallback.NONE
    ) -> Optional[WacomDevice]:
        device = self.libwacom_new_from_builder(builder.builder, fallback.value, 0)
        return WacomDevice(device) if device else None

    def list_devices(self) -> List[WacomDevice]:
        devices = self.libwacom_list_devices_from_database(self.db, 0)
        devs = [
            WacomDevice(d, destroy=False)
            for d in itertools.takewhile(lambda ptr: ptr is not None, devices)
        ]
        GlibC.instance().free(devices)
        return devs
