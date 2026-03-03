"""
XDMA Hexitec C++ Plugin
-----------------------

Provides a Python interface to the XDmaHexitec C++ library written by William Helsby
"""
from typing import Final

from .interfaces.consts import *
from .interfaces.structs import *
from .interfaces.circularHdfWriter import *
from .interfaces.XDmaHexitec import *

lib_version: Final[int]
"""SVN Commit number, for versioning the package"""