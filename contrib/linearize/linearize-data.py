#!/usr/bin/env python3
#
# linearize-data.py: Construct a linear, no-fork version of the chain.
#
# Copyright (c) 2013 The Bitcoin developers
# Copyright (c) 2016-2018 Stealth R&D LLC
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

import json
import struct
import re
import os
import base64
import sys
import hashlib
import datetime
import time
import codecs

from pyHash9 import hash9


# for config file parsing
TRUE_VALUES =  ('true', 'yes', 'TRUE', 'YES', 'True', 'Yes', 1, True)
FALSE_VALUES =  ('false', 'no', 'FALSE', 'NO', 'False', 'No', 0, False)

class ReMap(object):
   def __init__(self, reason, new_hash):
      self.reason = reason
      self.new_hash = new_hash

settings = {}

def getTF(v):
  if v in TRUE_VALUES:
    return True
  if v in FALSE_VALUES:
    return False
  return None

def uint32(x):
    return x & 0xffffffff

def bytereverse(x):
    return uint32(( ((x) << 24) | (((x) << 8) & 0x00ff0000) |
               (((x) >> 8) & 0x0000ff00) | ((x) >> 24) ))

def bufreverse(in_buf):
    out_words = []
    for i in range(0, len(in_buf), 4):
        word = struct.unpack('@I', in_buf[i:i+4])[0]
        out_words.append(struct.pack('@I', bytereverse(word)))
    return ''.join(out_words)

def wordreverse(in_buf):
    out_words = []
    for i in range(0, len(in_buf), 4):
        out_words.append(in_buf[i:i+4])
    out_words.reverse()
    return ''.join(out_words)

def sha256d(blk_hdr):
    hash1 = hashlib.sha256()
    hash1.update(blk_hdr)
    hash1_o = hash1.digest()

    hash2 = hashlib.sha256()
    hash2.update(hash1_o)
    hash2_o = hash2.digest()
    return hash2_o

def calc_hdr_hash(blk_hdr):
    # hash9 returns the hex digest
    return hash9(blk_hdr)

def calc_hash_str_old(blk_hdr):
    hash = calc_hdr_hash(blk_hdr)
    hash = bufreverse(hash)
    hash = wordreverse(hash)
    hash_str = hash.encode('hex')
    return hash_str

def calc_hash_str(blk_hdr):
    return hash9(blk_hdr)

def get_blk_dt(blk_hdr):
    members = struct.unpack("<I", blk_hdr[68:68+4])
    nTime = members[0]
    dt = datetime.datetime.fromtimestamp(nTime)
    dt_ym = datetime.datetime(dt.year, dt.month, 1)
    return (dt_ym, nTime)

def get_block_hashes(settings):
    blkindex = []
    f = open(settings['hashlist'], "r")
    for line in f:
        line = line.rstrip()
        blkindex.append(bytes(line, "utf-8"))

    print("Read %s hashes" % len(blkindex))

    return blkindex

def mkblockset(blkindex):
    blkmap = {}
    for hash in blkindex:
        blkmap[hash] = True
    return blkmap


# Make a lookup file that facilitates random access
# because indices are not guaranteed to be in order.
def mklookup(settings, blkindex):
    """
    settings: from linearize.cfg
    blkindex: list of main chain block indices in order
    """

    max_height = settings['max_height']

    # construct lookup for reference indices that are out of order
    # key: block hash, value: (filename, seek)
    lookup = {}

    # blk0001.dat -> inFn=1, blk0002 -> inFn=2, etc
    inFn = 1
    # file() opened with blk####.dat
    inF = None
    # keeps track of blocks
    blkCount = 0

    while True:

        if (blkCount % 1000) == 0:
            print("Read %s blocks" % blkCount)

        # height is 1 + number of blocks
        if (max_height is not None) and (len(lookup) >= (max_height+1)):
            break

        if inF is None:
            fname = "%s/blk%04d.dat" % (settings['input'], inFn)
            if not (os.path.exists(fname) and os.path.isfile(fname)):
              print("No such file: %s" % fname)
              if blkCount == 0:
                sys.exit(1)
            print("Input file: %s" % fname)
            try:
                inF = open(fname, "rb")
            except IOError:
                print("Done")
                return lookup

        position = (fname, inF.tell())

        inhdr = inF.read(8)
        if (not inhdr or (inhdr[0] == "\0")):
            inF.close()
            inF = None
            inFn = inFn + 1
            continue

        inMagic = inhdr[:4]
        if (inMagic != settings['netmagic']):
            print("Invalid magic: 0x%s" % base64.b16encode(inMagic))
            return lookup
        inLenLE = inhdr[4:]
        su = struct.unpack("<I", inLenLE)
        inLen = su[0]
        rawblock = inF.read(inLen)
        blk_ver = struct.unpack("<i", rawblock[:4])[0]
        if blk_ver < 8:
          header_len = 80
        else:
          header_len = 88
        blk_hdr = rawblock[:header_len]

        hash_str = calc_hash_str(blk_hdr)

        blkCount += 1

        if hash_str not in blkset:
            if settings['verbose']:
              print("Skipping unknown block: %s" % hash_str.decode("utf-8"))
            continue

        lookup[hash_str] = position

    return lookup



def copydata(settings, blkindex, blkset):
    """
    settings: from linearize.cfg
    blkindex: list of main chain block indices in order
    blkset: map of key: main chain block hash, value: irrelevant
            used for lookup in old method
    """

    print("Making lookup")
    # random access to block index
    # key: block hash, value: (filename, seek)
    lookup = mklookup(settings, blkindex)

    print("Writing data")

    # open all the files you need to read data, won't be that many
    # key: filename, value: open file()
    fileset = {}

    # output bootstrap file number
    outFn = 0
    # current output file size
    outsz = 0
    # current output bootstrap file() (opens for write)
    outF = None
    # current output bootstrap file name
    outFname = None
    # keeps track of blocks
    blkCount = 0


    lastDate = datetime.datetime(2000, 1, 1)
    highTS = 1408893517 - 315360000
    timestampSplit = False
    fileOutput = True
    setFileTime = False
    maxOutSz = settings['max_out_sz']
    if 'output' in settings:
        fileOutput = False
    if settings['file_timestamp'] != 0:
        setFileTime = True
    if settings['split_timestamp'] != 0:
        timestampSplit = True

    for (blkCount, hash_str) in enumerate(blkindex):
        fname, fpos = lookup[hash_str]
        if fname in fileset:
            inF = fileset[fname]
        else:
            try:
                inF = open(fname, "rb")
            except IOError:
                print("Suddenly can't read \"%s\". Aborting." % fname)
                return
            fileset[fname] = inF

        inF.seek(fpos)
        inhdr = inF.read(8)
        if (not inhdr or (inhdr[0] == "\0")):
            inF.close()
            inF = None
            inFn = inFn + 1
            print("File \"%s\" changed. Aborting." % fname)
            return

        inMagic = inhdr[:4]
        if (inMagic != settings['netmagic']):
            print("Invalid magic: 0x%s. Aborting" % base64.b16encode(inMagic))
            return

        inLenLE = inhdr[4:]
        su = struct.unpack("<I", inLenLE)
        inLen = su[0]
        rawblock = inF.read(inLen)
        blk_ver = struct.unpack("<i", rawblock[:4])[0]
        if blk_ver < 8:
          header_len = 80
        else:
          header_len = 88
        blk_hdr = rawblock[:header_len]

        hash_str_check = calc_hash_str(blk_hdr)

        if hash_str_check != hash_str:
            _h = hash_str.decode("utf-8")
            print("Block %s unexpectedly changed. Aborting " % _h)
            return

        if not fileOutput and ((outsz + inLen) > maxOutSz):
            outF.close()
            if setFileTime:
                os.utime(outFname, (int(time.time()), highTS))
            outF = None
            outFname = None
            outFn = outFn + 1
            outsz = 0

        (blkDate, blkTS) = get_blk_dt(blk_hdr)
        if timestampSplit and (blkDate > lastDate):
            _h = hash_str.decode("utf-8")
            print("New month %s @ %s" % (blkDate.strftime("%Y-%m"), _h))
            lastDate = blkDate
            if outF:
                outF.close()
                if setFileTime:
                    os.utime(outFname, (int(time.time()), highTS))
                outF = None
                outFname = None
                outFn = outFn + 1
                outsz = 0

        if not outF:
            if fileOutput:
                outFname = settings['output_file']
            else:
                outFname = os.path.join(settings['output'], "blk%05d.dat" % outFn)
            print("Output file: %s" % outFname)
            outF = open(outFname, "wb")

        outF.write(inhdr)
        outF.write(rawblock)
        outsz = outsz + inLen + 8

        blkCount = blkCount + 1
        if blkTS > highTS:
            highTS = blkTS

        if (blkCount % 1000) == 0:
            print("Wrote %s blocks" % blkCount)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: linearize-data.py CONFIG-FILE")
        sys.exit(1)

    f = open(sys.argv[1])
    for line in f:
        # skip comment lines
        m = re.search('^\s*#', line)
        if m:
            continue

        # parse key=value lines
        m = re.search('^(\w+)\s*=\s*(\S.*)$', line)
        if m is None:
            continue
        settings[m.group(1)] = m.group(2)
    f.close()

    if 'max_height' in settings:
        try:
            settings['max_height'] = int(settings['max_height'])
        except (ValueError, TypeError):
            settings['max_height'] = None
    else:
        settings['max_height'] = None
    if 'netmagic' not in settings:
        settings['netmagic'] = '70352205'
    if 'input' not in settings:
        settings['input'] = 'input'
    if 'hashlist' not in settings:
        settings['hashlist'] = 'hashlist.txt'
    if 'file_timestamp' not in settings:
        settings['file_timestamp'] = 0
    if 'split_timestamp' not in settings:
        settings['split_timestamp'] = 0
    if 'max_out_sz' not in settings:
        settings['max_out_sz'] = 1000 * 1000 * 1000
    if 'verbose' in settings:
        settings['verbose'] = getTF(settings['verbose'])
        if settings['verbose'] is None:
            print("Value for \"verbose\" setting should be \"true\"/\"false\"")
            sys.exit(1)

    settings['max_out_sz'] = int(settings['max_out_sz'])
    settings['split_timestamp'] = int(settings['split_timestamp'])
    settings['file_timestamp'] = int(settings['file_timestamp'])
    settings['netmagic'] = codecs.decode(settings['netmagic'], 'hex')

    if 'output_file' not in settings and 'output' not in settings:
        print("Missing output file / directory")
        sys.exit(1)

    blkindex = get_block_hashes(settings)
    print("Length of block index: %d" % len(blkindex))
    blkset = mkblockset(blkindex)


    if "hash_genesis" in settings:
      hash_genesis = settings['hash_genesis']
    else:
      hash_genesis = "1aaa07c5805c4ea8aee33c9f16a057215bc06d59f94fc12132c6135ed2d9712a"

    hash_genesis = bytes(hash_genesis, "utf-8")
    if not hash_genesis in blkset:
        print("hash %s not found" % hash_genesis.decode("utf-8"))
    else:
        copydata(settings, blkindex, blkset)


