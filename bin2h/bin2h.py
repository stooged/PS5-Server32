#!/usr/bin/python3
import os
import sys
import gzip
try:filename = sys.argv[1]
except:sys.exit("file required\n\nExample: bin2h payload.bin")
f = open(filename, 'rb')
bindat = f.read()
f.close()
f = gzip.open(filename + ".gz", 'wb')
f.write(bindat)
f.close()
cnt = 0
sfilename = os.path.basename(filename).replace('.bin', '')
sfilename = os.path.basename(sfilename).replace('.elf', '')
sfilename = os.path.basename(sfilename).replace('.js', '')
sfilename = os.path.basename(sfilename).replace('.css', '')
sfilename = os.path.basename(sfilename).replace('.html', '')
sfilename = os.path.basename(sfilename).replace('.', '_')
tmpdat = "static const uint8_t " + sfilename + "_gz[] PROGMEM = {\n"
with open(filename + ".gz", 'rb') as f:
    chnk = f.read(1)
    while chnk:
        if cnt == 31:
            cnt = 0
            tmpdat = tmpdat + "%s,\n" % ord(chnk)
        else:    
            tmpdat = tmpdat + "%s, " % ord(chnk)
        cnt=cnt+1
        chnk = f.read(1)
if tmpdat.endswith(","):
  tmpdat = tmpdat[:-1]
elif tmpdat.endswith(", "):
  tmpdat = tmpdat[:-2]
elif tmpdat.endswith(",\n"):
  tmpdat = tmpdat[:-2]
tmpdat = tmpdat + "\n};\n"
f.close()
if os.path.exists(sfilename + ".h"):
  os.remove(sfilename + ".h")
f = open(sfilename + ".h", 'w+', encoding="utf-8") 
f.write(tmpdat)
f.close()
if os.path.exists(filename + ".gz"):
  os.remove(filename + ".gz")