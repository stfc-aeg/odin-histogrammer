import logging
from xdma_hexitec import XDmaHexitec

from .base_controller import BaseError, BaseController

from functools import partial

from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError

class HistogramException(BaseError):
    """Simple exception class to wrap lower-level exceptions."""

class HistogramController(BaseController):

    def __init__(self, options):
        super().__init__(options)

        self.hexitec = XDmaHexitec()