from odin_control.adapters.adapter import ApiAdapter
from .controller import HistogramController, HistogramException
from histogrammer._version import __version__


class HistogramAdapter(ApiAdapter):
    """Histogram Adapter Class"""

    controller_cls = HistogramController
    error_cls = HistogramException
    version = __version__
