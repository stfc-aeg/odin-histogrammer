import logging
from typing import Callable, TypeVar, Literal, ParamSpec
from contextlib import redirect_stdout
from functools import wraps

T = TypeVar("T")
P = ParamSpec("P")

AcquisitionMode = Literal["continuous", "timed", "count frames"]
OutputMode = Literal["UDP", "HDF5"]

class InternalLibException(Exception):
    """Exception that translates a RuntimeError thrown by the Pybind11 module into a python exception"""

class HexitecUnconnectedException(Exception):
    """Exception that reports that the Hexitec device has not been connected"""

class RedirectStdout:

    def __init__(self, name=None, level=logging.DEBUG):
        self.level = level
        self.logger = logging.getLogger(name)
        self._redirector = redirect_stdout(self)

    def write(self, msg: str):
        if msg and not msg.isspace():
            self.logger.log(self.level, msg.rstrip())
    
    def flush(self):
        pass

    def __enter__(self):
        self._redirector.__enter__()
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        self._redirector.__exit__(exc_type, exc_value, traceback)



def _get_bitwise_trailing_zeros(val):
    """Method to get the number of trailing 0s on a binary value.
    Used to calculate how much to shift a masked value to return the specific value regardless of its position"""
    c = 0
    v = (val ^ (val - 1)) >> 1
    while v > 0:
        v >>= 1
        c += 1
    return c

def splitRegisterIntoValues(reg: int, *masks: int) -> tuple[int, ...]:
    """
    Split a value read from a register into parts
    
    :param reg: Full Register Value
    :type reg: int
    :param masks: List of bitmasks for each part of the register
    :type masks: int
    :return: The values from the register split into individual values
    :rtype: tuple[int, ...]
    """
    retVal: tuple[int] = ()
    for mask in masks:
        shift = _get_bitwise_trailing_zeros(mask)
        retVal = retVal + ((reg & mask) >> shift,)

    return retVal

def UsesHexitecLibrary(level=logging.DEBUG):
    """
    Decorator to ensure the Hexitec object exists, and to automatically redirect
    any stdout from the c++ code to the available logger.

    :param level: The logging level to output any stdout at. Defaults to DEBUG
    :type level: int

    :raises HexitecUnconnectedException: If the Hexitec object has not been created.
    """
    def dectorator(func):
        @wraps(func)
        def _wrapper(self, *args, **kwargs):
            if self.hexitec is None and func.__name__ not in ["XDmaHexitec", "connect"]:
                raise HexitecUnconnectedException("{}: Hexitec Device Not Connected".format(func.__name__))
            with RedirectStdout(level=level):
                val = func(self, *args, **kwargs)
            return val
        return _wrapper
    return dectorator