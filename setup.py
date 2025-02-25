from setuptools import setup, find_packages

from pybind11.setup_helpers import Pybind11Extension

import os

__version__ = "0.0.1"

lib_dir = os.environ.get("DET_SOFTWARE", "../libs")
print(lib_dir)

libs = ['xdma_hexitec', 'detfile', 'hdf5_cpp', 'hdf5_hl', 'hdf5', 'img_mod', 'rt']
libdirs = [os.path.join(lib_dir, "none_vme/hexitec/lib/objs.x86_64"),
           os.path.join(lib_dir, "libs/libs.linux.x86_64/lib"),
           os.path.join(lib_dir, "libs/src/HDF5/hdf5-1.14.4-3/hdf5/lib")]
include_dirs = [os.path.join(lib_dir, "none_vme/hexitec/include"),
                os.path.join(lib_dir, "libs/include"),
                os.path.join(lib_dir, "libs/include-hdf5")]
print(libdirs)
ext_modules = [
    Pybind11Extension(
        "py_hexitec",
        sources=["lib/main.cpp"],
        libraries=libs,
        library_dirs=libdirs,
        include_dirs=include_dirs,

        language="c++"
    )
]

setup(
    name="hexitec",
    version=__version__,
    ext_modules=ext_modules,
    packages=find_packages('src'),
    package_dir={"": 'src'}
)