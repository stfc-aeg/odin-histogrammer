from typing import Callable, TypeVar, Literal, ParamSpec

T = TypeVar("T")
P = ParamSpec("P")

AcquisitionMode = Literal["continuous", "timed", "count frames"]
OutputMode = Literal["UDP", "HDF5"]

class InternalLibException(Exception):
    """Exception that translates a RuntimeError thrown by the Pybind11 module into a python exception"""

class HexitecUnconnectedException(Exception):
    """Exception that reports that the Hexitec device has not been connected"""

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


def checkHexitecConnect(func: Callable[P, T]) -> Callable[P, T]:
    """DECTORATOR for functions to ensure hexitec object has been initialised"""
    def _wrapper(self, *args: P.args, **kwargs: P.kwargs) -> T:
        if self.hexitec is None:
            raise HexitecUnconnectedException("{}: Hexitec Device Not Connected".format(func.__name__))
        return func(self, *args, **kwargs)
    return _wrapper