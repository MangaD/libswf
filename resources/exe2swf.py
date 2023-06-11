#!/usr/bin/python

import sys

sig = ["FWS", "CWS", "ZWS"]
exeData = None

def detectPlatform(data):
	if data[0:2].decode("utf-8") == "MZ":
		# Pointer to PE header starts at 0x3C and is 4 bytes long
		pointerPEHeader = data[0x3c:0x40]
		p = int.from_bytes(pointerPEHeader, byteorder='little', signed=False)
		# The signature is 4 bytes long, but we only deal with two, the rest is 0's
		if data[p:p+2].decode("utf-8") != "PE":
			print ("Not an executable file.")
			exit(0)
		else:
			return "windows"
	else:
		if data[0] == 0x7F and data[1:4].decode("utf-8") == "ELF":
			return "linux"

def win_extractSWF():
	footerOffset = exeData.find(b"\x56\x34\x12\xFA")
	while footerOffset > 0:
		lengthBytes = exeData[footerOffset+4:footerOffset+8]
		swfLength = int.from_bytes(lengthBytes, byteorder='little', signed=False)
		if len(exeData) - footerOffset == 8:
			break
		footerOffset = exeData.find(b"\x56\x34\x12\xFA", footerOffset+4)

	if footerOffset < 0:
		print ("Failed to detect SWF file.")
		exit(0)

	exeLength = len(exeData)
	swfStart = len(exeData) - swfLength - 8
	swfEnd = swfStart + swfLength

	if exeData[swfStart:swfStart+3].decode("utf-8") not in sig:
		print ("Failed to detect SWF file.")
		exit(0)

	with open(out_file, "wb") as out:
		out.write(exeData[swfStart:swfEnd])

def lin_extractSWF():
	footerOffset = exeData.find(b"\x56\x34\x12\xFA")
	while footerOffset > 0:
		try:
			if exeData[footerOffset+4:footerOffset+7].decode("utf-8") in sig:
				break
		except:
			pass
		footerOffset = exeData.find(b"\x56\x34\x12\xFA", footerOffset+4)

	if footerOffset < 0:
		print ("Failed to detect SWF file.")
		exit(0)

	with open(out_file, "wb") as out:
		out.write(exeData[footerOffset+4:])



def printHelp():
	print("Usage:")
	print(sys.argv[0] + " <exe> <out_swf>")


if len(sys.argv) < 3:
	printHelp()
else:
	in_file = sys.argv[1]
	out_file = sys.argv[2]

	with open(in_file, "rb") as projector:
		exeData = bytearray(projector.read())

	platform = detectPlatform(exeData)

	if platform == "windows":
		win_extractSWF()
	else:
		lin_extractSWF()
