#!/usr/bin/env python3
#
# This file is formatted with ruff format
#

from pathlib import Path
import logging
import os
import pytest

from . import WacomDatabase

logger = logging.getLogger(__name__)


def load_test_db() -> WacomDatabase:
    try:
        dbpath = os.environ.get("MESON_SOURCE_ROOT")
        if dbpath is None and (Path.cwd() / "meson.build").exists():
            dbpath = Path.cwd()
            logger.info(f"Defaulting to MESON_SOURCE_ROOT={dbpath}")
        return WacomDatabase(path=Path(dbpath) / "data" if dbpath else None)
    except AttributeError as e:
        pytest.exit(
            f"Failed to initialize and wrap libwacom.so: {e}. You may need to set LD_LIBRARY_PATH and optionally MESON_SOURCE_ROOT"
        )


@pytest.fixture()
def db():
    return load_test_db()
