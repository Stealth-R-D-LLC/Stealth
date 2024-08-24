#! /usr/bin/env python3

import sys
import base64
import secrets
import unittest

import pyHash9

DEBUG = False

# Test data was created with `make_test_data`
DATA = {"inputs":
          [
            "Why did they call it Hash9?",
            "c9651c459be1b8e6851e92567a92c6c1a42dcdda8cb0a92e8543a0cbdd5cb5b6",
            "f6890695db4b7d76efecf7706dd62d86ef76446d244d28ad2ff311ff942e76a2",
            "90b410685cbeba0fae65c1eed50a1de29b3e328151c129a98812605c6cb932d3",
            "66f0df8aefe751ce74e203a8c4992c0cec78d9b73f1f32c64b0682b6242cb176",
            "d3460df1588584b13070a7c12b369c99cca3a2a0ed7e5b9bf36505a18274021c",
            "8ce712e6a8f717f916690d3fc2c4fa8b7ff0f84eb376a7189f018bb5c8460b68",
            "438d345a5b33f9b087a6e51cd3beab24d446d40b230e3bca93c7c4bb16e5c15b",
            "782031a14a7601539d4fd5782088b93dc84ddffdda400e5ad3aecc25e22f98fb",
            "0ce2a5cae0bdff1b279aaef006c3a73927f66976eaa243269bfc46ac4cf797df",
            "51a5ecf6224f4247c6be97a496faea6fd1056314983c30d5d4a65a872a5d6d28"
          ],
        "outputs":
          [
            ("314c8dbd325412d2c36411db867b49a73de2b54317d64fa46c70eb631fd7dd3b",
             "3bddd71f63eb706ca44fd61743b5e23da7497b86db1164c3d2125432bd8d4c31"),
            ("51badb5dcdaf90f4b3577fbe1440e5450cd2cd72e1cbfa21c69a0872da3b20f6",
             "f6203bda72089ac621facbe172cdd20c45e54014be7f57b3f490afcd5ddbba51"),
            ("afd779e52e37f0c9aad4bf54a95e70ae0ff0c9d0aabfbb74cf7350d0e0073c98",
             "983c07e0d05073cf74bbbfaad0c9f00fae705ea954bfd4aac9f0372ee579d7af"),
            ("f83fc295ebd3d3ef5153da0deba26757b195c5b2f2edcaaf4ebd46daf799d80d",
             "0dd899f7da46bd4eafcaedf2b2c595b15767a2eb0dda5351efd3d3eb95c23ff8"),
            ("07a287718ebc8a94aa35b81316861361505874502d1ce31ea6aceb7cdc915c81",
             "815c91dc7cebaca61ee31c2d507458506113861613b835aa948abc8e7187a207"),
            ("90d56ede60a5ce6b3f3a0eaa7224d5cb0bdeafa4bcb67f8a1d9357bde8feb06a",
             "6ab0fee8bd57931d8a7fb6bca4afde0bcbd52472aa0e3a3f6bcea560de6ed590"),
            ("5ee33f0241653217e31df89c91e1b862ad82594f5da7dca30cf9b9547143dc25",
             "25dc437154b9f90ca3dca75d4f5982ad62b8e1919cf81de317326541023fe35e"),
            ("85679c2bd7be637433f3d47e54e628b3f83088001716ba53c8bcf46f4c8a6729",
             "29678a4c6ff4bcc853ba1617008830f8b328e6547ed4f3337463bed72b9c6785"),
            ("2bab81c7f0511df307a292755a6cadf0e2c3d698609152bdd34da3a837479284",
             "84924737a8a34dd3bd52916098d6c3e2f0ad6c5a7592a207f31d51f0c781ab2b"),
            ("50f27131efc5b8f2c3095c1a6ab22bd9a716013b3970afea8f9a4376b4e14d61",
             "614de1b476439a8feaaf70393b0116a7d92bb26a1a5c09c3f2b8c5ef3171f250"),
            ("a5e414bb9a90e7e5a2c3167edc3f4efaec75de768b114bac10a2461c749fc802",
             "02c89f741c46a210ac4b118b76de75ecfa4e3fdc7e16c3a2e5e7909abb14e4a5")
          ]
        }


class PyHash9Test(unittest.TestCase):
  DIGEST = 0
  HASH = 1
  def test_digest(self):
    print("Testing `digest`")
    for i, v in enumerate(DATA['inputs']):
      d = pyHash9.digest(v).hex()
      self.assertEqual(d, DATA['outputs'][i][self.DIGEST])
  def test_hash(self):
    print("Testing `hash`")
    for i, v in enumerate(DATA['inputs']):
      d = pyHash9.hash(v).hex()
      self.assertEqual(d, DATA['outputs'][i][self.HASH])
  def test_hash9(self):
    print("Testing `hash9`")
    for i, v in enumerate(DATA['inputs']):
      d = pyHash9.hash9(v).decode("ASCII")
      self.assertEqual(d, DATA['outputs'][i][self.HASH])



def make_test_data():
  inputs = ["Why did they call it Hash9?"]

  outputs = [(
     "314c8dbd325412d2c36411db867b49a73de2b54317d64fa46c70eb631fd7dd3b",
     "3bddd71f63eb706ca44fd61743b5e23da7497b86db1164c3d2125432bd8d4c31")]

  for i in range(10):
    random_hex = secrets.token_bytes(32).hex()
    inputs.append(random_hex)
    hashes = (pyHash9.digest(random_hex).hex(), pyHash9.hash(random_hex).hex())
    outputs.append(hashes)

  return {"inputs": inputs, "outputs": outputs}

def format_test_data(data):
   import io
   filestr = io.StringIO()
   print("data = {\"inputs\":\n          [", file=filestr)
   lines = []
   for value in data['inputs']:
     lines.append("            \"%s\"" % value)
   print(",\n".join(lines) + "\n          ],", file=filestr)
   print("        \"outputs\":\n          [", file=filestr)
   lines = []
   for d, h in data['outputs']:
     lines.append(("            (\"%s\",\n" % d) +
                  ("             \"%s\")" % h))
   print(",\n".join(lines) + "\n          ]\n        }", file=filestr)
   return filestr.getvalue()

if __name__ == "__main__":
  unittest.main()
