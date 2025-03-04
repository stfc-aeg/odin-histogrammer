import logging
import py_hexitec
import time

from .hexitecDefines import HexitecDefines


class TestRunInfo():
    def __init__(self, options) -> None:

        self.from_host = options.get('from_host') == "true"
        self.hold_baseline = options.get('hold_baseline') == "true"
        self.mask_rhs = options.get('mask_rhs') == "true"
        self.no_clear = options.get('no_clear') == "true"
        self.run_circular = options.get('run_circular') == "true"
        self.enb_dither = options.get('enb_dither') == "true"
        self.manual_wait = options.get('manual_wait') == "true"
        self.send_udp_any = options.get('send_udp_any') == "true"
        self.send_udp_frames = options.get('send_udp_frames') == "true"
        self.send_udp_dist = options.get('send_udp_dist') == "true"
        self.send_udp_no_clear = options.get('set_udp_no_clear') == "true"
        self.send_from0 = options.get('send_from0') == "true"
        self.print_fifo = options.get('print_fifo') == "true"

        self.baseline = options.get("baseline", 0)


class Hexitec():

    def __init__(self, runInfo: TestRunInfo, config_options) -> None:
        
        self.hexitec = None  # py_hexitec.XDmaHexitec(useQDma, busNum, devNum, funcNum)

        self.useQdma = config_options.get('useqdma', False)
        self.busNum = int(config_options.get('busnum', 0))
        self.devNum = int(config_options.get('devnum', 0))
        self.funcNum = int(config_options.get('funcnum', 0))

        self.runInfo = runInfo

        self.runTimer = 1

        self.status = "disconnected"

        self.bsubMask = self.get_define_from_config(config_options, "bsubmask", HexitecDefines.BSUB_MASK_MAIN)
        self.bsubDivide = self.get_define_from_config(config_options, "bsubdivide", HexitecDefines.BSUB_DIVIDE2048)
        self.mainThres = [-35, 35]
        self.lowThres = [-25, 25]
        self.absThres = [0, 600]
        self.clusterMode = self.get_define_from_config(config_options, "clustermode", HexitecDefines.CLUSTER_MODE_POS)
        self.clusterType = self.get_define_from_config(config_options, "clustertype", HexitecDefines.CLUSTER_ENB_ALL)
        self.histFormat = self.get_define_from_config(config_options, "hist_format", HexitecDefines.HIST_FORMAT_RUN10)
        self.mappedMode = self.get_define_from_config(config_options, "mappedmode", HexitecDefines.HIST_MAPPED_MODE_OFF)

        self.clusterMode_options = {
            "independant": HexitecDefines.CLUSTER_MODE_INDEP,
            "lone": HexitecDefines.CLUSTER_MODE_LONE_POS,
            "lone_pos_neg": HexitecDefines.CLUSTER_MODE_LONE_POSNEG,
            "positive": HexitecDefines.CLUSTER_MODE_POS,
            "positive_negative": HexitecDefines.CLUSTER_MODE_POSNEG,
            "low": HexitecDefines.CLUSTER_MODE_POSNEG_LOW,
            "auto": HexitecDefines.CLUSTER_MODE_AUTO
        }

        self.clusterType_options = {
            "lone": HexitecDefines.CLUSTER_ENB_LONE,
            "hoz": HexitecDefines.CLUSTER_ENB_HOZ,
            "hoz_nl": HexitecDefines.CLUSTER_ENB_HOZ_NL,
            "hoz_nr": HexitecDefines.CLUSTER_ENB_HOZ_NR,
            "vert": HexitecDefines.CLUSTER_ENB_VERT,
            "vert_na": HexitecDefines.CLUSTER_ENB_VERT_NA,
            "vert_nb": HexitecDefines.CLUSTER_ENB_VERT_NB,
            "diag1": HexitecDefines.CLUSTER_ENB_DIAG1,
            "diag1_nl": HexitecDefines.CLUSTER_ENB_DIAG1NL,
            "diag1_nr": HexitecDefines.CLUSTER_ENB_DIAG1NR,
            "diag2": HexitecDefines.CLUSTER_ENB_DIAG2,
            "diag2_nl": HexitecDefines.CLUSTER_ENB_DIAG2NL,
            "diag2_nr": HexitecDefines.CLUSTER_ENB_DIAG2NR,
            "l1": HexitecDefines.CLUSTER_ENB_L1,
            "l2": HexitecDefines.CLUSTER_ENB_L2,
            "l3": HexitecDefines.CLUSTER_ENB_L3,
            "l4": HexitecDefines.CLUSTER_ENB_L4,
            "quad": HexitecDefines.CLUSTER_ENB_QUAD,
            "edge_pos": HexitecDefines.CLUSTER_ENB_EDGE_POS,
            "diag_pos": HexitecDefines.CLUSTER_ENB_DIAG_POS,
            "edge_neg": HexitecDefines.CLUSTER_ENB_EDGE_NEG,
            "diag_neg": HexitecDefines.CLUSTER_ENB_DIAG_NEG,
            "clust2": HexitecDefines.CLUSTER_ENB_CLUST2,
            "clust3": HexitecDefines.CLUSTER_ENB_CLUST3,
            "all": HexitecDefines.CLUSTER_ENB_ALL
        }

        self.mappedMode_options = {
            "off": HexitecDefines.HIST_MAPPED_MODE_OFF,
            "intl": HexitecDefines.HIST_MAPPED_MODE_INTL,
            "only": HexitecDefines.HIST_MAPPED_MODE_ONLY
        }

        self.bsubMask_options = {
            "none": HexitecDefines.BSUB_MASK_NONE,
            "main": HexitecDefines.BSUB_MASK_MAIN,
            "low": HexitecDefines.BSUB_MASK_LOW,
            "fixed": HexitecDefines.BSUB_MASK_FIXED,
            "main_or4neb": HexitecDefines.BSUB_MASK_MAIN_OR4NEB,
            "main_or8neb": HexitecDefines.BSUB_MASK_MAIN_OR8NEB,
            "low_or4neb": HexitecDefines.BSUB_MASK_LOW_OR4NEB,
            "low_or8neb": HexitecDefines.BSUB_MASK_LOW_OR8NEB,
            "main_or_low_and4neb": HexitecDefines.BSUB_MASK_MAIN_OR_LOW_AND4NEB,
            "main_or_low_and8neb": HexitecDefines.BSUB_MASK_MAIN_OR_LOW_AND4NEB
        }

        self.bsubDivide_options = {
            "256": HexitecDefines.BSUB_DIVIDE256,
            "512": HexitecDefines.BSUB_DIVIDE512,
            "1024": HexitecDefines.BSUB_DIVIDE1024,
            "2048": HexitecDefines.BSUB_DIVIDE2048,
            "4096": HexitecDefines.BSUB_DIVIDE4096,
            "8192": HexitecDefines.BSUB_DIVIDE8192,
            "16384": HexitecDefines.BSUB_DIVIDE16384,
            "32768": HexitecDefines.BSUB_DIVIDE32768,
            "65536": HexitecDefines.BSUB_DIVIDE65536,
        }

        self.histFormat_bins_options = {
            "4096": HexitecDefines.HIST_SHIFT_ENG12,
            "2048": HexitecDefines.HIST_SHIFT_ENG11,
            "1024": HexitecDefines.HIST_SHIFT_ENG10,
            "512": HexitecDefines.HIST_SHIFT_ENG9,
            "256": HexitecDefines.HIST_SHIFT_ENG8,
            "128": HexitecDefines.HIST_SHIFT_ENG7
        }

        self.histFormat_runMode_options = {
            "normal": (0 << 3),
            "eng_only": (1 << 3),
            "eng_cc_pos": (2 << 3),
            "eng_cc": (3 << 3),
            "calib": (4 << 3),
            "eng_cg_pos": (6 << 3)
        }

        self.circ_writer = None
        self.circ_num_threads = 8
        self.inter_frame_gap = 4095
        self.use_arp = False

        self.mapped_thres = -1
        self.mapped_fname = None

        self.linear_offset = 0

        self.num_rows = int(config_options.get("num_rows", 80))
        self.num_cols = int(config_options.get("num_cols", 80))

        self.lin_offset = 0.0

        self.itfg_input_frames = 0
        self.itfg_output_frames = 1
        self.itfg_cycles = 1

        self.chip_select = -1  # -1 means all chips

        self.pos_cshare_filename = None
        self.pos_cshare_mc_filename = None
        self.pos_cshare_l3_filename = None

        self.gain_asc_filename = None
        self.linearity_asc_filename = None
        self.disable_pixel_trig_asc_filename = None
        self.disable_pixel_out_asc_filename = None

        self.head_ip_addr =     10 << 24 | 0 << 16 | 100 << 8 | 108
        self.accel_rx_ip_addr = 10 << 24 | 0 << 16 | 100 << 8 | 8
        self.head_port = 61648
        self.accel_rx_port = 61649

        self.add_baseline = 0

        self.filenames = {
            "CShareAscii": None,
            "CShareAsciiMC": None,
            "CShareAsciiL3": None,
            "linearityGainHDF5": None,
            "gain_asc": None,

            "save_settings": None,
            "save_hdf": None
        }

        self.settings_files = []

        self.test_accel_tx_ip_addr = 10 << 24 | 0 << 16 | 101 << 8 | 109  # defaults, make them editable at some poitn
        self.test_server_ip_addr = 10 << 24 | 0 << 16 | 101 << 8 | 8

        self.frameCounter = []
        self.rawHitCounter = []

    def connect(self, _=None):
        try:
            self.hexitec = py_hexitec.XDmaHexitec(self.useQdma, self.busNum, self.devNum, self.funcNum)
            self.status = "connected"
        except RuntimeError:
            logging.error("Unable to connect to Device. check bus info:")
            logging.error("BusNum: %d, DevNum: %d, FuncNum: %d", self.busNum, self.devNum, self.funcNum)

    def setup_run(self, _=None):
        if not self.hexitec:
            return
        self.status = "configuring"

        self.hexitec.setGlobReg(HexitecDefines.GLB_RUN_REG, 0)

        useAbsTrig = not (self.bsubMask == HexitecDefines.BSUB_MASK_FIXED)  # if bsubMask is FIXED, can't use AbsTrigger
        self.hexitec.setBaselineMode(self.chip_select, self.bsubMask, self.bsubDivide, self.runInfo.enb_dither, useAbsTrig)

        if not self.runInfo.hold_baseline:
            self.hexitec.setPixelLUT(self.chip_select, HexitecDefines.REGION_BASELINE, 0, self.num_rows, 0, self.num_cols, 0)

        self.setup_trigger_thresholds()
        self.setup_linearity()
        self.setup_LUTS()
        self.load_settings(None)
        self.setup_cluster(self.clusterMode, self.clusterType)

        self.hexitec.setHistFormat(self.chip_select, self.histFormat, HexitecDefines.HIST_MAPPED_MODE_OFF)
        self.status = "connected"

    def do_run(self, _=None):
        self.status = "running"

        fixed_baseline = 0
        rewrite_baseline = 0
        useAbsTrig = not (self.bsubMask == HexitecDefines.BSUB_MASK_FIXED)  # if bsubMask is FIXED, can't use AbsTrigger

        if self.mapped_thres >= 0:
            self.mappedMode = HexitecDefines.HIST_MAPPED_MODE_INTL
            self.hexitec.initEngMapThres(self.chip_select, self.mapped_thres)
        
        elif self.mapped_fname:
            self.mappedMode = HexitecDefines.HIST_MAPPED_MODE_INTL
            self.hexitec.loadEngMapAscii(self.chip_select, self.mapped_fname)

        if self.runInfo.from_host:
            connType = py_hexitec.HexitecUdpRxConnection.FromHost
        else:
            connType = py_hexitec.HexitecUdpRxConnection.Normal

        if self.runInfo.mask_rhs:
            self.hexitec.setMainTriggerThres(self.chip_select, 64, 16, 0, 80, self.mainThres[1], self.mainThres[0], False)

        # loading saved settings

        if self.gain_asc_filename:
            self.hexitec.loadLinearityGainAscii(self.chip_select, self.gain_asc_filename, self.linear_offset)
        if self.linearity_asc_filename:
            self.hexitec.loadLinearityAscii(self.chip_select, self.linearity_asc_filename)
        if self.disable_pixel_trig_asc_filename:
            self.hexitec.loadBadPixelsTrigAscii(self.disable_pixel_trig_asc_filename)
        if self.disable_pixel_out_asc_filename:
            self.hexitec.loadBadPixelsOutputAscii(self.disable_pixel_out_asc_filename)
        

        self.hexitec.setHistFormat(self.chip_select, self.histFormat, self.mappedMode)

        self.hexitec.setGlobReg(HexitecDefines.GLB_DATA_PATH, HexitecDefines.DATA_PATH_ENB_FLUSH)
        self.hexitec.setRxEthernetLoopback(0)
        self.hexitec.udpRxSetup(self.head_ip_addr, self.accel_rx_ip_addr, self.head_port, self.accel_rx_port, connType)

        for i in range(self.hexitec.getNumRxUdp()):
            if self.hexitec.getGeneration() == py_hexitec.HexitecGenHexitec:
                self.hexitec.setRxEthernetReg(i, HexitecDefines.ETHERNET_PM_TICK_REG, 1)
        
        if self.runInfo.send_udp_any or self.runInfo.send_udp_frames or self.runInfo.send_udp_dist:
            self.hexitec.stopDataMoverStreamUDP(0)  # Stop any running data move r(UDP) TX
            self.hexitec.stopDataMoverStreamUDP(1)

        if self.runInfo.baseline & HexitecDefines.BASELINE_ENB:
            fixed_baseline = 0
            if self.runInfo.baseline & HexitecDefines.BASELINE_USE_REF and self.bsubMask == HexitecDefines.BSUB_MASK_FIXED:
                rewrite_baseline = self.add_baseline
            
        else:
            fixed_baseline = self.add_baseline

        if not self.runInfo.hold_baseline:
            self.hexitec.setPixelLUT(self.chip_select, HexitecDefines.REGION_BASELINE,
                                     0, self.num_rows,
                                     0, self.num_cols,
                                     fixed_baseline * self.hexitec.getBsubRefScale())


        # some stuff for ssave file names goes here?

        if self.filenames['save_settings']:
            self.hexitec.saveSettingsHdf5(self.save_settings_filename, self.chip_select,
                                          py_hexitec.HexitecSaveRestore_All)

        if self.runInfo.baseline & HexitecDefines.BASELINE_ENB:
            pass  # TEST BASELINE SETTLE LIVE GOES HERE
            logging.error("BASELINE TESTING NOT IMPLEMENTED")
        else:
            logging.debug("STARTING SOME FORM OF RUN?")
            # Timing Options:
            # start run. Wait until key press
            # Run for fixed time
            # run using ITFG
            useItfg = False
            if self.itfg_input_frames > 0:
                self.hexitec.iTfgSetup(py_hexitec.SWFirst, 0, 0, 
                                       self.itfg_input_frames,
                                       self.itfg_output_frames,
                                       self.itfg_cycles)
                useItfg = True
            else:
                self.hexitec.iTfgDisable()
        
        if not self.runInfo.hold_baseline:
            self.hexitec.loadBaseline(self.chip_select, py_hexitec.UseShortBurst)
        
        if not self.runInfo.no_clear:
            self.hexitec.clearHistAll()

        self.setup_send_udp()

        self.hexitec.udpResetCounts(False)
        self.hexitec.enableHist()
        if useItfg:
            stat = py_hexitec.HexitecITfgStat()
            prevStat      = 0xFFFFFFFF
            prevInpFrame  = 0xFFFFFFFF
            prevTimeFrame = 0xFFFFFFFF
            prevCycles    = 0xFFFFFFFF

            self.hexitec.iTfgTrigger()
            while True:
                # this is to replicate a do... while loop, we'll break if we need to
                self.hexitec.iTfgReadStatus(stat)
                if (prevStat != stat.status or prevInpFrame != stat.inpFrame or
                    prevTimeFrame != stat.timeFrame or prevCycles != stat.cycles):
                    
                    logging.debug("Status: %08X, inpFrame: %10d, outFrame: %4d, cycles: %4d",
                                  stat.status, stat.inpFrame, stat.timeFrame, stat.cycles)
                prevStat = stat.status
                prevInpFrame = stat.inpFrame
                prevTimeFrame = stat.timeFrame
                prevCycles = stat.cycles
                time.sleep(0.001)  # I continue to be dubious about using time.sleep
                if stat.status & HexitecDefines.ITFG_STAT_FINISHED:
                    break  # aquisition finished, end the loop
            self.stop_run()
        elif self.runTimer > 0:
            logging.debug("Running for %d seconds", self.runTimer)
            time.sleep(self.runTimer)
            self.stop_run()
        else:
            logging.debug("Running until stopped")

            
    def stop_run(self, _=None):
        
        totalHits = 0

        frameToken = self.hexitec.getFlushedFrame()

        self.frameCounter = []
        self.rawHitCounter = []
        for i in range(self.hexitec.getNumChips()):
            self.frameCounter.append(self.hexitec.getGlobReg(HexitecDefines.GLB_FRAME_COUNT + (2*i)))
            self.rawHitCounter.append(self.hexitec.getGlobReg(HexitecDefines.GLB_RAW_HIT_COUNT + (2*i)))

            totalHits += self.rawHitCounter[i]

        inpTimeFrame = self.hexitec.getInpTimeFrame(0)
        udpTimeFrame = inpTimeFrame & 0x7FFFFFFFF

        if frameToken & HexitecDefines.FLUSHED_FRAME_VALID:  # checking flushed frame valid
            lastFlushedFrame = frameToken & HexitecDefines.FLUSHED_FRAME_GET
        else:
            lastFlushedFrame = -1

        self.hexitec.setGlobReg(HexitecDefines.GLB_RUN_REG, 0)  # Turn off Run bit to force flush of hist caches.

        for i in range(self.hexitec.getNumChips()):
            if self.frameCounter[i] > 0:
                logging.debug("Chip %d: Frames = %d, Raw Hits = %d", i, self.frameCounter[i], self.rawHitCounter[i])
        
        status = self.hexitec.getGlobReg(HexitecDefines.GLB_REORDER_STATUS)

        #TODO: print status stuff here

        #TODO: save data stuff here
        if self.filenames['save_hdf'] and self.circ_writer is None:
            logging.debug("Saving Hdf5: %s, mappedMode: %s", self.filenames['save_hdf'], self.mappedMode != HexitecDefines.HIST_MAPPED_MODE_OFF)
            self.hexitec.saveSpectraHdf5(self.filenames['save_hdf'], self.chip_select, -1, 0, 1, 1, True, self.mappedMode != HexitecDefines.HIST_MAPPED_MODE_OFF, True)
        if self.circ_writer:
            curProgress = self.circ_writer.checkProgress(lastFlushedFrame)
            retries = 0
            while curProgress > lastFlushedFrame:
                if curProgress == lastProgress:
                    retries += 1
                    if retries > 10:
                        logging.error("timeout waiting for circular HDf writer to finish")
                        logging.error("current frame: %d, last firmware frame: %d", curProgress, lastFlushedFrame)
                        break
                lastProgress = curProgress
                curProgress = self.circ_writer.checkProgress(lastFlushedFrame)
            
            if self.circ_writer.getMappedOverRuns() > 0 or self.circ_writer.getSpectraOverRuns() > 0:
                logging.error("CircularHdfWriter detected %d mapped overruns and %d spectra overruns",
                              self.circ_writer.getMappedOverRuns(), self.circ_writer.getSpectraOverRuns())
            
            del self.circ_writer
            self.circ_writer = None

        self.status = "completed"

    def setup_trigger_thresholds(self, _=None):
        self.hexitec.setAbsTriggerThres(self.chip_select, 0, self.num_cols, 0, self.num_rows, self.absThres[1], self.absThres[0])
        self.hexitec.setMainTriggerThres(self.chip_select, 0, self.num_cols, 0, self.num_rows, self.mainThres[1], self.mainThres[0], True)
        self.hexitec.setLowerTriggerThres(self.chip_select, 0, self.num_cols, 0, self.num_rows, self.lowThres[1], self.lowThres[0])

    def setup_baseline(self):

        initBaseline = not self.runInfo.hold_baseline
        useAbsTrig = not (self.bsubMask == HexitecDefines.BSUB_MASK_FIXED)  # if bsubMask is FIXED, can't use AbsTrigger

        self.hexitec.setBaselineMode(self.chip_select, self.bsubMask, self.bsubDivide, self.enbDither, useAbsTrig)
        if self.runInfo.baseline & HexitecDefines.BASELINE_ENB:
            fixedBaseline = 0
            if self.runInfo.baseline & HexitecDefines.BASELINE_USE_REF and self.bsubMask == HexitecDefines.BSUB_MASK_FIXED:
                rewriteBaseline = self.add_baseline
        else:
            fixedBaseline = self.add_baseline
 
        if initBaseline:
            self.hexitec.setPixelLUT(self.chip_select, HexitecDefines.REGION_BASELINE,
                                     0, self.num_rows,
                                     0, self.num_cols,
                                     fixedBaseline * self.hexitec.getBsubRefScale())

        if self.runInfo.baseline & HexitecDefines.BASELINE_ENB:
            pass  # testBaselineSettleLive

    def setup_linearity(self):
        self.hexitec.setLinearityOne(self.chip_select, self.linear_offset)
        if self.filenames['linearityGainHDF5']:
            self.hexitec.loadLinearityGainHDF5(self.filenames['linearityGainHDF5'], 40.0)

    def setup_RxEthernet(self):
        pass

    def setup_itfg(self):
        pass

    def setup_LUTS(self):
        self.hexitec.initCShareLUTs(self.chip_select, -1)
        self.hexitec.setCShareMode(self.chip_select, True, True, True)
        self.hexitec.initRecipLUT(self.chip_select, HexitecDefines.REGION_EDGE_POS_RECIP, -1)
        self.hexitec.initRecipLUT(self.chip_select, HexitecDefines.REGION_NEG_NEB_RECIP, -1)
        self.hexitec.initRecipLUT(self.chip_select, HexitecDefines.REGION_L_POS_RECIP, -1)
        self.hexitec.initPixelMask(self.chip_select)

    def load_settings(self, _):
        if self.filenames["CShareAscii"]:
            self.hexitec.loadCShareAscii(self.chip_select, HexitecDefines.REGION_EDGE_POS_M, self.filenames['CShareAscii'])
        if self.filenames["CShareAsciiMC"]:
            self.hexitec.loadCShareAsciiMC(self.chip_select, HexitecDefines.REGION_EDGE_POS_M, self.filenames['CShareAsciiMC'])
        if self.filenames["CShareAsciiL3"]:
            self.hexitec.loadCShareAscii(self.chip_select, HexitecDefines.REGION_L_POS_M, self.filenames['CShareAsciiL3'])

        for settings_file in self.settings_files:
            self.hexitec.loadSettingsHdf5(settings_file, self.chip_select, py_hexitec.HexitecSaveRestore.HexitecSaveRestore_All)

    def setup_cluster(self, clusterMode, clusterEnb):

        self.hexitec.setClusterMode(self.chip_select, clusterMode)

        if clusterEnb == 0:
            clusterEnb = HexitecDefines.CLUSTER_ENB_ALL
        self.hexitec.setClusterTypes(self.chip_select, clusterEnb)

    def setup_circular_writer(self, filename):
        
        if self.hexitec.supportsIrqs():
            readout_mode = py_hexitec.CircWriterReadoutMode.IrqMemMapped
        else:
            readout_mode = py_hexitec.CircWriterReadoutMode.PolledMemMapped

        self.circ_writer = py_hexitec.CircularHdfWriter(self.hexitec, filename, -1, True, True, True)

        self.circ_writer.setupReadoutMode(readout_mode, 1, py_hexitec.CircWriterUdpTxOnlyMode.TxNormal)
        self.circ_writer.start()

    def setup_send_udp(self):
        logging.debug("Setting up UDP to send frames")
        for i in range(8):
            if self.circ_num_threads == 1 << i:
                break
        
        if i == 8:
            logging.error("Unsupported number of UDP RX Threads %d. Must be Power 2", self.runInfo.num_circular_threads)
            return

        farmMask = (1 << i) - 1  # In mapped interleaved mode, with e.g. m_numThreadsRequested==8, 8 threads are used fro spectra and 8 for mappped
                                 # Spectra datamover context used base=0, mask=7, mapped uses base=8, mask=7

        numThreads = self.circ_num_threads

        if self.mappedMode == HexitecDefines.HIST_MAPPED_MODE_INTL:
            numThreads = numThreads * 2
        
        self.hexitec.udpTxSetup(self.test_accel_tx_ip_addr, self.test_server_ip_addr, 0, 0, 0, numThreads, True, self.inter_frame_gap, self.use_arp)

        autoMode = self.hexitec.AutoTriggerReadAndClear
        farmIndexMode = self.hexitec.FarmIndexFromTF

        if self.runInfo.no_clear:
            autoMode = self.hexitec.AutoTriggerRead
        if self.runInfo.send_udp_dist:
            farmIndexMode = self.hexitec.FarmIndexIncEOP

        farmBase = 0
        self.hexitec.disableDataMoverUDPTrailer(False, 0)

        startTF = -1 # start timeframe

        self.hexitec.setGlobReg(HexitecDefines.GLB_RUN_REG, HexitecDefines.RUN_RUN)
        time.sleep(0.001)  # TODO: This sucks i hate using time.sleep find a better solution

        if self.runInfo.send_from0:
            startTF = -1
        else:
            # manual wait stuff here, skipping
            token = self.hexitec.getFlushedFrame()
            if token & HexitecDefines.FLUSHED_FRAME_VALID:
                startTF = token & 0xFFFFFFFFF
                logging.debug("Determined Start time Frame = %d from flushed token", startTF)
            else:
                inpTimeFrame = self.hexitec.getInpTimeFrame(0)
                startTF = inpTimeFrame & 0x7FFFFFFFF
                logging.debug("Determined start time frame = %d from UDP Header", startTF)

        logging.debug("MappedMode: %d", self.mappedMode)
        if self.mappedMode != HexitecDefines.HIST_MAPPED_MODE_ONLY:
            logging.debug("Setting up farm mode (port from TF) for spectra, base= 0x%02X, mask= 0x%02X", farmBase, farmMask)
            self.hexitec.startDataMoverStreamUDP(startTF, self.hexitec.MappedViewSpectra, False, True, 0, farmMask, farmBase, autoMode, farmIndexMode)
            farmBase = farmBase + farmMask + 1
        if self.mappedMode != HexitecDefines.HIST_MAPPED_MODE_OFF:
            logging.debug("Setting up farm mode (port from TF) for mapped, base= 0x%02X, mask= 0x%02X", farmBase, farmMask)
            self.hexitec.startDataMoverStreamUDP(startTF, self.hexitec.MappedViewMapped16, False, True, 1, farmMask, farmBase, autoMode, farmIndexMode)
            farmBase = farmBase + farmMask + 1

    def testBaselineSettle(self, numBursts, framesPerBurst, useAbsTrig, mask, divide, rewriteBaseline):
        logging.debug("Testing Baseline Settle")

        fineBursts = 30 if numBursts > 100 else 0
        framesPerBurstFine = 4

        self.hexitec.loadBaseline(self.chip_select, py_hexitec.UseShortBurst)

        for burst in range(numBursts):
            
            nFrames = framesPerBurstFine if burst < fineBursts else framesPerBurst

            if nFrames > 2:
                dataPath = self.hexitec.getGlobReg(HexitecDefines.GLB_DATA_PATH)
                dataPath = dataPath | HexitecDefines.DATA_PATH_SHORT_BURST_MODE
                self.hexitec.setGlobReg(HexitecDefines.GLB_DATA_PATH, dataPath)
                self.hexitec.setGlobReg(HexitecDefines.GLB_FRAME_BURST_LENGTH, nFrames - 2)

                if burst == 0:
                    self.hexitec.setGlobReg(HexitecDefines.GLB_RUN_REG, HexitecDefines.RUN_RUN)
                else:
                    self.hexitec.setGlobReg(HexitecDefines.GLB_RUN_REG, HexitecDefines.RUN_RUN | HexitecDefines.RUN_DIS_RESET_FRAME_COUNT)

                if self.hexitec.getGeneration() == py_hexitec.HexitecGenMhz:
                    while self.hexitec.getGlobReg(HexitecDefines.GLB_SCOPE_STATUS) & HexitecDefines.SCOPE_STAT_RUNNING:
                        pass
                else:
                    pass  # TODO: meant to have some sort of sleep here?

    def get_define_from_config(self, config_options, defineName, default=None):
        if defineName in config_options.keys():
            val = config_options.get(defineName)
            if hasattr(HexitecDefines, val):
                return getattr(HexitecDefines, val)
            else:
                try:
                    return int(val)
                except ValueError:
                    logging.error("%s not in HexitecDefines, and cannot be converted directly to int", val)
                    return default
        else:
            return default