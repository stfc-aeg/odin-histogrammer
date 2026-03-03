from abc import ABC, abstractmethod
from histogrammer.lib import XDmaHexitec


class BaseHandler(ABC):

    param_tree: dict[str]
    hexitec: XDmaHexitec

    @abstractmethod
    def __init__(self, options: dict[str, str]):
        self.hexitec = None

    def initialise(self, hexitec: XDmaHexitec):
        self.hexitec = hexitec

    def cleanup(self):
        """
        Removes the refernce the the Hexitec object, allowing garbage collection to clean it up
        Additional cleanup steps may need to be implemented by the individual Handler
        """
        self.hexitec = None

