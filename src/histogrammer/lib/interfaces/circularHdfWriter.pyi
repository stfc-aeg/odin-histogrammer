from structs import CircWriterReadoutMode, CircWriterUdpTxOnlyMode
from XDmaHexitec import XDmaHexitec

class CircularHdfWriter:
    """
    Class designed to allow continuous output of histograms to a HDF5 file during an aquisition
    Software is not designed to allow both this and the UDP output to run at the same time.
    """

    def __init__(self, hexitec: XDmaHexitec, fName: str, numEng: int,
                 enbSpectra: bool, enbMapped: bool, sumChips: bool) -> None:
        """
        Create a new CircularHdfWriter to output histograms to a HDF file during an acquisition
        
        :param hexitec: class performing the acquisition, used to access various values.
        :type hexitec: XDmaHexitec 
        :param fName: Name of the HDF5 file that will be written to
        :type fName: str
        :param numEng: Number of energy bins
        :type numEng: int
        :param enbSpectra: Enable saving of Spectra data
        :type enbSpectra: bool
        :param enbMapped: Enable saving of Mapped data
        :type enbMapped: bool
        :param sumChips: Sum data from all chips
        :type sumChips: bool
        """
    def setupReadoutMode(self, readoutMode: CircWriterReadoutMode, numSpectraThreadsReq: int,
                         txOnly: CircWriterUdpTxOnlyMode ) -> None:
        """
        Setup the readout mode for the HdfWriter

        :param readoutMode: The Readout Mode that defines how the data will be read out, and saved
        :type readoutMode: CircWriterReadoutMode
        :param numSpectraThreadsReq: Number of threads to use for readout. Recommended only 1
        :type numSpectraThreadsReq: int
        :param txOnly: Defines the UDP TX behaviour. **UNUSED**
        :type txOnly: CircWriterUdpTxOnlyMode
        """
    
    def start(self) -> None:
        """
        Start the Circular HDF Writer, enabling its readout and creating the output file
        
        This starts the number of threads requested when the readout mode was setup to listen
        for new Time Frames, to then save out those frames when updated.
        """

    def checkProgress(self, maxTF: int) -> int:
        """
        Docstring for checkProgress
        
        
        :param maxTF: The last time frame to check, if using multiple readout threads
        :type maxTF: int
        :return: The last frame processed by the writer
        :rtype: int
        """

    def getMappedOverRuns(self) -> int:
        """
        Get the number of overrun Mapped frames, which are frames that have been missed
        
        :return: Number of Overrun Mapped Frames
        :rtype: int
        """

    def getSpectraOverRuns(self) -> int:
        """
        Get the number of overrun frames, which are frames that have been missed
        
        :return: Number of Overrun Mapped Frames
        :rtype: int
        """