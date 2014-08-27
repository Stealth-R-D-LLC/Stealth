
#include "stealthtext.h"

// cryptopp headers

#include "cryptopp/hex.h"
using CryptoPP::HexEncoder;

#include "cryptopp/base64.h"
using CryptoPP::Base64Decoder;

#include "cryptopp/filters.h"
using CryptoPP::Redirector;
using CryptoPP::StringSink;
using CryptoPP::StringSource;
using CryptoPP::AuthenticatedDecryptionFilter;

#include "cryptopp/sha.h"
using CryptoPP::SHA256;
using CryptoPP::HashFilter;

#include "cryptopp/gcm.h"
using CryptoPP::GCM;

#include <boost/algorithm/string.hpp>



// SHA256 Key Derivation
void SHAKD(std::string twofa, std::string iv_str, byte key[KEY_SIZE]) {

        std::string salt;
        StringSource bar(iv_str, true,
               new HexEncoder(new StringSink(salt)));

        SHA256 hash;
        std::string string_key;
        StringSource foo(twofa + salt, true,
            new HashFilter(hash, new StringSink(string_key)));

        // convert string string_key into byte mykey
        for (int idx=0; idx<KEY_SIZE; idx++) {
           key[idx] = string_key[idx];
        }
}


// fill txdescr with address, amount, timestamp, as strings
bool decryptstealthtxt(std::string msg64,
                       std::string twofa,
                       std::vector<std::string> &txdescr) {

        std::string decoded, recovered;
        StringSource(msg64, true,
                new Base64Decoder(
                           new StringSink(decoded))); 

        // AES
        byte myiv[IV_SIZE];
        for (int idx=0; idx<IV_SIZE; idx++) {
            myiv[idx] = decoded[idx];
        }

        // needed to salt key derivation
        std::string iv_str;
        iv_str = decoded.substr(0, IV_SIZE);

	byte mykey[KEY_SIZE];
        SHAKD(twofa, iv_str, mykey);

        std::string ctxt = decoded.substr(IV_SIZE, decoded.size() - IV_SIZE);

        try
        {

            GCM< AES >::Decryption decryptor;
            decryptor.SetKeyWithIV(mykey, sizeof(mykey),
                                       myiv, sizeof(myiv));

            /* CGM ***************************************************/
            AuthenticatedDecryptionFilter df(decryptor,
                new StringSink(recovered),
                AuthenticatedDecryptionFilter::DEFAULT_FLAGS,
                TAG_SIZE
            ); // AuthenticatedDecryptionFilter

            StringSource(ctxt, true,
                new Redirector(df)
            ); // StringSource

            // If the object does not throw, here's the only
            //  opportunity to check the data's integrity
            bool b = df.GetLastResult();
            if (!b) {
                 return false;
             }
        }

        catch(const CryptoPP::Exception& e) {
            return false;
        }

        boost::split(txdescr, recovered, boost::is_any_of(","));

        // might as well check
        if (txdescr.size() != 3) {
          return false;
        }

	return true;
}

