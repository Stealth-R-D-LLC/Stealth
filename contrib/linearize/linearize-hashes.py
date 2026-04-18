#! /usr/bin/env python3
#
# linearize-hashes.py:  List blocks in a linear, no-fork version of the chain.
#
# Copyright (c) 2013 The Bitcoin developers
# Copyright (c) 2026 The Stealth developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

import json
import struct
import re
import base64
import http.client
import sys
import argparse

settings = {}

class BitcoinRPC:
  OBJID = 1

  def __init__(self, host, port, username, password):
    authpair = "%s:%s" % (username, password)
    authb64 = base64.b64encode(authpair.encode()).decode("ascii")
    self.authhdr = "Basic %s" % authb64
    self.conn = http.client.HTTPConnection(host, port, 30)
  def rpc(self, method, params=None):
    self.OBJID += 1
    obj = { 'version' : '1.1',
      'method' : method,
      'id' : self.OBJID }
    if params is None:
      obj['params'] = []
    else:
      obj['params'] = params
    self.conn.request('POST', '/', json.dumps(obj),
      { 'Authorization' : self.authhdr,
        'Content-type' : 'application/json' })

    resp = self.conn.getresponse()
    if resp is None:
      print("JSON-RPC: no response")
      return None

    body = resp.read()
    resp_obj = json.loads(body.decode('utf-8'))
    if resp_obj is None:
      print("JSON-RPC: cannot JSON-decode body")
      return None
    if 'error' in resp_obj and resp_obj['error'] != None:
      return resp_obj['error']
    if 'result' not in resp_obj:
      print("JSON-RPC: no result in object")
      return None

    return resp_obj['result']
  def getblock(self, hash_, verbose=True):
    return self.rpc('getblock', [hash_, verbose])
  def getblockhash(self, index):
    return self.rpc('getblockhash', [index])
  def getblockhash9(self, index):
    return self.rpc('getblockhash9', [index])

def get_block_hashes(settings):
  rpc = BitcoinRPC(settings['host'], int(settings['port']),
       settings['rpcuser'], settings['rpcpassword'])

  start = settings['min_height']
  stop = settings['max_height'] + 1
  total = stop - start
  UPDATE_EVERY = 10000
  for (i, height) in enumerate(range(start, stop)):
    if (i > 0) and ((i % UPDATE_EVERY) == 0):
        if not settings.get('quiet'):
            print("Wrote %d of %d hashes." % (i, total), file=sys.stderr)
    hash_ = rpc.getblockhash9(height)
    print(hash_)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(
              description="List blocks in a linear, no-fork version of the chain.")
  parser.add_argument("config", metavar="CONFIG-FILE",
                                help="Path to configuration file")
  parser.add_argument("-m","--min_height", type=int,
                                           default=None,
                                           help="Override min_height from config")
  parser.add_argument("-M", "--max_height", type=int,
                                            default=None,
                                            help="Override max_height from config")
  parser.add_argument("-q", "--quiet", action="store_true",
                                       help="Suppress progress output to stderr")
  args = parser.parse_args()

  f = open(args.config)
  for line in f:
    # skip comment lines
    m = re.search(r'^\s*#', line)
    if m:
      continue

    # parse key=value lines
    m = re.search(r'^(\w+)\s*=\s*(\S.*)$', line)
    if m is None:
      continue
    settings[m.group(1)] = m.group(2)
  f.close()

  if 'host' not in settings:
    settings['host'] = '127.0.0.1'
  if 'port' not in settings:
    settings['port'] = 46502
  if 'min_height' not in settings:
    settings['min_height'] = 0
  if args.min_height is not None:
    settings['min_height'] = args.min_height
  if args.max_height is not None:
    settings['max_height'] = args.max_height
  if 'max_height' not in settings:
    print("Missing max_height")
    sys.exit(1)
  if 'rpcuser' not in settings or 'rpcpassword' not in settings:
    print("Missing username and/or password in cfg file")
    sys.exit(1)

  settings['port'] = int(settings['port'])
  settings['min_height'] = int(settings['min_height'])
  settings['max_height'] = int(settings['max_height'])
  settings['quiet'] = args.quiet

  get_block_hashes(settings)

