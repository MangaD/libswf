#!/usr/bin/python

# Learned from here: https://github.com/OpenGG/swfzip/blob/master/swfzip.py

import sys # argv
import zlib
import pylzma # LZMA SDK
import lzma # XZ Utils

def zlib_compress(inputFile, outputFile):
	swfData = None
	with open(inputFile, "rb") as swf:
		swfData = bytearray(swf.read())

	# Signature, version and file length are not compressed
	tmpdata = bytearray(swfData[0:8])
	tmpdata[0] = ord('C')

	# Verify if SWF version >= 6. If not, put it to 6
	tmpdata[3]  = tmpdata[3]>=6 and tmpdata[3] or 6

	tmpdata += zlib.compress(swfData[8:], 9)
	with open(outputFile, "wb") as out:
		out.write(bytes(tmpdata))


def lzma_compress(inputFile, outputFile, sdk=True):
	swfData = None

	with open(inputFile, "rb") as swf:
		swfData = swf.read()

	# Signature, version and file length are not compressed
	if sdk:
		compressData = pylzma.compress(swfData[8:])
	else:
		compressData = lzma.compress(swfData[8:])

		# https://sourceforge.net/p/lzmautils/discussion/708858/thread/cd04b6ace0/?limit=25#cb9b
		# .lzma format has a 13-byte header (5 bytes for properties, 8 bytes for uncompressed size)
		# followed by the LZMA data. I think the format you want doesn't have the 8-byte
		# uncompressed size field. 8 bytes for uncompressed size
		#compressData = compressData_[:5] + compressData_[13:]

	# 5 accounts for lzma properties
	compressSize = len(compressData) - 5

	tmpdata = bytearray(swfData[0:12])
	tmpdata[0]  = ord('Z')

	# Verify if SWF version >= 13. If not, put it to 13
	tmpdata[3]  = tmpdata[3]>=13 and tmpdata[3] or 13

	size = compressSize.to_bytes(4, byteorder='little')
	tmpdata[8]  = size[0]
	tmpdata[9]  = size[1]
	tmpdata[10] = size[2]
	tmpdata[11] = size[3]

	tmpdata += compressData

	with open(outputFile, "wb") as out:
		out.write(bytes(tmpdata))


def decompress(inputFile, outputFile):
	swfData = None
	with open(inputFile, "rb") as swf:
		swfData = bytearray(swf.read())

	# Signature, version and file length are not compressed
	tmpdata = bytearray(swfData[0:8])

	if tmpdata[0] == ord('C'):
		# zlib
		decompressed_data = zlib.decompress(swfData[8:])
	elif tmpdata[0] == ord('Z'):
		# lzma sdk
		decompressed_data = pylzma.decompress(bytes(swfData[12:]))

		# xz utils
		#decompressed_data = decompress_lzma(bytes(swfData[12:]))
	elif tmpdata[0] == ord('F'):
		print ("SWF file is not compressed.")
		exit(0)
	else:
		print ("Invalid SWF file.")
		exit(0)

	tmpdata[0] = ord('F')

	tmpdata += decompressed_data
	with open(outputFile, "wb") as out:
		out.write(bytes(tmpdata))


def decompress_lzma(data):
	results = []
	len(data)
	while True:
		decomp = lzma.LZMADecompressor(lzma.FORMAT_AUTO, None, None)
		try:
			res = decomp.decompress(data)
		except lzma.LZMAError:
			if results:
				break  # Leftover data is not a valid LZMA/XZ stream; ignore it.
			else:
				raise  # Error on the first iteration; bail out.
		results.append(res)
		data = decomp.unused_data
		if not data:
			break
		if not decomp.eof:
			raise lzma.LZMAError("Compressed data ended before the end-of-stream marker was reached")
	return b"".join(results) 


def printHelp():
	print("Usage:")
	print(sys.argv[0] + " compress <zlib|lzma_sdk|lzma_xz> <raw_swf> <out_swf>")
	print(sys.argv[0] + " decompress <compressed_swf> <out_swf>")


if len(sys.argv) < 2:
	printHelp()
else:
	if sys.argv[1] == "compress":
		if len(sys.argv) < 5:
			printHelp()
		elif sys.argv[2] == "zlib":
			zlib_compress(sys.argv[3], sys.argv[4])
		elif sys.argv[2] == "lzma_sdk":
			lzma_compress(sys.argv[3], sys.argv[4])
		elif sys.argv[2] == "lzma_xz":
			lzma_compress(sys.argv[3], sys.argv[4], False)
		else:
			printHelp()
	elif sys.argv[1] == "decompress":
		if len(sys.argv) < 4:
			printHelp()
		else:
			decompress(sys.argv[2], sys.argv[3])
	else:
		printHelp()
