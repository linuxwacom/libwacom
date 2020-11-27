#!/usr/bin/env python3
#
# Run with pytest

import configparser
import os
import re
from pathlib import Path


def pytest_generate_tests(metafunc):
    # for any function that takes a "tabletfile" argument return the path to
    # a tablet file
    if 'tabletfile' in metafunc.fixturenames:
        datadir = Path(os.getenv('MESON_SOURCE_ROOT') or '.') / 'data'
        metafunc.parametrize('tabletfile', [f for f in datadir.glob('*.tablet')])


def test_device_match(tabletfile):
    config = configparser.ConfigParser()
    config.read(tabletfile)

    # Match format must be bus:vid:pid:name
    # where bus is 'usb' or 'bluetooth'
    # where vid/pid is a lowercase hex
    # where name is optional
    for match in config['Device']['DeviceMatch'].split(';'):
        if not match or match == 'generic':
            continue

        bus, vid, pid = match.split(':')[:3]  # skip the name part of the match
        assert bus in ['usb', 'bluetooth', 'i2c', 'serial'], f'{tabletfile}: unknown bus type'
        assert re.match('[0-9a-f]{4}', vid), f'{tabletfile}: {vid} must be lowercase hex'
        assert re.match('[0-9a-f]{4}', pid), f'{tabletfile}: {pid} must be lowercase hex'
