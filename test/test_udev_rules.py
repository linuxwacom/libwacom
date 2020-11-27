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
import libevdev
import os
from pathlib import Path
import pyudev
import pytest
import time
import logging
import sys


@pytest.fixture(scope='session', autouse=True)
def systemd_reload():
    '''Make sure our hwdb and udev rules are up-to-date'''
    import subprocess
    subprocess.run(['systemd-hwdb', 'update'])
    subprocess.run(['systemctl', 'daemon-reload'])


def pytest_generate_tests(metafunc):
    # for any function that takes a "tablet" argument return a Tablet object
    # filled with exactly one DeviceMatch from the list of all .tablet files
    # in the data dir. Where the tablet also has touch/buttons generate an
    # extra Finger or Pad device
    if 'tablet' in metafunc.fixturenames:
        datadir = Path(os.getenv('MESON_SOURCE_ROOT') or '.') / 'data'
        tablets = []
        for f in datadir.glob('*.tablet'):
            config = configparser.ConfigParser()
            config.read(f)
            name = config['Device']['Name']
            want_pad = config['Device'].get('Buttons', 0)
            want_finger = config['Features'].get('Touch') == 'true'
            integrated_in = config['Device'].get('IntegratedIn', '').split(';')
            is_touchscreen = set(integrated_in) & set(['Display', 'System'])

            for match in config['Device']['DeviceMatch'].split(';'):
                if not match or match == 'generic':
                    continue

                bus, vid, pid = match.split(':')[:3]  # skip the name part of the match
                if bus not in ['usb', 'bluetooth']:
                    continue

                vid = int(vid, 16)
                pid = int(pid, 16)
                if bus == 'usb':
                    bus = 0x3
                elif bus == 'bluetooth':
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
                    tablets.append(Tablet(name + ' Pad', bus, vid, pid))

                if want_finger:
                    tablets.append(Tablet(name + ' Finger', bus, vid, pid, is_touchscreen))

        # our tablets list now becomes the list of arguments passed to the
        # test functions taking a 'tablet' argument - one-by-one. So where
        # tablets contains 10 entries, our test function will be called 10
        # times.
        metafunc.parametrize('tablet', tablets)


@pytest.fixture
def uinput(tablet):
    dev = libevdev.Device()
    dev.name = tablet.name
    dev.id = {
        'vendor': tablet.vid,
        'product': tablet.pid,
        'bustype': tablet.bus
    }
    # Our rules match on pid/vid, so purposely make this look like a
    # non-tablet to verify that our rules apply anyway and not others
    dev.enable(libevdev.EV_REL.REL_X)
    dev.enable(libevdev.EV_REL.REL_Y)
    dev.enable(libevdev.EV_KEY.BTN_LEFT)
    dev.enable(libevdev.EV_KEY.BTN_RIGHT)

    try:
        uinput = dev.create_uinput_device()
        # We'll need the is_touchscreen later, so let's hide it in the
        # uinput device to pass it to the actual test
        try:
            uinput.is_touchscreen = tablet.is_touchscreen
        except AttributeError:
            pass
        time.sleep(0.3)
        return uinput
    except OSError:
        raise pytest.skip()


@pytest.mark.skipif(sys.platform != 'linux', reason='This test requires udev')
def test_hwdb_files(uinput):
    logging.debug('{:04x}:{:04x} {}'.format(uinput.id['vendor'], uinput.id['product'], uinput.name))
    udev = pyudev.Context()
    dev = pyudev.Devices.from_device_file(udev, uinput.devnode)
    props = list(dev.properties)  # convert to list for better error messages

    assert 'ID_INPUT' in props
    assert dev.properties['ID_INPUT'] == '1'

    assert 'ID_INPUT_TABLET' in props
    assert dev.properties['ID_INPUT_TABLET'] == '1'

    assert 'ID_INPUT_JOYSTICK' not in props

    if 'Finger' in uinput.name:
        if uinput.is_touchscreen:
            assert 'ID_INPUT_TOUCHSCREEN' in props
        else:
            assert 'ID_INPUT_TOUCHPAD' in props

    # For the Wacom Bamboo Pad we check for "Pad Pad" in the device name
    if 'Pad' in uinput.name:
        if 'Wacom Bamboo Pad' not in uinput.name or 'Pad Pad' in uinput.name:
            assert 'ID_INPUT_TABLET_PAD' in props
