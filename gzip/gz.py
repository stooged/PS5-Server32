#!/usr/bin/python3
import os
import sys
import gzip
try:filename = sys.argv[1]
except:sys.exit("file required\n\nExample: gz payload.bin")
f = open(filename, 'rb')
bindat = f.read()
f.close()
f = gzip.open(filename + ".gz", 'wb')
f.write(bindat)
f.close()