from .base_adapter import BaseAdapter
from .controller import HistogramController

class HistogramAdapter(BaseAdapter):
    """Histogram Adapter Class"""

    controller_cls = HistogramController