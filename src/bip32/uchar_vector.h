////////////////////////////////////////////////////////////////////////////////
//
// uchar_vector.h
//
// Copyright (c) 2011-2012 Eric Lombrozo
// Copyright (c) 2011-2016 Ciphrex Corp.
//
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.
//

#ifndef UCHAR_VECTOR_H__
#define UCHAR_VECTOR_H__

#include "allocators.h"

#include <stdio.h>
#include <stdint.h>

#include <iostream>
#include <cstring>

#include <vector>
#include <string>
#include <algorithm>

#define UCHAR_VECTOR(x) uchar_vector((x).begin(), (x).end())
#define UCHAR_VECTOR_SECURE(x) uchar_vector_secure((x).begin(), (x).end())

const char g_hexBytes[][3] = {
    "00","01","02","03","04","05","06","07","08","09","0a","0b","0c","0d","0e","0f",
    "10","11","12","13","14","15","16","17","18","19","1a","1b","1c","1d","1e","1f",
    "20","21","22","23","24","25","26","27","28","29","2a","2b","2c","2d","2e","2f",
    "30","31","32","33","34","35","36","37","38","39","3a","3b","3c","3d","3e","3f",
    "40","41","42","43","44","45","46","47","48","49","4a","4b","4c","4d","4e","4f",
    "50","51","52","53","54","55","56","57","58","59","5a","5b","5c","5d","5e","5f",
    "60","61","62","63","64","65","66","67","68","69","6a","6b","6c","6d","6e","6f",
    "70","71","72","73","74","75","76","77","78","79","7a","7b","7c","7d","7e","7f",
    "80","81","82","83","84","85","86","87","88","89","8a","8b","8c","8d","8e","8f",
    "90","91","92","93","94","95","96","97","98","99","9a","9b","9c","9d","9e","9f",
    "a0","a1","a2","a3","a4","a5","a6","a7","a8","a9","aa","ab","ac","ad","ae","af",
    "b0","b1","b2","b3","b4","b5","b6","b7","b8","b9","ba","bb","bc","bd","be","bf",
    "c0","c1","c2","c3","c4","c5","c6","c7","c8","c9","ca","cb","cc","cd","ce","cf",
    "d0","d1","d2","d3","d4","d5","d6","d7","d8","d9","da","db","dc","dd","de","df",
    "e0","e1","e2","e3","e4","e5","e6","e7","e8","e9","ea","eb","ec","ed","ee","ef",
    "f0","f1","f2","f3","f4","f5","f6","f7","f8","f9","fa","fb","fc","fd","fe","ff"
};

const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz"
                           "0123456789+/";

typedef unsigned int uint;

#define VCH_ALLOC_T std::vector<unsigned char, Allocator>

template < class Allocator = std::allocator<unsigned char> >
class basic_uchar_vector : public VCH_ALLOC_T
{
public:
    basic_uchar_vector() :
                  VCH_ALLOC_T() { }
    basic_uchar_vector(size_t n,
                       const unsigned char& value = 0) :
                  VCH_ALLOC_T(n, value) { }

    template <class InputIterator>
    basic_uchar_vector(InputIterator first,
                       InputIterator last) :
                  VCH_ALLOC_T(first, last) { }

    basic_uchar_vector(const VCH_ALLOC_T& vec) :
                  VCH_ALLOC_T(vec) { }
    basic_uchar_vector(const unsigned char* array, unsigned int size) :
                  VCH_ALLOC_T(array, array + size) { }

    basic_uchar_vector(const std::string& hex)
    {
        this->setHex(hex);
    }

    basic_uchar_vector(std::initializer_list<unsigned char> init) :
                  VCH_ALLOC_T(init) { }

    basic_uchar_vector& operator+=(const VCH_ALLOC_T& rhs)
    {
        this->insert(this->end(), rhs.begin(), rhs.end());
        return *this;
    }

    basic_uchar_vector& operator<<(const VCH_ALLOC_T& rhs)
    {
        this->insert(this->end(), rhs.begin(), rhs.end());
        return *this;
    }

    basic_uchar_vector& operator<<(unsigned char byte)
    {
        this->push_back(byte);
        return *this;
    }

    const basic_uchar_vector operator+(VCH_ALLOC_T& rightOperand) const
    {
        return basic_uchar_vector(*this) += rightOperand;
    }

    basic_uchar_vector& operator=(const std::string& hex)
    {
        this->setHex(hex); return *this;
    }

    void copyToArray(unsigned char* array)
    {
        std::copy(this->begin(),this->end(), array);
    }

    void padLeft(unsigned char pad, uint total_length)
    {
        this->reverse();
        this->padRight(pad, total_length);
        this->reverse();
    }

    void padRight(unsigned char pad, uint total_length)
    {
        for (uint i = this->size(); i < total_length; i++)
        {
            this->push_back(pad);
        }
    }

    std::string getHex(bool spaceBytes = false) const
    {
        std::string hex;
        hex.reserve(this->size() * 2);
        for (uint i = 0; i < this->size(); i++)
        {
            if (spaceBytes && (i > 0))
            {
                hex += " ";
            }
            hex += g_hexBytes[(*this)[i]];
        }
        return hex;
    }

    void setHex(std::string hex)
    {
        this->clear();

        // pad on the left if hex contains an odd number of digits.
        if (hex.size() % 2 == 1)
            hex = "0" + hex;

        this->reserve(hex.size() / 2);

        for (uint i = 0; i < hex.size(); i+=2)
        {
            uint byte;
            sscanf(hex.substr(i, 2).c_str(), "%x", &byte);
            this->push_back(byte);
        }
    }

    void reverse()
    {
        std::reverse(this->begin(), this->end());
    }

    basic_uchar_vector getReverse() const
    {
        basic_uchar_vector rval(*this);
        rval.reverse();
        return rval;
    }

    std::string getCharsAsString() const
    {
        std::string chars;
        chars.reserve(this->size());
        for (uint i = 0; i < this->size(); i++)
        {
            chars += (*this)[i];
        }
        return chars;
    }

    void setCharsFromString(const std::string& chars)
    {
        this->clear();
        this->reserve(chars.size());
        for (uint i = 0; i < chars.size(); i++)
        {
            this->push_back(chars[i]);
        }
    }

    std::string getBase64() const
    {
        unsigned int padding = (3 - (this->size() % 3)) % 3;
        std::string base64;

        basic_uchar_vector paddedBytes = *this;
        for (unsigned int i = 1; i <= padding; i++)
        {
            paddedBytes.push_back(0);
        }

        base64.reserve(4*(paddedBytes.size()) / 3);

        for (unsigned int i = 0; i < paddedBytes.size(); i += 3)
        {
            uint32_t triple = ((uint32_t)paddedBytes[i] << 16) |
                              ((uint32_t)paddedBytes[i+1] << 8) |
                              (uint32_t)paddedBytes[i+2];
            base64 += base64chars[(triple & 0x00fc0000) >> 18];
            base64 += base64chars[(triple & 0x0003f000) >> 12];
            base64 += base64chars[(triple & 0x00000fc0) >> 6];
            base64 += base64chars[triple & 0x0000003f];
        }

        for (unsigned int i = 1; i <= padding; i++)
        {
            base64[base64.size() - i] = '=';
        }

        return base64;
    }

    void setBase64(std::string base64)
    {
        unsigned int padding = (4 - (base64.size() % 4)) % 4;

        std::string paddedBase64;
        paddedBase64.reserve(base64.size() + padding);
        paddedBase64 = base64;
        paddedBase64.append(padding, '=');
        // we'll count them again in the loop so we also get any
        //   that were already there.
        padding = 0;

        this->clear();
        this->reserve(3*paddedBase64.size() / 4);

        bool bEnd = false;
        for (unsigned int i = 0; (i < paddedBase64.size()) && (!bEnd); i+=4)
        {
            uint32_t digits[4];
            for (unsigned int j = 0; j < 4; j++)
            {
                const char* pPos = strchr(base64chars, paddedBase64[i+j]);
                if (!pPos)
                {
                    bEnd = true;
                }
                if (bEnd)
                {
                    digits[j] = 0;
                    padding++;
                }
                else
                {
                    digits[j] = (uint32_t)(pPos - base64chars);
                }
            }

            uint32_t quadruple = (digits[0] << 18) |
                                 (digits[1] << 12) |
                                 (digits[2] << 6) |
                                 digits[3];

            this->push_back((quadruple & 0x00ff0000) >> 16);
            this->push_back((quadruple & 0x0000ff00) >> 8);
            this->push_back(quadruple & 0x000000ff);
        }

        for (unsigned int i = 0; i < padding; i++)
        {
            this->pop_back();
        }
    }
};

// Defined in allocators.h
using string_secure = SecureString;
using uchar_vector =
              basic_uchar_vector<std::allocator<unsigned char> >;
using uchar_vector_secure =
              basic_uchar_vector<secure_allocator<unsigned char> >;

#endif  // UCHAR_VECTOR_H__
