
#ifndef STEALTH_TEXT_H
#define STEALTH_TEXT_H

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <string>

#include <vector>

// crypto++ headers
#include "cryptopp/cryptlib.h"

#include "cryptopp/aes.h"
using CryptoPP::AES;


// KEY_SIZE and IV_SIZE
const int TAG_SIZE = 16;  // bouncycastle default
const int KEY_SIZE = 32;  // SHA256 for key deriv
const int IV_SIZE = AES::BLOCKSIZE;


void SHAKD(std::string twofa, std::string iv_str, byte key[KEY_SIZE]);
bool decryptstealthtxt(std::string msg64,
                       std::string twofa,
                       std::vector<std::string> &txdescr);


#endif   // STEALTH_TEXT
