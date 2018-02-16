#!/usr/bin/python3

"""
Script to pack .sea and .sed files into .img.

This script packs algo (.sea) and data (.sed) into a single image
(.img) which is then fed to the ecp5-fpga-mgr driver through the kernel
firmware API.

See:

http://wiki.ddg/display/DEV/ecp5-fpga-manager
"""

import argparse
import struct

# instance CLI argument parser
parser = argparse.ArgumentParser(
         description='Make .img file from .sea and .sed files '
                     'for ecp5-fpga-mgr driver')

# configuring argument parser to instance file objects directly by file
# paths from the CLI
parser.add_argument('algo', type=argparse.FileType('rb'),
	help='algo file (usually .sea)')
parser.add_argument('data', type=argparse.FileType('rb'),
	help='data file (usually .sed)')

# this argument is optional
parser.add_argument('-i','--image_file', type=argparse.FileType('wb'),
	default='ecp5_sspi_fw.img', help='result image file name')

# parsing args
args = parser.parse_args()

# get file contents
algo = args.algo.read()
data = args.data.read()

# calculate lengths
algo_size = len(algo)
data_size = len(data)

# packing into a single byte array representing `struct lattice_fpga_sspi_firmware`
# http://repo.ddg/common/sys/drivers/ecp5-sspi/blob/ae1560f3f1cbe617a07fa87d2274039dbed6be32/main.c#L26 
img = struct.pack('<II{}s{}s'.format(algo_size, data_size), algo_size, data_size, algo, data)

# put image to file and close
args.image_file.write(img)
args.image_file.close()


