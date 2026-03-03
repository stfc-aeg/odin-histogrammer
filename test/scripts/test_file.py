from histogrammer import lib


hexitec = lib.XDmaHexitec(False, 0, 1, 0)

print(hexitec.getNumChips())
print(lib.__doc__)
print(lib.__file__)
print(lib.__version__)
print(lib.lib_version)