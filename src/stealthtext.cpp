
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

            /////////////////////////////////////////////////////////
            // not essential, used for debugging output
            cout << "salt: " << salt << endl;
            /////////////////////////////////////////////////////////

        SHA256 hash;
        std::string string_key;
        StringSource foo(twofa + salt, true,
            new HashFilter(hash, new StringSink(string_key)));

            /////////////////////////////////////////////////////////
            // not essential, used for debugging output
            std::string hex_key;
            StringSource blah(string_key, true,
                   new HexEncoder(new StringSink(hex_key)));
            cout << "hex of key: " << hex_key << endl;
            cout << "hashed: " << twofa + salt << endl;
            /////////////////////////////////////////////////////////

        // convert string string_key into byte mykey
        for (int idx=0; idx<KEY_SIZE; idx++) {
           key[idx] = string_key[idx];
        }

            /////////////////////////////////////////////////////////////
            // debugging output
            cout << "size of string_key: " << string_key.size() << endl;
            cout << "size of key: " << sizeof(key) << endl;
            /////////////////////////////////////////////////////////////
}


// fill txdescr with address, amount, timestamp, as strings
bool decryptstealthtxt(std::string msg64,
                       std::string twofa,
                       std::vector<std::string> &txdescr) {

        std::string decoded, recovered;
        StringSource(msg64, true,
                new Base64Decoder(
                           new StringSink(decoded))); 

            //////////////////////////////////////////////////////
            // not essential, used for debugging output
            std::string msg16;
            StringSource(decoded, true,
                    new HexEncoder(
                               new StringSink(msg16)));
            cout << "msg64: " << msg64 << endl;
            cout << "msg16: " << msg16 << endl;
            //////////////////////////////////////////////////////

        // AES
        byte myiv[IV_SIZE];
        for (int idx=0; idx<IV_SIZE; idx++) {
            myiv[idx] = decoded[idx];
        }

        // needed to salt key derivation
        std::string iv_str;
        iv_str = decoded.substr(0, IV_SIZE);

            //////////////////////////////////////////////////////
            // not essential, used for debugging output
            std::string hex_iv;
            StringSource(iv_str, true,
                    new HexEncoder(
                             new StringSink(hex_iv)));
            cout << "hex_iv: " << hex_iv << endl;
            //////////////////////////////////////////////////////

	byte mykey[KEY_SIZE];
        SHAKD(twofa, iv_str, mykey);

        std::string ctxt = decoded.substr(IV_SIZE, decoded.size() - IV_SIZE);

            /////////////////////////////////////////////////////////////
            // not essential, used for debugging output
            std::string ct16;
            StringSource(ctxt, true,
                    new HexEncoder(
                               new StringSink(ct16)));
            cout << "ct16: " << ct16 << endl;
            /////////////////////////////////////////////////////////////
      

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


                ///////////////////////////////////////////////////////////
                // not essential, used for debugging output
                cout << "length of recovered: " << recovered.size() << endl;
                cout << "recovered text: " << recovered << endl;
                ///////////////////////////////////////////////////////////

        }

        catch(const CryptoPP::Exception& e) {

            ///////////////////////////////////////////////////////////
            // not essential, used for debugging output
            cout << e.what() << endl;
            ///////////////////////////////////////////////////////////

            return false;
        }

        boost::split(txdescr, recovered, boost::is_any_of(","));

        // might as well check
        if (txdescr.size() != 3) {
          return false;
        }

	return true;
}

