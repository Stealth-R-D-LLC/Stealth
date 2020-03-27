#! /usr/bin/env python3

import sys
import random
import base64

import pyHash9

if len(sys.argv) == 1:
  quick = True
else:
  quick = not (sys.argv[1] in ["false", "False", "no", "No"])

if quick:
  limit = 10
else:
  limit = 100000

print()

for i in range(limit):
  if (i > 0) and (i % 1000 == 0):
    print(i)
  s = "".join(chr(random.randint(0, 255)) for i in range(100))
  h = pyHash9.hash(s)
  assert len(h) == 32
  he = h.hex()
  h9 = pyHash9.hash9(s).decode('utf8')
  assert h9 == he
  if quick:
    print(he)
    print(h9)
    print()
