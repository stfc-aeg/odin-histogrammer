import py_hexitec


hexitec = py_hexitec.XDmaHexitec(0, 0, 0, 0)

print(hexitec.getNumChips())

# run live
hexitec.setGlobReg()