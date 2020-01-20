#!/usr/bin/env python3
#
# Copyright © 2018 Red Hat, Inc.
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

import os
from pathlib import Path
import configparser

wacom_receivers = [
    0x84,
]
RECEIVERS = ['usb:056a:{:04x}'.format(r) for r in wacom_receivers]


def pytest_generate_tests(metafunc):
    # For any test that takes a table_file argument return the list
    # of .tablet files in the LIBWACOM_DATA_DIR, thus making those files the
    # arguments to those tests.
    if 'tablet_file' in metafunc.fixturenames:
        datadir = Path(os.environ['LIBWACOM_DATA_DIR'])
        files = [f for f in datadir.glob('*.tablet')]
        metafunc.parametrize('tablet_file', files)


def test_no_receiver_id(tablet_file):
    data = configparser.ConfigParser(strict=True)
    # Don't convert to lowercase
    data.optionxform = lambda option: option
    data.read(tablet_file)

    matches = data['Device']['DeviceMatch']

    for device_match in matches.split(';'):
        assert device_match not in RECEIVERS
