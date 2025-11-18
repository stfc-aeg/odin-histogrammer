#include <pybind11/pybind11.h>
#include "xdma_hexitec.h"
// #include "circular_hdf_writer.h"

#include <stdint.h>
#include <iostream>
#include <string>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

#define HEXITEC_GLB_RUN_REG					1

#define HEXITEC_HIST_MAPPED_MODE_OFF		0
#define HEXITEC_HIST_MAPPED_MODE_ONLY		1
#define HEXITEC_HIST_MAPPED_MODE_INTL		2


namespace py = pybind11;

// uint32_t* getPointerFromList(py::list list) {
//     return &list;
// }

PYBIND11_MODULE(_core, m, py::mod_gil_not_used(), py::multiple_interpreters::per_interpreter_gil()) {
    m.doc() = R"pbdoc(
        Hexitec Python Binding
        ----------------------
        )pbdoc";
    py::class_<XDmaHexitec> hexitec(m, "XDmaHexitec");
    // py::class_<CircularHdfWriter> circularHdfWriter(m, "CircularHdfWriter");
    py::class_<HexitecITfgStat> HexitecITfgStat(m, "HexitecITfgStat");

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif

    hexitec.def(py::init<int, int, int, int>())
        .def("getNumChips", &XDmaHexitec::getNumChips)
        .def("getNumChipCols", &XDmaHexitec::getNumChipCols)
        .def("getNumChipRows", &XDmaHexitec::getNumChipRows)
        .def("getMaxAdcValue", &XDmaHexitec::getMaxAdcValue)
        .def("getBsubRefScale", &XDmaHexitec::getBsubRefScale)
        .def("getNumHBMPorts", &XDmaHexitec::getNumHBMPorts)
        .def("getNumProcCol", &XDmaHexitec::getNumProcCol)
        .def("getHasFIFOMon", &XDmaHexitec::getHasFIFOMon)
        .def("getGeneration", &XDmaHexitec::getGeneration)
        .def("getMaxBitsClustGrade", &XDmaHexitec::getMaxBitsClustGrade)
        .def("getRegionMask", &XDmaHexitec::getRegionMask)
        .def("getNumPbDma", &XDmaHexitec::getNumPbDma)
        .def("getNumScopeDma", &XDmaHexitec::getNumScopeDma)
        .def("getNBitsAddrPWLin", &XDmaHexitec::getNBitsAddrPWLin)
        .def("getNumRxUdp", &XDmaHexitec::getNumRxUdp)
        .def("getNumTxUdp", &XDmaHexitec::getNumTxUdp)
        .def("writeChipRegs", &XDmaHexitec::writeChipRegs)
        .def("readChipRegs", &XDmaHexitec::readChipRegs)
        .def("setChipReg", &XDmaHexitec::setChipReg)
        .def("getChipReg", &XDmaHexitec::getChipReg)
        .def("writeGlobRegs", &XDmaHexitec::writeGlobRegs)
        .def("readGlobRegs", &XDmaHexitec::readGlobRegs)
        .def("setGlobReg", &XDmaHexitec::setGlobReg)
        .def("getGlobReg", &XDmaHexitec::getGlobReg)
        .def("getGlobReg64", &XDmaHexitec::getGlobReg64)
        .def("setPixelLUT", &XDmaHexitec::setPixelLUT)
        .def("writePixelLUT", &XDmaHexitec::writePixelLUT)
        .def("readPixelLUT", &XDmaHexitec::readPixelLUT)
        .def("setPixelLin", &XDmaHexitec::setPixelLin)
        .def("writePixelLin", &XDmaHexitec::writePixelLin)
        .def("readPixelLin", &XDmaHexitec::readPixelLin)
        .def("setSharedLUT", &XDmaHexitec::setSharedLUT)
        .def("writeSharedLUT", &XDmaHexitec::writeSharedLUT)
        .def("readSharedLUT", &XDmaHexitec::readSharedLUT)
        .def("initRecipLUT", &XDmaHexitec::initRecipLUT)
        .def("initCShareLUTs", &XDmaHexitec::initCShareLUTs)
        .def("initPixelMask", &XDmaHexitec::initPixelMask)
        .def("setPixelMask", &XDmaHexitec::setPixelMask)
        .def("loadLinearityGainAscii", &XDmaHexitec::loadLinearityGainAscii)
        .def("loadLinearityAscii", &XDmaHexitec::loadLinearityAscii)
        // .def("loadLinearityGainHDF5", &XDmaHexitec::loadLinearityGainHDF5)
        .def("loadCShareAscii", &XDmaHexitec::loadCShareAscii)
        .def("loadCShareAsciiMC", &XDmaHexitec::loadCShareAsciiMC)
        .def("loadEngMapAscii", &XDmaHexitec::loadEngMapAscii)
        .def("initEngMapThres", &XDmaHexitec::initEngMapThres)
        .def("loadBadPixelsTrigAscii", &XDmaHexitec::loadBadPixelsTrigAscii)
        .def("loadBadPixelsOutputAscii", &XDmaHexitec::loadBadPixelsOutputAscii)
        .def("dmaReset", &XDmaHexitec::dmaReset)
        .def("dmaBuildDesc", &XDmaHexitec::dmaBuildDesc)
        .def("dmaBuildPBDesc", &XDmaHexitec::dmaBuildPBDesc)
        .def("dmaStart", &XDmaHexitec::dmaStart)
        .def("dmaStop", &XDmaHexitec::dmaStop)
        .def("dmaReadStatus", &XDmaHexitec::dmaReadStatus)
        .def("dmaReadCurrDesc", &XDmaHexitec::dmaReadCurrDesc)
        .def("dmaReadCurrDescNum", &XDmaHexitec::dmaReadCurrDescNum)
        .def("dmaWaitIdle", &XDmaHexitec::dmaWaitIdle)
        .def("dmaWaitIdleNoExcept", &XDmaHexitec::dmaWaitIdleNoExcept)
        .def("dmaPrintDesc", &XDmaHexitec::dmaPrintDesc)
        .def("getMaxPbFrames", &XDmaHexitec::getMaxPbFrames)
        .def("getMaxScopeFrames", &XDmaHexitec::getMaxScopeFrames)
        .def("getPbFrameBytesAligned", &XDmaHexitec::getPbFrameBytesAligned)
        .def("writeDmaBuff", &XDmaHexitec::writeDmaBuff)
        .def("readDmaBuff", &XDmaHexitec::readDmaBuff)
        .def("setBaselineMode", &XDmaHexitec::setBaselineMode)
        .def("loadBaseline", &XDmaHexitec::loadBaseline)
        .def("waitLoadBaseline", &XDmaHexitec::waitLoadBaseline)
        .def("saveBaseline", &XDmaHexitec::saveBaseline)
        .def("waitSaveBaseline", &XDmaHexitec::waitSaveBaseline)
        .def("setAbsTriggerThres", &XDmaHexitec::setAbsTriggerThres)
        .def("setMainTriggerThres", &XDmaHexitec::setMainTriggerThres)
        .def("setLowerTriggerThres", &XDmaHexitec::setLowerTriggerThres)
        .def("setLinearityRaw", &XDmaHexitec::setLinearityRaw)
        .def("setLinearityOne", &XDmaHexitec::setLinearityOne)
        .def("setClusterMode", &XDmaHexitec::setClusterMode)
        .def("setCShareMode", &XDmaHexitec::setCShareMode)
        .def("setClusterTypes", &XDmaHexitec::setClusterTypes)
        .def("setHistFormat", &XDmaHexitec::setHistFormat)
        .def("getHistFormat", &XDmaHexitec::getHistFormat)
        .def("getnBinsEng", &XDmaHexitec::getnBinsEng)
        .def("getEngLsb10", &XDmaHexitec::getEngLsb10)
        .def("getUsePosn", &XDmaHexitec::getUsePosn)
        .def("getnBinsClustClass", &XDmaHexitec::getnBinsClustClass)
        .def("getnBinsCharac", &XDmaHexitec::getnBinsCharac)
        .def("getNumTF", &XDmaHexitec::getNumTF)
        .def("getNumTFMapped", &XDmaHexitec::getNumTFMapped)
        .def("getEngOnly", &XDmaHexitec::getEngOnly)
        .def("getUseClustGrade", &XDmaHexitec::getUseClustGrade)
        .def("enableHist", &XDmaHexitec::enableHist)
        .def("readHistEngRowColTime", &XDmaHexitec::readHistEngRowColTime)
        .def("readHistEngColRowTime", &XDmaHexitec::readHistEngColRowTime)
        .def("readMappedEngRowColTime", &XDmaHexitec::readMappedEngRowColTime)
        .def("readMappedEngColRowTime", &XDmaHexitec::readMappedEngColRowTime)
        .def("readHistEngRowColCCTime", &XDmaHexitec::readHistEngRowColCCTime)
        .def("readHistEngColRowCCTime", &XDmaHexitec::readHistEngColRowCCTime)
        .def("readMappedEngRowColCCTime", &XDmaHexitec::readMappedEngRowColCCTime)
        .def("readHistEngGlobColRowTime", &XDmaHexitec::readHistEngGlobColRowTime)
        .def("readHistEngGlobColRowCCTime", &XDmaHexitec::readHistEngGlobColRowCCTime)
        .def("readMappedEngGlobColRowTime", &XDmaHexitec::readMappedEngGlobColRowTime)
        .def("readHistEngTime", &XDmaHexitec::readHistEngTime)
        .def("readHistEngCCTime", &XDmaHexitec::readHistEngCCTime)
        .def("readHistEngCalibClass", &XDmaHexitec::readHistEngCalibClass)
        .def("readHistCharac2d", &XDmaHexitec::readHistCharac2d)
        .def("readHistCharac3d", &XDmaHexitec::readHistCharac3d)
        .def("clearHistAll", &XDmaHexitec::clearHistAll)
        .def("clearHistTimeframes", &XDmaHexitec::clearHistTimeframes)
        .def("setDefaultXDmaChan", &XDmaHexitec::setDefaultXDmaChan)
        .def("setDmaDescRWChan", &XDmaHexitec::setDmaDescRWChan)
        .def("getRxEthernetReg", &XDmaHexitec::getRxEthernetReg)
        .def("getRxEthernetReg64", &XDmaHexitec::getRxEthernetReg64)
        .def("setRxEthernetReg", &XDmaHexitec::setRxEthernetReg)
        .def("udpRxTestCreateSockets", &XDmaHexitec::udpRxTestCreateSockets)
        .def("getUdpRxTestSocket", &XDmaHexitec::getUdpRxTestSocket)
        .def("setRxEthernetLoopback", &XDmaHexitec::setRxEthernetLoopback)
        .def("udpRxSetup", &XDmaHexitec::udpRxSetup)
        .def("getMacAddr", &XDmaHexitec::getMacAddr)
        .def("udpResetCounts", &XDmaHexitec::udpResetCounts)
        .def("udpTxTestCreateSockets", &XDmaHexitec::udpTxTestCreateSockets)
        .def("udpTxSetup", &XDmaHexitec::udpTxSetup)
        .def("getUdpTxTestSocket", &XDmaHexitec::getUdpTxTestSocket)
        .def("udpTxTestReadFrame", &XDmaHexitec::udpTxTestReadFrame)
        .def("udpShowRxStatus", &XDmaHexitec::udpShowRxStatus)
        .def("iTfgDisable", &XDmaHexitec::iTfgDisable)
        .def("iTfgTrigger", &XDmaHexitec::iTfgTrigger)
        .def("iTfgSetup", &XDmaHexitec::iTfgSetup)
        .def("iTfgReadStatus", &XDmaHexitec::iTfgReadStatus)
        .def("printClockFrequencies", &XDmaHexitec::printClockFrequencies)
        .def("startDataMoverStream", &XDmaHexitec::startDataMoverStream)
        .def("stopDataMoverStreamUDP", &XDmaHexitec::stopDataMoverStreamUDP)
        .def("startDataMoverStreamUDP", &XDmaHexitec::startDataMoverStreamUDP)
        .def("startDataMoverEvList", &XDmaHexitec::startDataMoverEvList)
        .def("disableDataMoverUDPTrailer", &XDmaHexitec::disableDataMoverUDPTrailer)
        .def("clearDataMoverOverRun", &XDmaHexitec::clearDataMoverOverRun)
        .def("getDataMoverOverRun", &XDmaHexitec::getDataMoverOverRun)
        .def("readDataMoverStream", &XDmaHexitec::readDataMoverStream)
        .def("getDataMoverUDPIndex", &XDmaHexitec::getDataMoverUDPIndex)
        .def("saveSpectraAsc", &XDmaHexitec::saveSpectraAsc)
        .def("saveSpectraDet", &XDmaHexitec::saveSpectraDet)
        // .def("saveSpectraHdf5", &XDmaHexitec::saveSpectraHdf5)
        .def("getFlushedFrame", &XDmaHexitec::getFlushedFrame)
        .def("getBsubMaskName", &XDmaHexitec::getBsubMaskName)
        .def("getDiagnosticCounters", py::overload_cast<int, uint32_t *, uint32_t*>(&XDmaHexitec::getDiagnosticCounters))
        .def("getDiagnosticCounters", py::overload_cast<uint32_t *, uint32_t*>(&XDmaHexitec::getDiagnosticCounters))
        .def("writeIrqEnable", &XDmaHexitec::writeIrqEnable)
        .def("getIrqEnable", &XDmaHexitec::getIrqEnable)
        .def("setIrqEnable", &XDmaHexitec::setIrqEnable)
        .def("clearIrqEnable", &XDmaHexitec::clearIrqEnable)
        .def("getEventFd", &XDmaHexitec::getEventFd)
        .def("supportsIrqs", &XDmaHexitec::supportsIrqs)
        .def("getOneFIFOCounts", &XDmaHexitec::getOneFIFOCounts)
        .def("getAllFIFOCounts", &XDmaHexitec::getAllFIFOCounts)
        .def("getInpTimeFrame", &XDmaHexitec::getInpTimeFrame)
        .def("setClusterGradeReg", &XDmaHexitec::setClusterGradeReg)
        .def("setClusterGrade", &XDmaHexitec::setClusterGrade)
        .def("getClusterGrade", &XDmaHexitec::getClusterGrade)
        // .def("saveSettingsHdf5", &XDmaHexitec::saveSettingsHdf5)
        // .def("loadSettingsHdf5", &XDmaHexitec::loadSettingsHdf5)
        .def("writePixelMask", &XDmaHexitec::writePixelMask)
        .def("readPixelMask", &XDmaHexitec::readPixelMask)
        ;

    // circularHdfWriter.def(py::init<XDmaHexitec&, const char*, int, bool, bool, bool>())
    //     .def("setupReadoutMode", &CircularHdfWriter::setupReadoutMode)
    //     .def("setIpAddr", &CircularHdfWriter::setIpAddr)
    //     .def("start", &CircularHdfWriter::start)
    //     // .def("stop", &CircularHdfWriter::stop) not declared
    //     .def("checkProgress", &CircularHdfWriter::checkProgress)
    //     .def("getMappedOverRuns", &CircularHdfWriter::getMappedOverRuns)
    //     .def("getSpectraOverRuns", &CircularHdfWriter::getSpectraOverRuns);

    HexitecITfgStat.def(py::init<>())
        .def_readwrite("status", &HexitecITfgStat::status)
        .def_readwrite("inpFrame", &HexitecITfgStat::status)
        .def_readwrite("timeFrame", &HexitecITfgStat::status)
        .def_readwrite("cycles", &HexitecITfgStat::status);

    py::enum_<HexitecGeneration>(m, "HexitecGeneration")
        .value("HexitecGenHexitec", HexitecGeneration::HexitecGenHexitec)
        .value("HexitecGenMHz", HexitecGeneration::HexitecGenMHz)
        .export_values();

    py::enum_<HexitecUdpRxConnection>(m, "HexitecUdpRxConnection")
        .value("Normal",   HexitecUdpRxConnection::Normal)
        .value("Loopback", HexitecUdpRxConnection::Loopback)
        .value("FromHost", HexitecUdpRxConnection::FromHost)
        .export_values();

    py::enum_<HexitecITfgMode>(m, "HexitecITfgMode")
        .value("Immediate",     HexitecITfgMode::Immediate)
        .value("SWFirst",       HexitecITfgMode::SWFirst)
        .value("SWCountedEach", HexitecITfgMode::SWCountedEach)
        .value("SWIncEach",     HexitecITfgMode::SWIncEach)
        .value("SWGated",       HexitecITfgMode::SWGated)
        .value("HWFirst",       HexitecITfgMode::HWFirst)
        .value("HWCountedEach", HexitecITfgMode::HWCountedEach)
        .value("HWIncEach",     HexitecITfgMode::HWIncEach)
        .value("HWGated",       HexitecITfgMode::HWGated)
        .export_values();

    py::enum_<XDmaHexitec::MappedView>(hexitec, "MappedView")
        .value("MappedViewSpectra", XDmaHexitec::MappedView::MappedViewSpectra)
        .value("MappedViewMapped8", XDmaHexitec::MappedView::MappedViewMapped8)
        .value("MappedViewMapped16", XDmaHexitec::MappedView::MappedViewMapped16)
        .export_values();
    
    py::enum_<XDmaHexitec::AutonomousMode>(hexitec, "AutonomousMode")
        .value("AutoOff", XDmaHexitec::AutonomousMode::AutoOff)
        .value("AutoTriggerRead", XDmaHexitec::AutonomousMode::AutoTriggerRead)
        .value("AutoTriggerReadAndClear", XDmaHexitec::AutonomousMode::AutoTriggerReadAndClear)
        .export_values();

    py::enum_<XDmaHexitec::FarmIndexMode>(hexitec, "FarmIndexMode")
        .value("FarmIndexIncEOF", XDmaHexitec::FarmIndexMode::FarmIndexIncEOF)
        .value("FarmIndexIncEOP", XDmaHexitec::FarmIndexMode::FarmIndexIncEOP)
        .value("FarmIndexFromTF", XDmaHexitec::FarmIndexMode::FarmIndexFromTF)
        .export_values();

    py::enum_<HexitecLoadSaveBaseLine>(m, "HexitecLoadSaveBaseLine")
        .value("Request", HexitecLoadSaveBaseLine::Request)
        .value("RequestAndWait", HexitecLoadSaveBaseLine::RequestAndWait)
        .value("UseShortBurst", HexitecLoadSaveBaseLine::UseShortBurst)
        .export_values();

    py::enum_<HexitecSaveRestore>(m, "HexitecSaveRestore")
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
        .export_values();

    // py::enum_<CircWriterReadoutMode>(m, "CircWriterReadoutMode")
    //     .value("Unknown", CircWriterReadoutMode::Unknown)
    //     .value("PolledMemMapped", CircWriterReadoutMode::PolledMemMapped)
    //     .value("IrqMemMapped", CircWriterReadoutMode::IrqMemMapped)
    //     .value("AutoUDPThreadPerFrame", CircWriterReadoutMode::AutoUDPThreadPerFrame)
    //     .value("AutoUDPThreadPerPacket", CircWriterReadoutMode::AutoUDPThreadPerPacket)
    //     .value("AutoUDPNoTrailer", CircWriterReadoutMode::AutoUDPNoTrailer)
    //     .export_values();

    // py::enum_<CircWriterUdpTxOnlyMode>(m, "CircWriterUdpTxOnlyMode")
    //     .value("TxNormal", CircWriterUdpTxOnlyMode::TxNormal)
    //     .value("TxOnlyLoop", CircWriterUdpTxOnlyMode::TxOnlyLoop)
    //     .value("TxOnly1Pass", CircWriterUdpTxOnlyMode::TxOnly1Pass)
    //     .export_values();

}