"""
XDMA Hexitec C++ Plugin
-----------------------

Provides a Python interface to the XDmaHexitec C++ library written by William Helsby
"""

class XDmaHexitec:

    def __init__(self, useQdma: bool, busNum: int, devNum: int, funcNum: int):
        """
        Initialise the XDmaHexitec class, connected to the defined XDMA device
        """

    def getNumChips(self) -> int:
        """
        Return the total number of chips in the Hexitec device
        """