#include <pybind11/pybind11.h>
#include <pybind11/native_enum.h>
#include <pybind11/iostream.h>
#include <pybind11/stl.h>

#include "xdma_hexitec.h"
#include "circular_hdf_writer.h"

#include "hexitec_version.h"

#include <stdint.h>
#include <iostream>
#include <string>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;


PYBIND11_MODULE(_core, m, py::mod_gil_not_used(), py::multiple_interpreters::per_interpreter_gil()) {
    m.doc() = R"pbdoc(
        Hexitec Python Binding
        ----------------------
        )pbdoc";
    py::class_<XDmaHexitec> hexitec(m, "XDmaHexitec");
    py::class_<CircularHdfWriter> circularHdfWriter(m, "CircularHdfWriter");
    py::class_<HexitecITfgStat> HexitecITfgStat(m, "HexitecITfgStat");
    py::class_<DataMoverContext> dmContext(m, "DataMoverContext");

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif

#ifdef SVN_VERSION
    m.attr("lib_version") = MACRO_STRINGIFY(SVN_VERSION);
#else
    m.attr("lib_version") = "dev";
#endif

    // #define values and register masks used by the Histogrammer
    m.attr("DATA_PATH_ENB_FLUSH") = HEXITEC_DATA_PATH_ENB_FLUSH;
    m.attr("DATA_PATH_SHORT_BURST_MODE") = HEXITEC_DATA_PATH_SHORT_BURST_MODE;
    m.attr("ETHERNET_PM_TICK_REG") = ETHERNET_PM_TICK_REG;
    m.attr("DM0_AUTO_TF") = HEXITEC_DM0_AUTO_TF;
    
    m.attr("MASK_HIST_FORMAT_NUMBINS") = 0x7;
    m.attr("MASK_HIST_FORMAT_RUNMODE") = 0x7<<3;
    m.attr("MASK_HIST_FORMAT_MAPPEDMODE") = 0x7<<8;

    m.attr("MASK_CLUSTER_MODE") = 0x7;
    m.attr("MASK_CLUSTER_TRIG_MODE") = 0x3 << 8;

    m.attr("MASK_BSUB_MODE") = 0xF << 4;
    m.attr("MASK_BSUB_DIV") = 0xF;
    m.attr("MASK_BSUB_DITHER") = 1 << 12;

    m.attr("MASK_CSHARE_ENB_EDGE") = HEXITEC_CSHARE_ENB_EDGE_POS_CORR;
    m.attr("MASK_CSHARE_ENB_NEG") = HEXITEC_CSHARE_ENB_NEG_NEB_CORR;
    m.attr("MASK_CSHARE_ENB_L_POS") = HEXITEC_CSHARE_ENB_L_POS_CORR;
    m.attr("MASK_CSHARE_DIS_SUM") = HEXITEC_CSHARE_DIS_SUMMING;
    m.attr("MASK_CSHARE_DIS_ADJ") = HEXITEC_CSHARE_DIS_ADJUST_POSN;

    m.def("GET_THRES_POS", [](uint32_t x)
    {
        return HEXITEC_GET_THRES_POS(x);
    });
    m.def("GET_THRES_NEG", [](uint32_t x)
    {
        return HEXITEC_GET_THRES_NEG(x);
    });



    hexitec.def(py::init<int, int, int, int>(), py::call_guard<py::scoped_ostream_redirect, py::scoped_estream_redirect>())
        .def("getNumChips", &XDmaHexitec::getNumChips)
        // .def("getNumChipCols", &XDmaHexitec::getNumChipCols)
        // .def("getNumChipRows", &XDmaHexitec::getNumChipRows)
        // .def("getMaxAdcValue", &XDmaHexitec::getMaxAdcValue)
        // .def("getBsubRefScale", &XDmaHexitec::getBsubRefScale)
        // .def("getNumHBMPorts", &XDmaHexitec::getNumHBMPorts)
        // .def("getNumProcCol", &XDmaHexitec::getNumProcCol)
        // .def("getHasFIFOMon", &XDmaHexitec::getHasFIFOMon)
        .def("getGeneration", &XDmaHexitec::getGeneration)
        // .def("getMaxBitsClustGrade", &XDmaHexitec::getMaxBitsClustGrade)
        // .def("getRegionMask", &XDmaHexitec::getRegionMask)
        // .def("getNumPbDma", &XDmaHexitec::getNumPbDma)
        // .def("getNumScopeDma", &XDmaHexitec::getNumScopeDma)
        // .def("getNBitsAddrPWLin", &XDmaHexitec::getNBitsAddrPWLin)
        .def("getNumRxUdp", &XDmaHexitec::getNumRxUdp)
        .def("getNumTxUdp", &XDmaHexitec::getNumTxUdp)
        // .def("writeChipRegs", &XDmaHexitec::writeChipRegs)
        // .def("readChipRegs", &XDmaHexitec::readChipRegs)
        .def("setChipReg", &XDmaHexitec::setChipReg)
        .def("getChipReg", &XDmaHexitec::getChipReg)
        // .def("writeGlobRegs", &XDmaHexitec::writeGlobRegs)
        // .def("readGlobRegs", &XDmaHexitec::readGlobRegs)
        .def("setGlobReg", &XDmaHexitec::setGlobReg, "Set the value of a global register", py::arg("offset"), py::arg("value"))
        .def("getGlobReg", &XDmaHexitec::getGlobReg)
        .def("getGlobReg64", &XDmaHexitec::getGlobReg64)
        .def("setPixelLUT", &XDmaHexitec::setPixelLUT)
        // .def("writePixelLUT", &XDmaHexitec::writePixelLUT) // Wrapped methods below to handle array pointer
        // .def("readPixelLUT", &XDmaHexitec::readPixelLUT)
        // .def("setPixelLin", &XDmaHexitec::setPixelLin)
        // .def("writePixelLin", &XDmaHexitec::writePixelLin)
        // .def("readPixelLin", &XDmaHexitec::readPixelLin)
        // .def("setSharedLUT", &XDmaHexitec::setSharedLUT)
        // .def("writeSharedLUT", &XDmaHexitec::writeSharedLUT)  // Wrapped methods below to handle array pointer
        // .def("readSharedLUT", &XDmaHexitec::readSharedLUT)
        .def("initRecipLUT", &XDmaHexitec::initRecipLUT)
        .def("initCShareLUTs", &XDmaHexitec::initCShareLUTs)
        .def("initPixelMask", &XDmaHexitec::initPixelMask)
        // .def("setPixelMask", &XDmaHexitec::setPixelMask)
        .def("loadLinearityGainAscii", &XDmaHexitec::loadLinearityGainAscii)
        .def("loadLinearityAscii", &XDmaHexitec::loadLinearityAscii)
        // .def("loadLinearityGainHDF5", &XDmaHexitec::loadLinearityGainHDF5)
        .def("loadCShareAscii", &XDmaHexitec::loadCShareAscii)
        .def("loadCShareAsciiMC", &XDmaHexitec::loadCShareAsciiMC)
        // .def("loadEngMapAscii", &XDmaHexitec::loadEngMapAscii)
        // .def("initEngMapThres", &XDmaHexitec::initEngMapThres)
        .def("loadBadPixelsTrigAscii", &XDmaHexitec::loadBadPixelsTrigAscii)
        .def("loadBadPixelsOutputAscii", &XDmaHexitec::loadBadPixelsOutputAscii)
        // .def("dmaReset", &XDmaHexitec::dmaReset)
        // .def("dmaBuildDesc", &XDmaHexitec::dmaBuildDesc)
        // .def("dmaBuildPBDesc", &XDmaHexitec::dmaBuildPBDesc)
        // .def("dmaStart", &XDmaHexitec::dmaStart)
        // .def("dmaStop", &XDmaHexitec::dmaStop)
        // .def("dmaReadStatus", &XDmaHexitec::dmaReadStatus)
        // .def("dmaReadCurrDesc", &XDmaHexitec::dmaReadCurrDesc)
        // .def("dmaReadCurrDescNum", &XDmaHexitec::dmaReadCurrDescNum)
        // .def("dmaWaitIdle", &XDmaHexitec::dmaWaitIdle)
        // .def("dmaWaitIdleNoExcept", &XDmaHexitec::dmaWaitIdleNoExcept)
        // .def("dmaPrintDesc", &XDmaHexitec::dmaPrintDesc)
        // .def("getMaxPbFrames", &XDmaHexitec::getMaxPbFrames)
        // .def("getMaxScopeFrames", &XDmaHexitec::getMaxScopeFrames)
        // .def("getPbFrameBytesAligned", &XDmaHexitec::getPbFrameBytesAligned)
        // .def("writeDmaBuff", &XDmaHexitec::writeDmaBuff)
        // .def("readDmaBuff", &XDmaHexitec::readDmaBuff)
        .def("setBaselineMode", &XDmaHexitec::setBaselineMode)
        // .def("loadBaseline", &XDmaHexitec::loadBaseline)
        // .def("waitLoadBaseline", &XDmaHexitec::waitLoadBaseline)
        // .def("saveBaseline", &XDmaHexitec::saveBaseline)
        // .def("waitSaveBaseline", &XDmaHexitec::waitSaveBaseline)
        .def("setAbsTriggerThres", &XDmaHexitec::setAbsTriggerThres)
        .def("setMainTriggerThres", &XDmaHexitec::setMainTriggerThres)
        .def("setLowerTriggerThres", &XDmaHexitec::setLowerTriggerThres)
        // .def("setLinearityRaw", &XDmaHexitec::setLinearityRaw)
        .def("setLinearityOne", &XDmaHexitec::setLinearityOne)
        .def("linearityAddOffset", &XDmaHexitec::linearityAddOffset)
        .def("setClusterMode", &XDmaHexitec::setClusterMode)
        .def("setCShareMode", &XDmaHexitec::setCShareMode)
        .def("setClusterTypes", &XDmaHexitec::setClusterTypes)
        .def("setHistFormat", &XDmaHexitec::setHistFormat)
        // .def("getHistFormat", &XDmaHexitec::getHistFormat)
        // .def("getnBinsEng", &XDmaHexitec::getnBinsEng)
        // .def("getEngLsb10", &XDmaHexitec::getEngLsb10)
        // .def("getUsePosn", &XDmaHexitec::getUsePosn)
        // .def("getnBinsClustClass", &XDmaHexitec::getnBinsClustClass)
        // .def("getnBinsCharac", &XDmaHexitec::getnBinsCharac)
        // .def("getNumTF", &XDmaHexitec::getNumTF)
        // .def("getNumTFMapped", &XDmaHexitec::getNumTFMapped)
        // .def("getEngOnly", &XDmaHexitec::getEngOnly)
        // .def("getUseClustGrade", &XDmaHexitec::getUseClustGrade)
        .def("enableHist", &XDmaHexitec::enableHist)
        // .def("readHistEngRowColTime", &XDmaHexitec::readHistEngRowColTime)
        // .def("readHistEngColRowTime", &XDmaHexitec::readHistEngColRowTime)
        // .def("readMappedEngRowColTime", &XDmaHexitec::readMappedEngRowColTime)
        // .def("readMappedEngColRowTime", &XDmaHexitec::readMappedEngColRowTime)
        // .def("readHistEngRowColCCTime", &XDmaHexitec::readHistEngRowColCCTime)
        // .def("readHistEngColRowCCTime", &XDmaHexitec::readHistEngColRowCCTime)
        // .def("readMappedEngRowColCCTime", &XDmaHexitec::readMappedEngRowColCCTime)
        // .def("readHistEngGlobColRowTime", &XDmaHexitec::readHistEngGlobColRowTime)
        // .def("readHistEngGlobColRowCCTime", &XDmaHexitec::readHistEngGlobColRowCCTime)
        // .def("readMappedEngGlobColRowTime", &XDmaHexitec::readMappedEngGlobColRowTime)
        // .def("readHistEngTime", &XDmaHexitec::readHistEngTime)
        // .def("readHistEngCCTime", &XDmaHexitec::readHistEngCCTime)
        // .def("readHistEngCalibClass", &XDmaHexitec::readHistEngCalibClass)
        // .def("readHistCharac2d", &XDmaHexitec::readHistCharac2d)
        // .def("readHistCharac3d", &XDmaHexitec::readHistCharac3d)
        .def("clearHistAll", &XDmaHexitec::clearHistAll)
        // .def("clearHistTimeframes", &XDmaHexitec::clearHistTimeframes)
        // .def("setDefaultXDmaChan", &XDmaHexitec::setDefaultXDmaChan)
        // .def("setDmaDescRWChan", &XDmaHexitec::setDmaDescRWChan)
        // .def("getRxEthernetReg", &XDmaHexitec::getRxEthernetReg)
        // .def("getRxEthernetReg64", &XDmaHexitec::getRxEthernetReg64)
        .def("setRxEthernetReg", &XDmaHexitec::setRxEthernetReg)
        // .def("udpRxTestCreateSockets", &XDmaHexitec::udpRxTestCreateSockets)
        // .def("getUdpRxTestSocket", &XDmaHexitec::getUdpRxTestSocket)
        .def("setRxEthernetLoopback", &XDmaHexitec::setRxEthernetLoopback)
        .def("udpRxSetup", &XDmaHexitec::udpRxSetup)
        // .def("getMacAddr", &XDmaHexitec::getMacAddr)
        .def("udpResetCounts", &XDmaHexitec::udpResetCounts)
        // .def("udpTxTestCreateSockets", &XDmaHexitec::udpTxTestCreateSockets)
        .def("udpTxSetup", &XDmaHexitec::udpTxSetup)
        // .def("getUdpTxTestSocket", &XDmaHexitec::getUdpTxTestSocket)
        // .def("udpTxTestReadFrame", &XDmaHexitec::udpTxTestReadFrame)
        // .def("udpShowRxStatus", &XDmaHexitec::udpShowRxStatus)
        .def("iTfgDisable", &XDmaHexitec::iTfgDisable)
        .def("iTfgTrigger", &XDmaHexitec::iTfgTrigger)
        .def("iTfgSetup", &XDmaHexitec::iTfgSetup)
        // .def("iTfgReadStatus", &XDmaHexitec::iTfgReadStatus)
        // .def("printClockFrequencies", &XDmaHexitec::printClockFrequencies)
        // .def("startDataMoverStream", &XDmaHexitec::startDataMoverStream)
        .def("stopDataMoverStreamUDP", &XDmaHexitec::stopDataMoverStreamUDP)
        .def("startDataMoverStreamUDP", &XDmaHexitec::startDataMoverStreamUDP)
        // .def("startDataMoverEvList", &XDmaHexitec::startDataMoverEvList)
        .def("disableDataMoverUDPTrailer", &XDmaHexitec::disableDataMoverUDPTrailer)
        // .def("clearDataMoverOverRun", &XDmaHexitec::clearDataMoverOverRun)
        // .def("getDataMoverOverRun", &XDmaHexitec::getDataMoverOverRun)
        // .def("readDataMoverStream", &XDmaHexitec::readDataMoverStream) //overwritten below
        // .def("getDataMoverUDPIndex", &XDmaHexitec::getDataMoverUDPIndex)
        // .def("saveSpectraAsc", &XDmaHexitec::saveSpectraAsc)
        // .def("saveSpectraDet", &XDmaHexitec::saveSpectraDet)
        // .def("saveSpectraHdf5", &XDmaHexitec::saveSpectraHdf5) // overwritten below to avoid compilatione error
        .def("getFlushedFrame", &XDmaHexitec::getFlushedFrame)
        // .def("getBsubMaskName", &XDmaHexitec::getBsubMaskName)
        // .def("getDiagnosticCounters", py::overload_cast<int, uint32_t *, uint32_t*>(&XDmaHexitec::getDiagnosticCounters))
        // .def("getDiagnosticCounters", py::overload_cast<uint32_t *, uint32_t*>(&XDmaHexitec::getDiagnosticCounters))
        // .def("writeIrqEnable", &XDmaHexitec::writeIrqEnable)
        // .def("getIrqEnable", &XDmaHexitec::getIrqEnable)
        // .def("setIrqEnable", &XDmaHexitec::setIrqEnable)
        // .def("clearIrqEnable", &XDmaHexitec::clearIrqEnable)
        // .def("getEventFd", &XDmaHexitec::getEventFd)
        .def("supportsIrqs", &XDmaHexitec::supportsIrqs)
        // .def("getOneFIFOCounts", &XDmaHexitec::getOneFIFOCounts)
        // .def("getAllFIFOCounts", &XDmaHexitec::getAllFIFOCounts)
        .def("getInpTimeFrame", &XDmaHexitec::getInpTimeFrame)
        // .def("setClusterGradeReg", &XDmaHexitec::setClusterGradeReg)
        // .def("setClusterGrade", &XDmaHexitec::setClusterGrade)
        // .def("getClusterGrade", &XDmaHexitec::getClusterGrade)
        .def("saveSettingsHdf5", &XDmaHexitec::saveSettingsHdf5)
        .def("loadSettingsHdf5", &XDmaHexitec::loadSettingsHdf5)
        // .def("writePixelMask", &XDmaHexitec::writePixelMask) // Wrapped methods below to handle array pointer
        // .def("readPixelMask", &XDmaHexitec::readPixelMask)
        .def_readwrite("m_debug", &XDmaHexitec::m_debug)
        ;

    hexitec.def("readPixelLUT", [](XDmaHexitec &self, int chip, int region,
                                    int firstCol, int numCol,
                                    int firstRow, int numRow)
    {
        // create new array pointer to store data
        uint32_t *x = new uint32_t [numCol*numRow];
        self.readPixelLUT(chip, region, firstCol, numCol, firstRow, numRow, x);

        //convert array into vector, so when returned it is auto-cast into a python list
        std::vector<uint32_t> retVal(x, x + (numCol*numRow));

        return retVal;
    });
    
    hexitec.def("writePixelLUT", [](XDmaHexitec &self, int chip, int region,
                                    int firstCol, int numCol,
                                    int firstRow, int numRow,
                                    std::vector<uint32_t> data)
    {
        uint32_t *x = data.data();  // convert vector (which started as python vector) into an array

        self.writePixelLUT(chip, region, firstCol, numCol, firstRow, numRow, x);
    });

    hexitec.def("readSharedLUT", [](XDmaHexitec &self, int chip, int region,
                                        int stream, int first, int num)
    {
        uint32_t *x = new uint32_t[num];
        self.readSharedLUT(chip, region, stream, first, num, x);

        std::vector<uint32_t> retVal(x, x+num);
        return retVal;
    });

    hexitec.def("writeSharedLUT", [](XDmaHexitec &self, int chip, int region,
                                        int stream, int first, int num,
                                    std::vector<uint32_t> data)
    {
        uint32_t *x = data.data();

        self.writeSharedLUT(chip, region, stream, first, num, x);
    });

    hexitec.def("readPixelMask", [](XDmaHexitec &self, int chip,
                                    int firstCol, int numCols,
                                    int firstRow, int numRows)
    {
        uint8_t *x = new uint8_t[numCols*numRows];
        self.readPixelMask(chip, firstCol, numCols, firstRow, numRows, x);
        
        //convert array into vector, so when returned it is auto-cast into a python list
        std::vector<uint8_t> retVal(x, x + (numCols*numRows));

        return retVal;

    });

    hexitec.def("writePixelMask", [](XDmaHexitec &self, int chip,
                                     int firstCol, int numCols,
                                     int firstRow, int numRows,
                                     std::vector<uint8_t> data)
    {
        uint8_t *x = data.data();

        self.writePixelMask(chip, firstCol, numCols, firstRow, numRows, x);
    });

    // Source IP Addr Get/Set
    // IP Addr of the Alpha Data card producing raw data
    hexitec.def("getSrcAddr", [](XDmaHexitec &self, int core)
    {
        // RX UDP cores are in RX mode, so Destination and Source are flipped
        return self.m_udpCore[core].getDstIpAddr();
    }, py::arg("core") = 0);

    hexitec.def("setSrcAddr", [](XDmaHexitec &self, int addr, int core)
    {
        self.m_udpCore[core].setDstIpAddr(addr);

    }, py::arg("addr"), py::arg("core") = 0);

    // Destination IP Addr Get/Set
    // IP Addr of the network interface sending completed Histogram Packets to
    hexitec.def("getDestAddr", [](XDmaHexitec &self, int core)
    {
        //Note this is using the TxCore Array of UDP Cores.
        return self.m_udpTxCore[core].getDstIpAddr();
    }, py::arg("core") = 0);

    hexitec.def("setDestAddr", [](XDmaHexitec &self, int addr, int core)
    {
        self.m_udpTxCore[core].setDstIpAddr(addr);
    }, py::arg("addr"), py::arg("core") = 0);

    // Accelerator (The Histogrammer Card) IP Addr Get/Set
    // IP Address the histogrammer is receiving raw data on
    hexitec.def("getAccelRXAddr", [](XDmaHexitec &self, int core)
    {
        // RX UDP cores are in RX mode, so dest/src are swapped
        return self.m_udpCore[core].getSrcIpAddr();
    }, py::arg("core") = 0);
    
    hexitec.def("setAccelRXAddr", [](XDmaHexitec &self, int addr, int core)
    {
        self.m_udpCore[core].setSrcIpAddr(addr);
    }, py::arg("addr"), py::arg("core") = 0);

    hexitec.def("getAccelTXAddr", [](XDmaHexitec &self, int core)
    {
        return self.m_udpTxCore[core].getSrcIpAddr();
    }, py::arg("core") = 0);

    hexitec.def("setAccelTXAddr", [](XDmaHexitec &self, int addr, int core)
    {
        self.m_udpTxCore[core].setSrcIpAddr(addr);
    }, py::arg("addr"), py::arg("core") = 0);

    // Get/Set Ports
    hexitec.def("getSrcPort", [](XDmaHexitec &self)
    {
        return self.m_udpCore[0].getDstPort();
    });
    hexitec.def("setSrcPort", [](XDmaHexitec &self, int port)
    {
        self.m_udpCore[0].setDstPort(port);
    });

    hexitec.def("getAccelPort", [](XDmaHexitec &self)
    {
        return self.m_udpCore[0].getSrcPort();
    });
    hexitec.def("setAccelPort", [](XDmaHexitec &self, int port)
    {
        self.m_udpCore[0].setSrcPort(port);
    });

    hexitec.def("getDestPort", [](XDmaHexitec &self)
    {
        return self.m_udpTxCore[0].getDstPort();
    });
    hexitec.def("setDestPort", [](XDmaHexitec &self, int port)
    {
        self.m_udpTxCore[0].setDstPort(port);
    });


    hexitec.def("saveSpectraHdf5", [](XDmaHexitec &self, 
        char *fname, int chip, 
        int numEng, int firstTF, int numTFSpectra, int numTFMapped,
        bool enbSpectra, bool enbMapped, bool sumChips, std::vector<std::string> comments

    )
    {
        const char **extComment;
        int i = 0;
        for(auto comment : comments)
        {
            extComment[i] = comment.c_str();
            i++;
        }
        
        self.saveSpectraHdf5(fname, chip, numEng, firstTF, numTFSpectra, numTFMapped,
                             enbSpectra, enbMapped, sumChips, extComment);
    });

    hexitec.def("readDataMoverStream", [](XDmaHexitec &self, int qid)
    {
        DataMoverContext context;
        self.readDataMoverStream(&context, qid);
        return context;
    });

    circularHdfWriter.def(py::init<XDmaHexitec&, const char*, int, bool, bool, bool>())
        .def("setupReadoutMode", &CircularHdfWriter::setupReadoutMode)
        // .def("setIpAddr", &CircularHdfWriter::setIpAddr) UNUSED
        .def("start", &CircularHdfWriter::start)
        .def("checkProgress", &CircularHdfWriter::checkProgress)
        .def("getMappedOverRuns", &CircularHdfWriter::getMappedOverRuns)
        .def("getSpectraOverRuns", &CircularHdfWriter::getSpectraOverRuns);

    HexitecITfgStat.def(py::init<>())
        .def_readwrite("status", &HexitecITfgStat::status)
        .def_readwrite("inpFrame", &HexitecITfgStat::status)
        .def_readwrite("timeFrame", &HexitecITfgStat::status)
        .def_readwrite("cycles", &HexitecITfgStat::status);

    dmContext.def(py::init<>())
        .def_readonly("raw", &DataMoverContext::raw)
        .def_readonly("readCredit", &DataMoverContext::readCredit)
        .def_readonly("sixteenBitMode", &DataMoverContext::sixteenBitMode)
        .def_readonly("mappedView", &DataMoverContext::mappeView)
        .def_readonly("sumChips", &DataMoverContext::sumChips)
        .def_readonly("tfMode", &DataMoverContext::tfMode)
        .def_readonly("farmIndexMode", &DataMoverContext::farmIndexMode)
        .def_readonly("farmBase", &DataMoverContext::farmBase)
        .def_readonly("farmMask", &DataMoverContext::farmMask)
        .def_readonly("timeFrame", &DataMoverContext::timeFrame)
        .def_readonly("run", &DataMoverContext::run)
        .def_readonly("pixelColEng", &DataMoverContext::pixelColEng)
        .def_readonly("pixelRow", &DataMoverContext::pixelRow)
        .def_readonly("chipCol", &DataMoverContext::chipCol)
        .def_readonly("chipRow", &DataMoverContext::chipRow)
        .def_readonly("packetIndex", &DataMoverContext::packetIndex);

    py::native_enum<HexitecGeneration>(m, "HexitecGeneration", "enum.IntEnum")
        .value("HexitecGenHexitec", HexitecGeneration::HexitecGenHexitec)
        .value("HexitecGenMHz", HexitecGeneration::HexitecGenMHz)
        .finalize();

    py::native_enum<HexitecUdpRxConnection>(m, "HexitecUdpRxConnection", "enum.IntEnum")
        .value("Normal",   HexitecUdpRxConnection::Normal)
        .value("Loopback", HexitecUdpRxConnection::Loopback)
        .value("FromHost", HexitecUdpRxConnection::FromHost)
        .finalize();

    py::native_enum<HexitecITfgMode>(m, "HexitecITfgMode", "enum.IntEnum")
        .value("Immediate",     HexitecITfgMode::Immediate)
        .value("SWFirst",       HexitecITfgMode::SWFirst)
        .value("SWCountedEach", HexitecITfgMode::SWCountedEach)
        .value("SWIncEach",     HexitecITfgMode::SWIncEach)
        .value("SWGated",       HexitecITfgMode::SWGated)
        .value("HWFirst",       HexitecITfgMode::HWFirst)
        .value("HWCountedEach", HexitecITfgMode::HWCountedEach)
        .value("HWIncEach",     HexitecITfgMode::HWIncEach)
        .value("HWGated",       HexitecITfgMode::HWGated)
        .finalize();

    py::native_enum<XDmaHexitec::MappedView>(hexitec, "MappedView", "enum.IntEnum", "Define the spectra readout for a Data Mover")
        .value("Spectra", XDmaHexitec::MappedView::MappedViewSpectra)
        .value("Mapped8", XDmaHexitec::MappedView::MappedViewMapped8)
        .value("Mapped16", XDmaHexitec::MappedView::MappedViewMapped16)
        .finalize();
    
    py::native_enum<XDmaHexitec::AutonomousMode>(hexitec, "AutonomousMode", "enum.IntEnum")
        .value("AutoOff", XDmaHexitec::AutonomousMode::AutoOff)
        .value("AutoTriggerRead", XDmaHexitec::AutonomousMode::AutoTriggerRead)
        .value("AutoTriggerReadAndClear", XDmaHexitec::AutonomousMode::AutoTriggerReadAndClear)
        .finalize();

    py::native_enum<XDmaHexitec::FarmIndexMode>(hexitec, "FarmIndexMode", "enum.IntEnum")
        .value("FarmIndexIncEOF", XDmaHexitec::FarmIndexMode::FarmIndexIncEOF)
        .value("FarmIndexIncEOP", XDmaHexitec::FarmIndexMode::FarmIndexIncEOP)
        .value("FarmIndexFromTF", XDmaHexitec::FarmIndexMode::FarmIndexFromTF)
        .finalize();

    py::native_enum<HexitecLoadSaveBaseLine>(m, "HexitecLoadSaveBaseLine", "enum.IntEnum")
        .value("Request", HexitecLoadSaveBaseLine::Request)
        .value("RequestAndWait", HexitecLoadSaveBaseLine::RequestAndWait)
        .value("UseShortBurst", HexitecLoadSaveBaseLine::UseShortBurst)
        .finalize();

    py::native_enum<HexitecSaveRestore>(m, "HexitecSaveRestore", "enum.IntFlag")
        .value("AbsThresPos",     HexitecSaveRestore::HexitecSaveRestore_AbsThresPos)
        .value("AbsThresNeg",     HexitecSaveRestore::HexitecSaveRestore_AbsThresNeg)
        .value("AbsThres",        HexitecSaveRestore::HexitecSaveRestore_AbsThres)
        .value("MainThresPos",    HexitecSaveRestore::HexitecSaveRestore_MainThresPos)
        .value("MainThresNeg",    HexitecSaveRestore::HexitecSaveRestore_MainThresNeg)
        .value("MainThres",       HexitecSaveRestore::HexitecSaveRestore_MainThres)
        .value("TrigEnable",      HexitecSaveRestore::HexitecSaveRestore_TrigEnable)
        .value("LowThresPos",     HexitecSaveRestore::HexitecSaveRestore_LowThresPos)
        .value("LowThresNeg",     HexitecSaveRestore::HexitecSaveRestore_LowThresNeg)
        .value("LowThres",        HexitecSaveRestore::HexitecSaveRestore_LowThres)
        .value("LinCorr",         HexitecSaveRestore::HexitecSaveRestore_LinCorr)
        .value("CShareEdgePos",   HexitecSaveRestore::HexitecSaveRestore_CShareEdgePos)
        .value("CShareNegNeb",    HexitecSaveRestore::HexitecSaveRestore_CShareNegNeb)
        .value("CShareLPos",      HexitecSaveRestore::HexitecSaveRestore_CShareLPos)
        .value("CShareSpares",    HexitecSaveRestore::HexitecSaveRestore_CShareSpares)
        .value("CShare",          HexitecSaveRestore::HexitecSaveRestore_CShare)
        .value("OutputPixelMask", HexitecSaveRestore::HexitecSaveRestore_OutputPixelMask)
        .value("RequireAll",      HexitecSaveRestore::HexitecSaveRestore_RequireAll)
        .value("All",             HexitecSaveRestore::HexitecSaveRestore_All)
        .finalize();

    py::native_enum<CircWriterReadoutMode>(m, "CircWriterReadoutMode", "enum.IntEnum")
        .value("Unknown", CircWriterReadoutMode::Unknown)
        .value("PolledMemMapped", CircWriterReadoutMode::PolledMemMapped)
        .value("IrqMemMapped", CircWriterReadoutMode::IrqMemMapped)
        .value("AutoUDPThreadPerFrame", CircWriterReadoutMode::AutoUDPThreadPerFrame)
        .value("AutoUDPThreadPerPacket", CircWriterReadoutMode::AutoUDPThreadPerPacket)
        .value("AutoUDPNoTrailer", CircWriterReadoutMode::AutoUDPNoTrailer)
        .finalize();

    py::native_enum<CircWriterUdpTxOnlyMode>(m, "CircWriterUdpTxOnlyMode", "enum.IntEnum")
        .value("TxNormal", CircWriterUdpTxOnlyMode::TxNormal)
        .value("TxOnlyLoop", CircWriterUdpTxOnlyMode::TxOnlyLoop)
        .value("TxOnly1Pass", CircWriterUdpTxOnlyMode::TxOnly1Pass)
        .finalize();

}