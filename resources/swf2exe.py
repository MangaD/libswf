#!/usr/bin/python

# MANY THANKS TO https://github.com/devil-tamachan/tama-ieeye/tree/master/SWF2EXE FOR SHOWING ME THE ALGORITHM

import struct
import sys

projectorData = None
swfData = None
footer = bytearray(b'\x56\x34\x12\xFA')

def swf2exe_win(in_file, out_file, projector):
	with open(projector, "rb") as proj:
		projectorData = proj.read()

	with open(in_file, "rb") as swf:
		swfData = swf.read()

	with open(out_file, "wb") as out:
		out.write(projectorData)
		out.write(swfData)
		out.write(footer)
		out.write(struct.pack("i", len(swfData)))

def swf2exe_lin(in_file, out_file, projector):
	with open(projector, "rb") as proj:
		projectorData = proj.read()

	with open(in_file, "rb") as swf:
		swfData = swf.read()

	with open(out_file, "wb") as out:
		out.write(projectorData)
		out.write(struct.pack("i", len(swfData)))
		out.write(footer)
		out.write(swfData)


def printHelp():
	print("Usage:")
	print(sys.argv[0] + " <windows|linux> <swf> <out_exe> <projector>")


if len(sys.argv) < 5:
	printHelp()
else:
	if sys.argv[1] == "windows":
		swf2exe_win(sys.argv[2], sys.argv[3], sys.argv[4])
	elif sys.argv[1] == "linux":
		swf2exe_lin(sys.argv[2], sys.argv[3], sys.argv[4])
	else:
		printHelp()
