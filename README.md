# Hexitec Histogrammer Adapter

An [Odin Control](https://github.com/odin-detector/odin-control) adapter designed to control the histogrammer within the Hexitec Mhz project. This adapter utilises libraries from William Helsby to configure and control the histogrammer firmware. It is designed to work in tandem with [Odin Data](https://github.com/odin-detector/odin-data), sending completed histograms via UDP to an Odin Data Instance for further processing and saving.

## Installation

Installation can be done using [Pip](https://pypi.org/project/pip/) on the cloned repo, or as a dependency in another project.

This project requires access to William Helsby's `det-software` SVN repo to install the libraries required. To do this, the environment variable `DET_SOFTWARE_ROOT` may be set on the command line before attempting installation. Alternatively, the same variable can be set specifically for CMAKE in the `lib/pyproject.toml` file, under `[tool.scikit-build.cmake.define]`. This variable defaults to `usr/lib/det-software`.

The project also requires [HDF5](https://www.hdfgroup.org/solutions/hdf5/) libraries be installed on the system and finable by standard CMAKE methods.

## Configuration

Various settings can be configured from a config file provided to Odin Control:


| Setting Name | Default Value | Description |
| ---:         | ---           | ---         |
| dev_num    | 0             | The XDMA device number of the Alveo Card |
| bus_num    | 0             | The PCIe Bus Number of the Alveo Card |
| func_num   | 0             | The PCIe Function number of the Alveo Card |
| source_ip    | default | The IP Address for the data source.<br> *default* Uses built in default values from the library |
| accel_rx_ip  | default | The receiving IP Address of the Alveo Card.<br> *default* Uses built in default values from the library |
| accel_tx_ip  | default | The sending IP Address of the Alveo Card.<br> *default* Uses built in default values from the library |
| dest_ip      | default | The IP Address completed histograms will be send to.<br> *default* Uses built in default values from the library |
| source_port | 0 | The Port number for the data source.<br>*0* uses built in default values |
| accel_port  | 0 | The Port number for the Alveo Card.<br>*0* uses built in default values |
| dest_port   | 0 | The Port number for the Odin Data instance completed histograms should be sent to. <br>*0* uses built in default values |
| config_dir | test/config/files | The directory containing the various extra config files that can be loaded into the Histogrammer software, such as Bad Pixel Masking and Gain Correction
| acq_mode  | count frames | Define how to control the length of an acquisition. Options are **count frames**, * for ITFG mode, **timed** to run for a set number of seconds, and **continuous** to run until interrupted.
| output_mode | UDP | Define how to output the Histograms, either via **UDP** or to a **HDF5** File for debugging purposes.
| run_timer | 0 | Number of seconds to run the Acquisition for, if the mode is set to do so.
| itfg_input | 2000000 | Number of input frames to use per Histogram if the Acquisition is setup to use the Internal Time Frame Generator
| itfg_output | 20 | Number of Histograms to create, if the Acquisition is set up to use the Internal Time Frame Generator
