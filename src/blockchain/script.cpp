// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

using namespace std;
using namespace boost;

#include "script.h"
#include "keystore.h"
#include "bignum.h"
#include "key.h"
#include "main.h"
#include "sync.h"
#include "util.h"
#include "stealthaddress.h"
#include "qpos/QPConstants.hpp"

#define OP(x) static_cast<opcodetype>(x)

extern int nBestHeight;

bool CheckSig(vector<unsigned char> vchSig, const vector<unsigned char> &vchPubKey, const CScript &scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType, int flags);

static const valtype vchFalse(0);
static const valtype vchZero(0);
static const valtype vchTrue(1, 1);
static const CBigNum bnZero(0);
static const CBigNum bnOne(1);
static const CBigNum bnFalse(0);
static const CBigNum bnTrue(1);
static const size_t nDefaultMaxNumSize = 4;


CBigNum CastToBigNum(const valtype& vch, const size_t nMaxNumSize = nDefaultMaxNumSize)
{
    if (vch.size() > nMaxNumSize)
        throw runtime_error("CastToBigNum() : overflow");
    // Get rid of extra leading zeros
    return CBigNum(CBigNum(vch).getvch());
}

bool CastToBool(const valtype& vch)
{
    for (unsigned int i = 0; i < vch.size(); i++)
    {
        if (vch[i] != 0)
        {
            // Can be negative zero
            if (i == vch.size()-1 && vch[i] == 0x80)
                return false;
            return true;
        }
    }
    return false;
}

//
// WARNING: This does not work as expected for signed integers; the sign-bit
// is left in place as the integer is zero-extended. The correct behavior
// would be to move the most significant bit of the last byte during the
// resize process. MakeSameSize() is currently only used by the disabled
// opcodes OP_AND, OP_OR, and OP_XOR.
//
void MakeSameSize(valtype& vch1, valtype& vch2)
{
    // Lengthen the shorter one
    if (vch1.size() < vch2.size())
        // PATCH:
        // +unsigned char msb = vch1[vch1.size()-1];
        // +vch1[vch1.size()-1] &= 0x7f;
        //  vch1.resize(vch2.size(), 0);
        // +vch1[vch1.size()-1] = msb;
        vch1.resize(vch2.size(), 0);
    if (vch2.size() < vch1.size())
        // PATCH:
        // +unsigned char msb = vch2[vch2.size()-1];
        // +vch2[vch2.size()-1] &= 0x7f;
        //  vch2.resize(vch1.size(), 0);
        // +vch2[vch2.size()-1] = msb;
        vch2.resize(vch1.size(), 0);
}



//
// Script is a stack machine (like Forth) that evaluates a predicate
// returning a bool indicating valid or not.  There are no loops.
//
#define stacktop(i)  (stack.at(stack.size()+(i)))
#define altstacktop(i)  (altstack.at(altstack.size()+(i)))
static inline void popstack(vector<valtype>& stack)
{
    if (stack.empty())
        throw runtime_error("popstack() : stack empty");
    stack.pop_back();
}


const char* GetTxnOutputType(txnouttype t)
{
    switch (t)
    {
    case TX_NONSTANDARD: return "nonstandard";
    case TX_PUBKEY: return "pubkey";
    case TX_PUBKEYHASH: return "pubkeyhash";
    case TX_SCRIPTHASH: return "scripthash";
    case TX_MULTISIG: return "multisig";
    case TX_PURCHASE1: return "purchase1";
    case TX_PURCHASE4: return "purchasen";
    case TX_SETOWNER: return "setowner";
    case TX_SETMANAGER: return "setmanager";
    case TX_SETDELEGATE: return "setdelegate";
    case TX_SETCONTROLLER: return "setcontroller";
    case TX_ENABLE: return "enable";
    case TX_DISABLE: return "disable";
    case TX_CLAIM: return "claim";
    case TX_SETMETA: return "setmeta";
    case TX_FEEWORK: return "feework";
    case TX_NULL_DATA: return "nulldata";
    }
    return NULL;
}

const char* GetPayeeType(txnouttype t)
{
    switch (t)
    {
    case TX_PUBKEY: return "pubkey";
    case TX_PUBKEYHASH:
    case TX_CLAIM: return "pubkeyhash";
    case TX_SCRIPTHASH: return "scripthash";
    case TX_MULTISIG: return "multisig";
    default: return "none";
    }
    return NULL;
}

const char* GetOpName(opcodetype opcode)
{
    switch (opcode)
    {
    // push value
    case OP_0                      : return "0";
    case OP_PUSHDATA1              : return "OP_PUSHDATA1";
    case OP_PUSHDATA2              : return "OP_PUSHDATA2";
    case OP_PUSHDATA4              : return "OP_PUSHDATA4";
    case OP_1NEGATE                : return "-1";
    case OP_RESERVED               : return "OP_RESERVED";
    case OP_1                      : return "1";
    case OP_2                      : return "2";
    case OP_3                      : return "3";
    case OP_4                      : return "4";
    case OP_5                      : return "5";
    case OP_6                      : return "6";
    case OP_7                      : return "7";
    case OP_8                      : return "8";
    case OP_9                      : return "9";
    case OP_10                     : return "10";
    case OP_11                     : return "11";
    case OP_12                     : return "12";
    case OP_13                     : return "13";
    case OP_14                     : return "14";
    case OP_15                     : return "15";
    case OP_16                     : return "16";

    // control
    case OP_NOP                    : return "OP_NOP";
    case OP_VER                    : return "OP_VER";
    case OP_IF                     : return "OP_IF";
    case OP_NOTIF                  : return "OP_NOTIF";
    case OP_VERIF                  : return "OP_VERIF";
    case OP_VERNOTIF               : return "OP_VERNOTIF";
    case OP_ELSE                   : return "OP_ELSE";
    case OP_ENDIF                  : return "OP_ENDIF";
    case OP_VERIFY                 : return "OP_VERIFY";
    case OP_RETURN                 : return "OP_RETURN";

    // stack ops
    case OP_TOALTSTACK             : return "OP_TOALTSTACK";
    case OP_FROMALTSTACK           : return "OP_FROMALTSTACK";
    case OP_2DROP                  : return "OP_2DROP";
    case OP_2DUP                   : return "OP_2DUP";
    case OP_3DUP                   : return "OP_3DUP";
    case OP_2OVER                  : return "OP_2OVER";
    case OP_2ROT                   : return "OP_2ROT";
    case OP_2SWAP                  : return "OP_2SWAP";
    case OP_IFDUP                  : return "OP_IFDUP";
    case OP_DEPTH                  : return "OP_DEPTH";
    case OP_DROP                   : return "OP_DROP";
    case OP_DUP                    : return "OP_DUP";
    case OP_NIP                    : return "OP_NIP";
    case OP_OVER                   : return "OP_OVER";
    case OP_PICK                   : return "OP_PICK";
    case OP_ROLL                   : return "OP_ROLL";
    case OP_ROT                    : return "OP_ROT";
    case OP_SWAP                   : return "OP_SWAP";
    case OP_TUCK                   : return "OP_TUCK";

    // splice ops
    case OP_CAT                    : return "OP_CAT";
    case OP_SUBSTR                 : return "OP_SUBSTR";
    case OP_LEFT                   : return "OP_LEFT";
    case OP_RIGHT                  : return "OP_RIGHT";
    case OP_SIZE                   : return "OP_SIZE";

    // bit logic
    case OP_INVERT                 : return "OP_INVERT";
    case OP_AND                    : return "OP_AND";
    case OP_OR                     : return "OP_OR";
    case OP_XOR                    : return "OP_XOR";
    case OP_EQUAL                  : return "OP_EQUAL";
    case OP_EQUALVERIFY            : return "OP_EQUALVERIFY";
    case OP_RESERVED1              : return "OP_RESERVED1";
    case OP_RESERVED2              : return "OP_RESERVED2";

    // numeric
    case OP_1ADD                   : return "OP_1ADD";
    case OP_1SUB                   : return "OP_1SUB";
    case OP_2MUL                   : return "OP_2MUL";
    case OP_2DIV                   : return "OP_2DIV";
    case OP_NEGATE                 : return "OP_NEGATE";
    case OP_ABS                    : return "OP_ABS";
    case OP_NOT                    : return "OP_NOT";
    case OP_0NOTEQUAL              : return "OP_0NOTEQUAL";
    case OP_ADD                    : return "OP_ADD";
    case OP_SUB                    : return "OP_SUB";
    case OP_MUL                    : return "OP_MUL";
    case OP_DIV                    : return "OP_DIV";
    case OP_MOD                    : return "OP_MOD";
    case OP_LSHIFT                 : return "OP_LSHIFT";
    case OP_RSHIFT                 : return "OP_RSHIFT";
    case OP_BOOLAND                : return "OP_BOOLAND";
    case OP_BOOLOR                 : return "OP_BOOLOR";
    case OP_NUMEQUAL               : return "OP_NUMEQUAL";
    case OP_NUMEQUALVERIFY         : return "OP_NUMEQUALVERIFY";
    case OP_NUMNOTEQUAL            : return "OP_NUMNOTEQUAL";
    case OP_LESSTHAN               : return "OP_LESSTHAN";
    case OP_GREATERTHAN            : return "OP_GREATERTHAN";
    case OP_LESSTHANOREQUAL        : return "OP_LESSTHANOREQUAL";
    case OP_GREATERTHANOREQUAL     : return "OP_GREATERTHANOREQUAL";
    case OP_MIN                    : return "OP_MIN";
    case OP_MAX                    : return "OP_MAX";
    case OP_WITHIN                 : return "OP_WITHIN";

    // crypto
    case OP_RIPEMD160              : return "OP_RIPEMD160";
    case OP_SHA1                   : return "OP_SHA1";
    case OP_SHA256                 : return "OP_SHA256";
    case OP_HASH160                : return "OP_HASH160";
    case OP_HASH256                : return "OP_HASH256";
    case OP_CODESEPARATOR          : return "OP_CODESEPARATOR";
    case OP_CHECKSIG               : return "OP_CHECKSIG";
    case OP_CHECKSIGVERIFY         : return "OP_CHECKSIGVERIFY";
    case OP_CHECKMULTISIG          : return "OP_CHECKMULTISIG";
    case OP_CHECKMULTISIGVERIFY    : return "OP_CHECKMULTISIGVERIFY";

    // expanson
    case OP_NOP1                   : return "OP_NOP1";
    case OP_CHECKLOCKTIMEVERIFY    : return "OP_CHECKLOPTIMEVERIFY";
    case OP_NOP3                   : return "OP_NOP3";
    case OP_NOP4                   : return "OP_NOP4";
    case OP_NOP5                   : return "OP_NOP5";
    case OP_NOP6                   : return "OP_NOP6";
    case OP_NOP7                   : return "OP_NOP7";
    case OP_NOP8                   : return "OP_NOP8";
    case OP_NOP9                   : return "OP_NOP9";
    case OP_NOP10                  : return "OP_NOP10";

    // qPoS
    case OP_PURCHASE1              : return "OP_PURCHASE1";
    case OP_PURCHASE4              : return "OP_PURCHASE4";
    case OP_SETOWNER               : return "OP_SETOWNER";
    case OP_SETMANAGER             : return "OP_SETMANAGER";
    case OP_SETDELEGATE            : return "OP_SETDELEGATE";
    case OP_SETCONTROLLER          : return "OP_SETCONTROLLER";
    case OP_ENABLE                 : return "OP_ENABLE";
    case OP_DISABLE                : return "OP_DISABLE";
    case OP_CLAIM                  : return "OP_CLAIM";
    case OP_SETMETA                : return "OP_SETMETA";

    // feeless transactions
    case OP_FEEWORK                : return "OP_FEEWORK";

    case OP_INVALIDOPCODE          : return "OP_INVALIDOPCODE";

    // Note:
    //  The template matching params OP_SMALLDATA/etc are defined in opcodetype enum
    //  as kind of implementation hack, they are *NOT* real opcodes.  If found in real
    //  Script, just let the default: case deal with them.

    default:
        return "OP_UNKNOWN";
    }
}

bool IsCompressedOrUncompressedPubKey(const valtype &vchPubKey) {
    if (vchPubKey.size() < 33)
        return error("Non-canonical public key: too short");
    if (vchPubKey[0] == 0x04) {
        if (vchPubKey.size() != 65)
            return error("Non-canonical public key: invalid length for uncompressed key");
    } else if (vchPubKey[0] == 0x02 || vchPubKey[0] == 0x03) {
        if (vchPubKey.size() != 33)
            return error("Non-canonical public key: invalid length for compressed key");
    } else {
        return error("Non-canonical public key: neither compressed nor uncompressed");
    }
    return true;
}

bool IsDERSignature(const valtype &vchSig, bool haveHashType) {
    // See https://bitcointalk.org/index.php?topic=8392.msg127623#msg127623
    // A canonical signature exists of: <30> <total len> <02> <len R> <R> <02> <len S> <S> <hashtype>
    // Where R and S are not negative (their first byte has its highest bit not set), and not
    // excessively padded (do not start with a 0 byte, unless an otherwise negative number follows,
    // in which case a single 0 byte is necessary and even required).
    if (vchSig.size() < 9)
        return error("Non-canonical signature: too short");
    if (vchSig.size() > 73)
        return error("Non-canonical signature: too long");
    if (vchSig[0] != 0x30)
        return error("Non-canonical signature: wrong type");
    if (vchSig[1] != vchSig.size() - (haveHashType ? 3 : 2))
        return error("Non-canonical signature: wrong length marker");
    unsigned int nLenR = vchSig[3];
    if (5 + nLenR >= vchSig.size())
        return error("Non-canonical signature: S length misplaced");
    unsigned int nLenS = vchSig[5+nLenR];
    if ((unsigned long)(nLenR + nLenS + (haveHashType ? 7 : 6)) != vchSig.size())
        return error("Non-canonical signature: R+S length mismatch");

    const unsigned char *R = &vchSig[4];
    if (R[-2] != 0x02)
        return error("Non-canonical signature: R value type mismatch");
    if (nLenR == 0)
        return error("Non-canonical signature: R length is zero");
    if (R[0] & 0x80)
        return error("Non-canonical signature: R value negative");
    if (nLenR > 1 && (R[0] == 0x00) && !(R[1] & 0x80))
        return error("Non-canonical signature: R value excessively padded");

    const unsigned char *S = &vchSig[6+nLenR];
    if (S[-2] != 0x02)
        return error("Non-canonical signature: S value type mismatch");
    if (nLenS == 0)
        return error("Non-canonical signature: S length is zero");
    if (S[0] & 0x80)
        return error("Non-canonical signature: S value negative");
    if (nLenS > 1 && (S[0] == 0x00) && !(S[1] & 0x80))
        return error("Non-canonical signature: S value excessively padded");

    return true;
}

bool static IsLowDERSignature(const valtype &vchSig) {
    if (!IsDERSignature(vchSig)) {
        return false;
    }
    unsigned int nLenR = vchSig[3];
    unsigned int nLenS = vchSig[5+nLenR];
    const unsigned char *S = &vchSig[6+nLenR];
    // If the S value is above the order of the curve divided by two, its
    // complement modulo the order could have been used instead, which is
    // one byte shorter when encoded correctly.
    if (!CKey::CheckSignatureElement(S, nLenS, true))
        return error("Non-canonical signature: S value is unnecessarily high");

    return true;
}

bool static IsDefinedHashtypeSignature(const valtype &vchSig) {
    if (vchSig.size() == 0) {
        return false;
    }
    unsigned char nHashType = vchSig[vchSig.size() - 1] & (~(SIGHASH_ANYONECANPAY));
    if (nHashType < SIGHASH_ALL || nHashType > SIGHASH_SINGLE)
        return error("Non-canonical signature: unknown hashtype byte");

    return true;
}

bool static CheckSignatureEncoding(const valtype &vchSig, unsigned int flags) {
    if (GetFork(nBestHeight) < XST_FORK005)
    {
        return true;
    }
    // Empty signature. Not strictly DER encoded, but allowed to provide a
    // compact way to provide an invalid signature for use with CHECK(MULTI)SIG
    if ((flags & SCRIPT_VERIFY_ALLOW_EMPTY_SIG) && vchSig.size() == 0) {
        return true;
    }
    if (!IsLowDERSignature(vchSig)) {
        return false;
    } else if (!(flags & SCRIPT_VERIFY_FIX_HASHTYPE) && !IsDefinedHashtypeSignature(vchSig)) {
        return false;
    }
    return true;
}

bool static CheckPubKeyEncoding(const valtype &vchSig) {
    if (!IsCompressedOrUncompressedPubKey(vchSig)) {
        return false;
    }
    return true;
}

static bool CheckLockTime(const CTransaction& txTo, unsigned int nIn, const CBigNum& nLockTime)
{
    // There are two times of nLockTime: lock-by-blockheight
    // and lock-by-blocktime, distinguished by whether
    // nLockTime < LOCKTIME_THRESHOLD.
    //
    // We want to compare apples to apples, so fail the script
    // unless the type of nLockTime being tested is the same as
    // the nLockTime in the transaction.
    if (!(
        (txTo.nLockTime <  LOCKTIME_THRESHOLD && nLockTime <  LOCKTIME_THRESHOLD) ||
        (txTo.nLockTime >= LOCKTIME_THRESHOLD && nLockTime >= LOCKTIME_THRESHOLD)
    ))
        return false;

    // Now that we know we're comparing apples-to-apples, the
    // comparison is a simple numeric one.
    if (nLockTime > (int64_t)txTo.nLockTime)
        return false;

    // Finally the nLockTime feature can be disabled and thus
    // CHECKLOCKTIMEVERIFY bypassed if every txin has been
    // finalized by setting nSequence to maxint. The
    // transaction would be allowed into the blockchain, making
    // the opcode ineffective.
    //
    // Testing if this vin is not final is sufficient to
    // prevent this condition. Alternatively we could test all
    // inputs, but testing just this input minimizes the data
    // required to prove correct CHECKLOCKTIMEVERIFY execution.
    if (txTo.vin[nIn].IsFinal())
        return false;

    return true;
}

bool EvalScript(vector<vector<unsigned char> >& stack, const CScript& script, const CTransaction& txTo, unsigned int nIn, unsigned int flags, int nHashType)
{
    CAutoBN_CTX pctx;
    CScript::const_iterator pc = script.begin();
    CScript::const_iterator pend = script.end();
    CScript::const_iterator pbegincodehash = script.begin();
    opcodetype opcode;
    valtype vchPushValue;
    vector<bool> vfExec;
    vector<valtype> altstack;
    if (script.size() > 10000)
        return false;
    int nOpCount = 0;


    try
    {
        while (pc < pend)
        {
            bool fExec = !count(vfExec.begin(), vfExec.end(), false);

            //
            // Read instruction
            //
            if (!script.GetOp(pc, opcode, vchPushValue))
            {
                return false;
            }
            if (vchPushValue.size() > MAX_SCRIPT_ELEMENT_SIZE)
                return false;
            if (opcode > OP_16 && ++nOpCount > 201)
                return false;

            if (opcode == OP_CAT ||
                opcode == OP_SUBSTR ||
                opcode == OP_LEFT ||
                opcode == OP_RIGHT ||
                opcode == OP_INVERT ||
                opcode == OP_AND ||
                opcode == OP_OR ||
                opcode == OP_XOR ||
                opcode == OP_2MUL ||
                opcode == OP_2DIV ||
                opcode == OP_MUL ||
                opcode == OP_DIV ||
                opcode == OP_MOD ||
                opcode == OP_LSHIFT ||
                opcode == OP_RSHIFT)
                return false;

            if (fExec && 0 <= opcode && opcode <= OP_PUSHDATA4)
                stack.push_back(vchPushValue);
            else if (fExec || (OP_IF <= opcode && opcode <= OP_ENDIF))
            switch (opcode)
            {
                //
                // Push value
                //
                case OP_1NEGATE:
                case OP_1:
                case OP_2:
                case OP_3:
                case OP_4:
                case OP_5:
                case OP_6:
                case OP_7:
                case OP_8:
                case OP_9:
                case OP_10:
                case OP_11:
                case OP_12:
                case OP_13:
                case OP_14:
                case OP_15:
                case OP_16:
                {
                    // ( -- value)
                    CBigNum bn((int)opcode - (int)(OP_1 - 1));
                    stack.push_back(bn.getvch());
                }
                break;


                //
                // Control
                //
                case OP_NOP:
                    break;

                case OP_CHECKLOCKTIMEVERIFY:
                {
                    // not necessary, but activate on hard fork
                    if (!(flags & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY)) {
                        // not enabled; treat as a NOP2
                        if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS) {
                            return false;
                        }
                        break;
                    }

                    if (stack.size() < 1)
                        return false;

                    // Note that elsewhere numeric opcodes are limited to
                    // operands in the range -2**31+1 to 2**31-1, however it is
                    // legal for opcodes to produce results exceeding that
                    // range. This limitation is implemented by CScriptNum's
                    // default 4-byte limit.
                    //
                    // If we kept to that limit we'd have a year 2038 problem,
                    // even though the nLockTime field in transactions
                    // themselves is uint32 which only becomes meaningless
                    // after the year 2106.
                    //
                    // Thus as a special case we tell CScriptNum to accept up
                    // to 5-byte bignums, which are good until 2**32-1, the
                    // same limit as the nLockTime field itself.
                    const CBigNum nLockTime = CastToBigNum(stacktop(-1), 5);

                    // In the rare event that the argument may be < 0 due to
                    // some arithmetic being done first, you can always use
                    // 0 MAX CHECKLOCKTIMEVERIFY.
                    if (nLockTime < 0)
                        return false;

                    // Actually compare the specified lock time with the transaction.
                    if (!CheckLockTime(txTo, nIn, nLockTime))
                        return false;

                    break;
                }

                case OP_NOP1: case OP_NOP3: case OP_NOP4: case OP_NOP5:
                case OP_NOP6: case OP_NOP7: case OP_NOP8: case OP_NOP9: case OP_NOP10:
                {
                    if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS)
                        return false;
                }
                break;

                // qPoS: no script with these opcodes will be spendable
                case OP_PURCHASE1: case OP_PURCHASE4:
                case OP_SETOWNER: case OP_SETMANAGER:
                case OP_SETDELEGATE: case OP_SETCONTROLLER:
                case OP_ENABLE: case OP_DISABLE:
                case OP_SETMETA:
                {
                    return false;
                }

                // qPoS OP_CLAIM pops 41 bytes of the amount off the stack
                case OP_CLAIM:
                {
                    if (stack.size() != 3)
                    {
                        return false;
                    }
                    if (stack.back().size() != 41)
                    {
                        return false;
                    }
                    popstack(stack);
                    break;
                }

                // feeless transactions: no feework is spendable
                case OP_FEEWORK:
                {
                    return false;
                }

                case OP_IF:
                case OP_NOTIF:
                {
                    // <expression> if [statements] [else [statements]] endif
                    bool fValue = false;
                    if (fExec)
                    {
                        if (stack.size() < 1)
                            return false;
                        valtype& vch = stacktop(-1);
                        fValue = CastToBool(vch);
                        if (opcode == OP_NOTIF)
                            fValue = !fValue;
                        popstack(stack);
                    }
                    vfExec.push_back(fValue);
                }
                break;

                case OP_ELSE:
                {
                    if (vfExec.empty())
                        return false;
                    vfExec.back() = !vfExec.back();
                }
                break;

                case OP_ENDIF:
                {
                    if (vfExec.empty())
                        return false;
                    vfExec.pop_back();
                }
                break;

                case OP_VERIFY:
                {
                    // (true -- ) or
                    // (false -- false) and return
                    if (stack.size() < 1)
                    {
                        return false;
                    }
                    bool fValue = CastToBool(stacktop(-1));
                    if (fValue)
                        popstack(stack);
                    else
                    {
                        return false;
                    }
                }
                break;

                case OP_RETURN:
                {
                    return false;
                }
                break;


                //
                // Stack ops
                //
                case OP_TOALTSTACK:
                {
                    if (stack.size() < 1)
                        return false;
                    altstack.push_back(stacktop(-1));
                    popstack(stack);
                }
                break;

                case OP_FROMALTSTACK:
                {
                    if (altstack.size() < 1)
                        return false;
                    stack.push_back(altstacktop(-1));
                    popstack(altstack);
                }
                break;

                case OP_2DROP:
                {
                    // (x1 x2 -- )
                    if (stack.size() < 2)
                        return false;
                    popstack(stack);
                    popstack(stack);
                }
                break;

                case OP_2DUP:
                {
                    // (x1 x2 -- x1 x2 x1 x2)
                    if (stack.size() < 2)
                        return false;
                    valtype vch1 = stacktop(-2);
                    valtype vch2 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                }
                break;

                case OP_3DUP:
                {
                    // (x1 x2 x3 -- x1 x2 x3 x1 x2 x3)
                    if (stack.size() < 3)
                        return false;
                    valtype vch1 = stacktop(-3);
                    valtype vch2 = stacktop(-2);
                    valtype vch3 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                    stack.push_back(vch3);
                }
                break;

                case OP_2OVER:
                {
                    // (x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2)
                    if (stack.size() < 4)
                        return false;
                    valtype vch1 = stacktop(-4);
                    valtype vch2 = stacktop(-3);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                }
                break;

                case OP_2ROT:
                {
                    // (x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2)
                    if (stack.size() < 6)
                        return false;
                    valtype vch1 = stacktop(-6);
                    valtype vch2 = stacktop(-5);
                    stack.erase(stack.end()-6, stack.end()-4);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                }
                break;

                case OP_2SWAP:
                {
                    // (x1 x2 x3 x4 -- x3 x4 x1 x2)
                    if (stack.size() < 4)
                        return false;
                    swap(stacktop(-4), stacktop(-2));
                    swap(stacktop(-3), stacktop(-1));
                }
                break;

                case OP_IFDUP:
                {
                    // (x - 0 | x x)
                    if (stack.size() < 1)
                        return false;
                    valtype vch = stacktop(-1);
                    if (CastToBool(vch))
                        stack.push_back(vch);
                }
                break;

                case OP_DEPTH:
                {
                    // -- stacksize
                    CBigNum bn((unsigned int)stack.size());
                    stack.push_back(bn.getvch());
                }
                break;

                case OP_DROP:
                {
                    // (x -- )
                    if (stack.size() < 1)
                        return false;
                    popstack(stack);
                }
                break;

                case OP_DUP:
                {
                    // (x -- x x)
                    if (stack.size() < 1)
                    {
                        return false;
                    }
                    valtype vch = stacktop(-1);
                    stack.push_back(vch);
                }
                break;

                case OP_NIP:
                {
                    // (x1 x2 -- x2)
                    if (stack.size() < 2)
                        return false;
                    stack.erase(stack.end() - 2);
                }
                break;

                case OP_OVER:
                {
                    // (x1 x2 -- x1 x2 x1)
                    if (stack.size() < 2)
                        return false;
                    valtype vch = stacktop(-2);
                    stack.push_back(vch);
                }
                break;

                case OP_PICK:
                case OP_ROLL:
                {
                    // (xn ... x2 x1 x0 n - xn ... x2 x1 x0 xn)
                    // (xn ... x2 x1 x0 n - ... x2 x1 x0 xn)
                    if (stack.size() < 2)
                        return false;
                    int n = CastToBigNum(stacktop(-1)).getint();
                    popstack(stack);
                    if (n < 0 || n >= (int)stack.size())
                        return false;
                    valtype vch = stacktop(-n-1);
                    if (opcode == OP_ROLL)
                        stack.erase(stack.end()-n-1);
                    stack.push_back(vch);
                }
                break;

                case OP_ROT:
                {
                    // (x1 x2 x3 -- x2 x3 x1)
                    //  x2 x1 x3  after first swap
                    //  x2 x3 x1  after second swap
                    if (stack.size() < 3)
                        return false;
                    swap(stacktop(-3), stacktop(-2));
                    swap(stacktop(-2), stacktop(-1));
                }
                break;

                case OP_SWAP:
                {
                    // (x1 x2 -- x2 x1)
                    if (stack.size() < 2)
                        return false;
                    swap(stacktop(-2), stacktop(-1));
                }
                break;

                case OP_TUCK:
                {
                    // (x1 x2 -- x2 x1 x2)
                    if (stack.size() < 2)
                        return false;
                    valtype vch = stacktop(-1);
                    stack.insert(stack.end()-2, vch);
                }
                break;


                //
                // Splice ops
                //
                case OP_CAT:
                {
                    // (x1 x2 -- out)
                    if (stack.size() < 2)
                        return false;
                    valtype& vch1 = stacktop(-2);
                    valtype& vch2 = stacktop(-1);
                    vch1.insert(vch1.end(), vch2.begin(), vch2.end());
                    popstack(stack);
                    if (stacktop(-1).size() > MAX_SCRIPT_ELEMENT_SIZE)
                        return false;
                }
                break;

                case OP_SUBSTR:
                {
                    // (in begin size -- out)
                    if (stack.size() < 3)
                        return false;
                    valtype& vch = stacktop(-3);
                    int nBegin = CastToBigNum(stacktop(-2)).getint();
                    int nEnd = nBegin + CastToBigNum(stacktop(-1)).getint();
                    if (nBegin < 0 || nEnd < nBegin)
                        return false;
                    if (nBegin > (int)vch.size())
                        nBegin = vch.size();
                    if (nEnd > (int)vch.size())
                        nEnd = vch.size();
                    vch.erase(vch.begin() + nEnd, vch.end());
                    vch.erase(vch.begin(), vch.begin() + nBegin);
                    popstack(stack);
                    popstack(stack);
                }
                break;

                case OP_LEFT:
                case OP_RIGHT:
                {
                    // (in size -- out)
                    if (stack.size() < 2)
                        return false;
                    valtype& vch = stacktop(-2);
                    int nSize = CastToBigNum(stacktop(-1)).getint();
                    if (nSize < 0)
                        return false;
                    if (nSize > (int)vch.size())
                        nSize = vch.size();
                    if (opcode == OP_LEFT)
                        vch.erase(vch.begin() + nSize, vch.end());
                    else
                        vch.erase(vch.begin(), vch.end() - nSize);
                    popstack(stack);
                }
                break;

                case OP_SIZE:
                {
                    // (in -- in size)
                    if (stack.size() < 1)
                        return false;
                    CBigNum bn((unsigned int)stacktop(-1).size());
                    stack.push_back(bn.getvch());
                }
                break;


                //
                // Bitwise logic
                //
                case OP_INVERT:
                {
                    // (in - out)
                    if (stack.size() < 1)
                        return false;
                    valtype& vch = stacktop(-1);
                    for (unsigned int i = 0; i < vch.size(); i++)
                        vch[i] = ~vch[i];
                }
                break;

                //
                // WARNING: These disabled opcodes exhibit unexpected behavior
                // when used on signed integers due to a bug in MakeSameSize()
                // [see definition of MakeSameSize() above].
                //
                case OP_AND:
                case OP_OR:
                case OP_XOR:
                {
                    // (x1 x2 - out)
                    if (stack.size() < 2)
                        return false;
                    valtype& vch1 = stacktop(-2);
                    valtype& vch2 = stacktop(-1);
                    MakeSameSize(vch1, vch2); // <-- NOT SAFE FOR SIGNED VALUES
                    if (opcode == OP_AND)
                    {
                        for (unsigned int i = 0; i < vch1.size(); i++)
                            vch1[i] &= vch2[i];
                    }
                    else if (opcode == OP_OR)
                    {
                        for (unsigned int i = 0; i < vch1.size(); i++)
                            vch1[i] |= vch2[i];
                    }
                    else if (opcode == OP_XOR)
                    {
                        for (unsigned int i = 0; i < vch1.size(); i++)
                            vch1[i] ^= vch2[i];
                    }
                    popstack(stack);
                }
                break;

                case OP_EQUAL:
                case OP_EQUALVERIFY:
                //case OP_NOTEQUAL: // use OP_NUMNOTEQUAL
                {
                    // (x1 x2 - bool)
                    if (stack.size() < 2)
                    {
                        return false;
                    }
                    valtype& vch1 = stacktop(-2);
                    valtype& vch2 = stacktop(-1);
                    bool fEqual = (vch1 == vch2);
                    // OP_NOTEQUAL is disabled because it would be too easy to say
                    // something like n != 1 and have some wiseguy pass in 1 with extra
                    // zero bytes after it (numerically, 0x01 == 0x0001 == 0x000001)
                    //if (opcode == OP_NOTEQUAL)
                    //    fEqual = !fEqual;
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fEqual ? vchTrue : vchFalse);
                    if (opcode == OP_EQUALVERIFY)
                    {
                        if (fEqual)
                            popstack(stack);
                        else
                        {
                            return false;
                        }
                    }
                }
                break;


                //
                // Numeric
                //
                case OP_1ADD:
                case OP_1SUB:
                case OP_2MUL:
                case OP_2DIV:
                case OP_NEGATE:
                case OP_ABS:
                case OP_NOT:
                case OP_0NOTEQUAL:
                {
                    // (in -- out)
                    if (stack.size() < 1)
                        return false;
                    CBigNum bn = CastToBigNum(stacktop(-1));
                    switch (opcode)
                    {
                    case OP_1ADD:       bn += bnOne; break;
                    case OP_1SUB:       bn -= bnOne; break;
                    case OP_2MUL:       bn <<= 1; break;
                    case OP_2DIV:       bn >>= 1; break;
                    case OP_NEGATE:     bn = -bn; break;
                    case OP_ABS:        if (bn < bnZero) bn = -bn; break;
                    case OP_NOT:        bn = (bn == bnZero); break;
                    case OP_0NOTEQUAL:  bn = (bn != bnZero); break;
                    default:            assert(!"invalid opcode"); break;
                    }
                    popstack(stack);
                    stack.push_back(bn.getvch());
                }
                break;

                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                case OP_MOD:
                case OP_LSHIFT:
                case OP_RSHIFT:
                case OP_BOOLAND:
                case OP_BOOLOR:
                case OP_NUMEQUAL:
                case OP_NUMEQUALVERIFY:
                case OP_NUMNOTEQUAL:
                case OP_LESSTHAN:
                case OP_GREATERTHAN:
                case OP_LESSTHANOREQUAL:
                case OP_GREATERTHANOREQUAL:
                case OP_MIN:
                case OP_MAX:
                {
                    // (x1 x2 -- out)
                    if (stack.size() < 2)
                        return false;
                    CBigNum bn1 = CastToBigNum(stacktop(-2));
                    CBigNum bn2 = CastToBigNum(stacktop(-1));
                    CBigNum bn;
                    switch (opcode)
                    {
                    case OP_ADD:
                        bn = bn1 + bn2;
                        break;

                    case OP_SUB:
                        bn = bn1 - bn2;
                        break;

                    case OP_MUL:
                        if (!BN_mul(&bn, &bn1, &bn2, pctx))
                            return false;
                        break;

                    case OP_DIV:
                        if (!BN_div(&bn, NULL, &bn1, &bn2, pctx))
                            return false;
                        break;

                    case OP_MOD:
                        if (!BN_mod(&bn, &bn1, &bn2, pctx))
                            return false;
                        break;

                    case OP_LSHIFT:
                        if (bn2 < bnZero || bn2 > CBigNum(2048))
                            return false;
                        bn = bn1 << bn2.getulong();
                        break;

                    case OP_RSHIFT:
                        if (bn2 < bnZero || bn2 > CBigNum(2048))
                            return false;
                        bn = bn1 >> bn2.getulong();
                        break;

                    case OP_BOOLAND:             bn = (bn1 != bnZero && bn2 != bnZero); break;
                    case OP_BOOLOR:              bn = (bn1 != bnZero || bn2 != bnZero); break;
                    case OP_NUMEQUAL:            bn = (bn1 == bn2); break;
                    case OP_NUMEQUALVERIFY:      bn = (bn1 == bn2); break;
                    case OP_NUMNOTEQUAL:         bn = (bn1 != bn2); break;
                    case OP_LESSTHAN:            bn = (bn1 < bn2); break;
                    case OP_GREATERTHAN:         bn = (bn1 > bn2); break;
                    case OP_LESSTHANOREQUAL:     bn = (bn1 <= bn2); break;
                    case OP_GREATERTHANOREQUAL:  bn = (bn1 >= bn2); break;
                    case OP_MIN:                 bn = (bn1 < bn2 ? bn1 : bn2); break;
                    case OP_MAX:                 bn = (bn1 > bn2 ? bn1 : bn2); break;
                    default:                     assert(!"invalid opcode"); break;
                    }
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(bn.getvch());

                    if (opcode == OP_NUMEQUALVERIFY)
                    {
                        if (CastToBool(stacktop(-1)))
                            popstack(stack);
                        else
                            return false;
                    }
                }
                break;

                case OP_WITHIN:
                {
                    // (x min max -- out)
                    if (stack.size() < 3)
                        return false;
                    CBigNum bn1 = CastToBigNum(stacktop(-3));
                    CBigNum bn2 = CastToBigNum(stacktop(-2));
                    CBigNum bn3 = CastToBigNum(stacktop(-1));
                    bool fValue = (bn2 <= bn1 && bn1 < bn3);
                    popstack(stack);
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fValue ? vchTrue : vchFalse);
                }
                break;


                //
                // Crypto
                //
                case OP_RIPEMD160:
                case OP_SHA1:
                case OP_SHA256:
                case OP_HASH160:
                case OP_HASH256:
                {
                    // (in -- hash)
                    if (stack.size() < 1)
                    {
                        return false;
                    }
                    valtype& vch = stacktop(-1);
                    valtype vchHash((opcode == OP_RIPEMD160 || opcode == OP_SHA1 || opcode == OP_HASH160) ? 20 : 32);
                    if (opcode == OP_RIPEMD160)
                        RIPEMD160(&vch[0], vch.size(), &vchHash[0]);
                    else if (opcode == OP_SHA1)
                        SHA1(&vch[0], vch.size(), &vchHash[0]);
                    else if (opcode == OP_SHA256)
                        SHA256(&vch[0], vch.size(), &vchHash[0]);
                    else if (opcode == OP_HASH160)
                    {
                        uint160 hash160 = Hash160(vch);
                        memcpy(&vchHash[0], &hash160, sizeof(hash160));
                    }
                    else if (opcode == OP_HASH256)
                    {
                        uint256 hash = Hash(vch.begin(), vch.end());
                        memcpy(&vchHash[0], &hash, sizeof(hash));
                    }
                    popstack(stack);
                    stack.push_back(vchHash);
                }
                break;

                case OP_CODESEPARATOR:
                {
                    // Hash starts after the code separator
                    pbegincodehash = pc;
                }
                break;

                case OP_CHECKSIG:
                case OP_CHECKSIGVERIFY:
                {
                    // (sig pubkey -- bool)
                    if (stack.size() < 2)
                    {
                        return false;
                    }

                    valtype& vchSig    = stacktop(-2);
                    valtype& vchPubKey = stacktop(-1);

                    // Subset of script starting at the most recent codeseparator
                    CScript scriptCode(pbegincodehash, pend);

                    // Drop the signature, since there's no way for a signature to sign itself
                    scriptCode.FindAndDelete(CScript(vchSig));

                    if ((flags & SCRIPT_VERIFY_STRICTENC) &&
                        (!CheckSignatureEncoding(vchSig, flags) ||
                        !CheckPubKeyEncoding(vchPubKey)))
                    {
                        return false;
                    }

                    bool fSuccess = CheckSignatureEncoding(vchSig, flags) && CheckPubKeyEncoding(vchPubKey) &&
                        CheckSig(vchSig, vchPubKey, scriptCode, txTo, nIn, nHashType, flags);

                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fSuccess ? vchTrue : vchFalse);
                    if (opcode == OP_CHECKSIGVERIFY)
                    {
                        if (fSuccess)
                            popstack(stack);
                        else
                        {
                            return false;
                        }
                    }
                }
                break;

                case OP_CHECKMULTISIG:
                case OP_CHECKMULTISIGVERIFY:
                {
                    // ([sig ...] num_of_signatures [pubkey ...] num_of_pubkeys -- bool)

                    int i = 1;
                    if ((int)stack.size() < i)
                        return false;

                    int nKeysCount = CastToBigNum(stacktop(-i)).getint();
                    if (nKeysCount < 0 || nKeysCount > 20)
                        return false;
                    nOpCount += nKeysCount;
                    if (nOpCount > 201)
                        return false;
                    int ikey = ++i;
                    i += nKeysCount;
                    if ((int)stack.size() < i)
                        return false;

                    int nSigsCount = CastToBigNum(stacktop(-i)).getint();
                    if (nSigsCount < 0 || nSigsCount > nKeysCount)
                        return false;
                    int isig = ++i;
                    i += nSigsCount;
                    if ((int)stack.size() < i)
                        return false;

                    // Subset of script starting at the most recent codeseparator
                    CScript scriptCode(pbegincodehash, pend);

                    // Drop the signatures, since there's no way for a signature to sign itself
                    for (int k = 0; k < nSigsCount; k++)
                    {
                        valtype& vchSig = stacktop(-isig-k);
                        scriptCode.FindAndDelete(CScript(vchSig));
                    }

                    bool fSuccess = true;
                    while (fSuccess && nSigsCount > 0)
                    {
                        valtype& vchSig    = stacktop(-isig);
                        valtype& vchPubKey = stacktop(-ikey);

                        if ((flags & SCRIPT_VERIFY_STRICTENC) && (!CheckSignatureEncoding(vchSig, flags) || !CheckPubKeyEncoding(vchPubKey)))
                            return false;

                        // Check signature
                        bool fOk = CheckSignatureEncoding(vchSig, flags) && CheckPubKeyEncoding(vchPubKey) &&
                            CheckSig(vchSig, vchPubKey, scriptCode, txTo, nIn, nHashType, flags);

                        if (fOk)
                        {
                            isig++;
                            nSigsCount--;
                        }
                        ikey++;
                        nKeysCount--;

                        // If there are more signatures left than keys left,
                        // then too many signatures have failed
                        if (nSigsCount > nKeysCount)
                            fSuccess = false;
                    }

                    // Clean up stack of actual arguments
                    while (i-- > 1)
                        popstack(stack);

                    // A bug causes CHECKMULTISIG to consume one extra argument
                    // whose contents were not checked in any way.
                    //
                    // Unfortunately this is a potential source of mutability,
                    // so optionally verify it is exactly equal to zero prior
                    // to removing it from the stack.
                    if (stack.size() < 1)
                        return false;
                    if ((flags & SCRIPT_VERIFY_NULLDUMMY) && stacktop(-1).size())
                        return error("CHECKMULTISIG dummy argument not null");
                    popstack(stack);

                    stack.push_back(fSuccess ? vchTrue : vchFalse);

                    if (opcode == OP_CHECKMULTISIGVERIFY)
                    {
                        if (fSuccess)
                            popstack(stack);
                        else
                            return false;
                    }
                }
                break;

                default:
                    return false;
            }

            // Size limits
            if (stack.size() + altstack.size() > 1000)
                return false;
        }
    }
    catch (...)
    {
        return false;
    }


    if (!vfExec.empty())
        return false;

    return true;
}

uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType)
{
    if (nIn >= txTo.vin.size())
    {
        printf("ERROR: SignatureHash() : nIn=%d out of range\n", nIn);
        return 1;
    }
    CTransaction txTmp(txTo);

    // In case concatenating two scripts ends up with two codeseparators,
    // or an extra one at the end, this prevents all those possible incompatibilities.
    scriptCode.FindAndDelete(CScript(OP_CODESEPARATOR));

    // Blank out other inputs' signatures
    for (unsigned int i = 0; i < txTmp.vin.size(); i++)
    {
        txTmp.vin[i].scriptSig = CScript();
    }
    txTmp.vin[nIn].scriptSig = scriptCode;

    // Blank out some of the outputs
    if ((nHashType & 0x1f) == SIGHASH_NONE)
    {
        // Wildcard payee
        txTmp.vout.clear();

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
        {
            if (i != nIn)
            {
                txTmp.vin[i].nSequence = 0;
            }
        }
    }
    else if ((nHashType & 0x1f) == SIGHASH_SINGLE)
    {
        // Only lock-in the txout payee at same index as txin
        unsigned int nOut = nIn;
        if (nOut >= txTmp.vout.size())
        {
            printf("ERROR: SignatureHash() : nOut=%d out of range\n", nOut);
            return 1;
        }
        txTmp.vout.resize(nOut+1);
        for (unsigned int i = 0; i < nOut; i++)
        {
            txTmp.vout[i].SetNull();
        }

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
        {
            if (i != nIn)
            {
                txTmp.vin[i].nSequence = 0;
            }
        }
    }

    // Blank out other inputs completely, not recommended for open transactions
    if (nHashType & SIGHASH_ANYONECANPAY)
    {
        txTmp.vin[0] = txTmp.vin[nIn];
        txTmp.vin.resize(1);
    }

    // Serialize and hash
    CDataStream ss(SER_GETHASH, 0);
    ss.reserve(10000);
    if (txTo.nVersion >= CTransaction::IMMALLEABLE_VERSION)
    {
        // prepend txid to stream
        ss << txTo.GetHash() << txTmp << nHashType;
    }
    else
    {
        ss << txTmp << nHashType;
    }
    return Hash(ss.begin(), ss.end());
}


// Valid signature cache, to avoid doing expensive ECDSA signature checking
// twice for every transaction (once when accepted into memory pool, and
// again when accepted into the block chain)

class CSignatureCache
{
private:
     // sigdata_type is (signature hash, signature, public key):
    typedef boost::tuple<uint256, std::vector<unsigned char>, std::vector<unsigned char> > sigdata_type;
    std::set< sigdata_type> setValid;
    CCriticalSection cs_sigcache;

public:
    bool
    Get(uint256 hash, const std::vector<unsigned char>& vchSig, const std::vector<unsigned char>& pubKey)
    {
        LOCK(cs_sigcache);

        sigdata_type k(hash, vchSig, pubKey);
        std::set<sigdata_type>::iterator mi = setValid.find(k);
        if (mi != setValid.end())
            return true;
        return false;
    }

    void Set(uint256 hash, const std::vector<unsigned char>& vchSig, const std::vector<unsigned char>& pubKey)
    {
        // DoS prevention: limit cache size to less than 10MB
        // (~200 bytes per cache entry times 50,000 entries)
        // Since there are a maximum of 20,000 signature operations per block
        // 50,000 is a reasonable default.
        int64_t nMaxCacheSize = GetArg("-maxsigcachesize", 50000);
        if (nMaxCacheSize <= 0) return;

        LOCK(cs_sigcache);

        while (static_cast<int64_t>(setValid.size()) > nMaxCacheSize)
        {
            // Evict a random entry. Random because that helps
            // foil would-be DoS attackers who might try to pre-generate
            // and re-use a set of valid signatures just-slightly-greater
            // than our cache size.
            uint256 randomHash = GetRandHash();
            std::vector<unsigned char> unused;
            std::set<sigdata_type>::iterator it =
                setValid.lower_bound(sigdata_type(randomHash, unused, unused));
            if (it == setValid.end())
                it = setValid.begin();
            setValid.erase(*it);
        }

        sigdata_type k(hash, vchSig, pubKey);
        setValid.insert(k);
    }
};

bool CheckSig(vector<unsigned char> vchSig, const vector<unsigned char> &vchPubKey, const CScript &scriptCode,
              const CTransaction& txTo, unsigned int nIn, int nHashType, int flags)
{
    static CSignatureCache signatureCache;

    // Hash type is one byte tacked on to the end of the signature
    if (vchSig.empty())
        return false;
    if (nHashType == 0)
        nHashType = vchSig.back();
    else if (nHashType != vchSig.back())
        return false;
    vchSig.pop_back();

    uint256 sighash = SignatureHash(scriptCode, txTo, nIn, nHashType);

    if (signatureCache.Get(sighash, vchSig, vchPubKey))
        return true;

    CKey key;
    if (!key.SetPubKey(vchPubKey))
        return false;

    if (!key.Verify(sighash, vchSig))
        return false;

    signatureCache.Set(sighash, vchSig, vchPubKey);

    return true;
}


bool TxTypeIsStandard(txnouttype t, const vector<valtype>& vSolutions)
{
    switch (t)
    {
    case TX_MULTISIG:
      {
        unsigned char m = vSolutions.front()[0];
        unsigned char n = vSolutions.back()[0];
        if (m<1 || n<1 || m>n || n>3 || (vSolutions.size() - 2) != n)
        {
            return false;
        }
        break;
      }
    case TX_PURCHASE1:
    case TX_PURCHASE4:
      {
        if (vSolutions.size() < 1)
        {
            // this should never happen
            printf("TxTypeIsStandard(): TSNH vSolutions size < 1\n");
            return false;
        }
        valtype v = vSolutions.front();
        if (t == TX_PURCHASE1)
        {
            if (v.size() != 57)
            {
                return false;
            }
        }
        else if ((v.size() != 127) && (v.size() != 160))
        {
            return false;
        }
        valtype::const_iterator b = v.end() - 16;
        valtype::const_iterator e;
        for (e = b; e != v.end(); ++e)
        {
            if (*e == static_cast<unsigned char>(0))
            {
                break;
            }
        }
        string s(b, e);
        if (!AliasIsValid(s))
        {
            return false;
        }
        break;
      }
    case TX_SETOWNER:
    case TX_SETMANAGER:
    case TX_SETCONTROLLER:
        if (vSolutions.front().size() != 37)
        {
            return false;
        }
        break;
    case TX_SETDELEGATE:
        if (vSolutions.front().size() != 41)
        {
            return false;
        }
        break;
    case TX_CLAIM:
        if (vSolutions.front().size() != 41)
        {
            return false;
        }
        break;
    case TX_SETMETA:
      {
        if (vSolutions.size() < 1)
        {
            // this should never happen
            printf("TxTypeIsStandard(): TSNH vSolutions size < 1\n");
            return false;
        }
        valtype v = vSolutions.front();
        unsigned int nSizeFront = v.size();
        if (nSizeFront != 60)
        {
            return false;
        }
        // first 4 bytes are ID
        valtype::const_iterator b = v.begin() + 4;
        valtype::const_iterator e;
        unsigned int nKeyLength = 0;
        for (e = b; e != v.end(); ++e)
        {
            if (*e == static_cast<unsigned char>(0))
            {
                break;
            }
            ++nKeyLength;
            if (nKeyLength == QP_MAX_META_KEY_LENGTH)
            {
                break;
            }
        }
        string sKey(b, e);
        if (CheckMetaKey(sKey) == QPKEY_NONE)
        {
            return false;
        }
        b = v.begin() + 20;
        for (e = b; e != v.end(); ++e)
        {
            if (*e == static_cast<unsigned char>(0))
            {
                break;
            }
        }
        string sValue(b, e);
        if (!CheckMetaValue(sValue))
        {
            return false;
        }
        break;
      }
    case TX_FEEWORK:
        if (vSolutions.front().size() != 16)
        {
            return false;
        }
        break;
    case TX_NONSTANDARD:
        return false;
    default:
        break;
    }
    return true;
}

//
// Return public keys or hashes from scriptPubKey, for 'standard' transaction types.
//
bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, vector<vector<unsigned char> >& vSolutionsRet)
{
    // Templates
    static multimap<txnouttype, CScript> mTemplates;
    if (mTemplates.empty())
    {
        // Standard tx, sender provides pubkey, receiver adds signature
        mTemplates.insert(make_pair(TX_PUBKEY, CScript() << OP_PUBKEY << OP_CHECKSIG));

        // Bitcoin address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        mTemplates.insert(make_pair(TX_PUBKEYHASH, CScript() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));

        // Sender provides N pubkeys, receivers provides M signatures
        mTemplates.insert(make_pair(TX_MULTISIG, CScript() << OP_SMALLINTEGER << OP_PUBKEYS << OP_SMALLINTEGER << OP_CHECKMULTISIG));

        // qPoS transactions are not prunable, only TX_CLAIM is spendable
        // PubKeys are 33 byte compressed keys
        // name is 16 byte 0 padded at the end because canonical
        //   pushes enforce data size < OP_PUSHDATA1 to be fixed width
        //   and will not treat PURCHASE4 differently from PURCHASE1
        // 0x00 < OpCodes < OP_PUSHDATA1 (0x4c == 76) specify size to push

        // ====================================================================
        // Buy a staker, all 4 keys are the same, amount is 8 byte
        // size = 57 bytes (0x39)
        // [size, data(amount, pubKey, name), OP_PURCHASE1]
        mTemplates.insert(make_pair(TX_PURCHASE1, CScript() << OP(0x39) << OP_PURCHASE1));

        // ====================================================================
        // Buy a staker, all 4 keys are potentially unique, amount is 8 byte
        // The option to specify 3 keys is for testnet backwards compatibility
        // size = 127 bytes (0x7f)
        //        160 bytes (0xa0)
        // keys = (ownerKey, managerKey/delegateKey, controllerKey) if 127 bytes
        //      = (ownerKey, managerKey, delegateKey, controllerKey) if 140 
        // pcm = 4 byte unsigned int
        // [OP_PUSHDATA1, size, data(amount, keys, pcm, name), OP_PURCHASE4]
        mTemplates.insert(make_pair(TX_PURCHASE4, CScript() << OP_PUSHDATA1 << OP_PURCHASE4));

        // ====================================================================
        // Assign staker owner key (signatory of input must match owner of stakerID)
        // size = 37 bytes (0x25)
        // [size, data(stakerID, pubKey), OP_SETOWNER]
        mTemplates.insert(make_pair(TX_SETOWNER, CScript() << OP(0x25) << OP_SETOWNER));

        // ====================================================================
        // Assign staker manager key (signatory of input must match owner of stakerID)
        // size = 37 bytes (0x25)
        // [size, data(stakerID, pubKey), OP_SETMANAGER]
        mTemplates.insert(make_pair(TX_SETMANAGER, CScript() << OP(0x25) << OP_SETMANAGER));

        // ====================================================================
        // Assign staker delegate key and payout (signatory of input must match owner of stakerID)
        // size = 41 bytes (0x29) (pcm is a 4 byte unsigned int)
        // [size, data(stakerID, pubKey, pcm), OP_SETDELEGATE]
        mTemplates.insert(make_pair(TX_SETDELEGATE, CScript() << OP(0x29) << OP_SETDELEGATE));

        // ====================================================================
        // Assign staker controller key (signatory of input must match owner of stakerID)
        // size = 37 bytes (0x25)
        // [size, data(stakerID, pubKey), OP_SETCONTROLLER]
        mTemplates.insert(make_pair(TX_SETCONTROLLER, CScript() << OP(0x25) << OP_SETCONTROLLER));

        // ====================================================================
        // Enable staker (signatory of input must match owner of stakerID)
        // size = 4 bytes (0x04)
        // [size, data(stakerID), OP_ENABLE]
        mTemplates.insert(make_pair(TX_ENABLE, CScript() << OP(0x04) << OP_ENABLE));

        // ====================================================================
        // Disable staker (signatory of input must match owner of stakerID)
        // size = 4 bytes (0x04)
        // [size, data(stakerID), OP_DISABLE]
        mTemplates.insert(make_pair(TX_DISABLE, CScript() << OP(0x04) << OP_DISABLE));

        // ====================================================================
        // Claim rewards from registry ledger, is spendable, use only bitcoin-style address
        // Input pubKey is spent from registry ledger, amount is 8 byte unsigned int
        // size = 41 bytes (0x29) = 33 bytes of pubkey + 8 bytes amount
        // [size, data(pubKey, amount), OP_CLAIM, OP_DUP, OP_HASH160, OP_PUBKEYHASH, OP_EQUALVERIFY, OP_CHECKSIG]
        mTemplates.insert(make_pair(TX_CLAIM, CScript() << OP(0x29) << OP_CLAIM << OP_DUP << OP_HASH160 <<
                                                 OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));

        // ====================================================================
        // Set metadata for staker (signatory of input must match owner of stakerID)
        // key is <= 16 bytes, with significant other constraints
        // value is a string length <= 40, matching egrep of:
        //            [a-zA-Z._: <>/@#,+-]+
        // both key and value are 0x0 terminated if end-padded (to comply with canonical pushes)
        // 0 length value (before padding) is empty string and means to delete the key/value pair
        // additional key/value constraints may be enforced by the protocol elsewhere
        // size = 60 (0x3c) = 4 bytes of ID + 16 bytes of key + 40 bytes of value
        // [size, data(stakerID, key, value), OP_SETMETA]
        mTemplates.insert(make_pair(TX_SETMETA, CScript() << OP(0x3c) << OP_SETMETA));

        // ====================================================================
        // Fee work for feeless transactions
        // Must be last vout (aka vout[-1])
        // workhash is 64 bit (8 byte) i.e.:
        //       workhash = argon2d(work, hashblock, tx*)
        //    work is 8 byte unsigned int (i.e. uint64_t)
        //       might be thought of as a "nonce"
        //    hashblock is hash of the block at height,
        //       this allows feeless transactions specifying a block that is too
        //       old to be cleared from the mempool, reducing mempool flooding
        //       with feeless transactions where the work is far less than
        //       current demand requires
        //    tx* is the transaction
        //       without the last element of vout and
        //          - workhash isn't a hash of the work
        //       with all signatures stripped from vin
        //          - allows the work to be signed in each of the inputs
        //          - prevents any malleability related to the fee work
        // Transactions stored info:
        //    height is 4 bytes unsigned int (i.e. uint32_t)
        //    size = 16 bytes (0x10) = 4 bytes of height + 4 bytes of mcost + 8 bytes of work
        // [size, data(height, mcost, work), OP_FEEWORK]
        mTemplates.insert(make_pair(TX_FEEWORK, CScript() << OP(0x10) << OP_FEEWORK));

        // ====================================================================
        // Empty, provably prunable, data-carrying output
        mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN));
        mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN << OP_SMALLDATA));
        mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN << OP_SMALLDATA << OP_RETURN << OP_SMALLDATA));
    }

    // Shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
    if (scriptPubKey.IsPayToScriptHash())
    {
        typeRet = TX_SCRIPTHASH;
        vector<unsigned char> hashBytes(scriptPubKey.begin()+2, scriptPubKey.begin()+22);
        vSolutionsRet.push_back(hashBytes);
        return true;
    }

    static const bool fDebugSolver = false;

    // Scan templates
    const CScript& script1 = scriptPubKey;
    BOOST_FOREACH(const PAIRTYPE(txnouttype, CScript)& tplate, mTemplates)
    {
        if (fDebugSolver)
        {
            printf("debug solver: whichtype is '%s' (%d)\n",
                    GetTxnOutputType(tplate.first), (int)tplate.first);
        }
        const CScript& script2 = tplate.second;
        vSolutionsRet.clear();

        opcodetype opcode1, opcode2;
        vector<unsigned char> vch1, vch2;

        // Compare
        CScript::const_iterator pc1 = script1.begin();
        CScript::const_iterator pc2 = script2.begin();
        while (true)
        {
            if (pc1 == script1.end() && pc2 == script2.end())
            {
                // Found a match
                typeRet = tplate.first;
                // Do additional checks for matching types
                return TxTypeIsStandard(tplate.first, vSolutionsRet);
            }
            if (!script1.GetOp(pc1, opcode1, vch1))
            {
                if (fDebugSolver)
                {
                    printf("debug solver: cant' get op 1 (%d)\n", (int)opcode1);
                }
                break;
            }
                if (fDebugSolver)
                {
                    printf("debug solver: op 1 is %s (%d)\n",
                           GetOpName(opcode1), (int)opcode1);
                }
            // get op for template, so last arg (fTemplate) is true
            if (!script2.GetOp(pc2, opcode2, vch2, true))
            {
                if (fDebugSolver)
                {
                    printf("debug solver: cant' get op 2 (%d)\n",(int)opcode2);
                }
                break;
            }
            if (fDebugSolver)
            {
                printf("debug solver: op 2 is %s (%d)\n",
                       GetOpName(opcode2), (int)opcode2);
            }

            // Template matching opcodes:
            if (opcode2 == OP_PUBKEYS)
            {
                while (vch1.size() >= 33 && vch1.size() <= 120)
                {
                    vSolutionsRet.push_back(vch1);
                    if (!script1.GetOp(pc1, opcode1, vch1))
                    {
                        if (fDebugSolver)
                        {
                            printf("debug solver: cant' get op 1 for pubkeys\n");
                        }
                        break;
                    }
                }
                // get op for template, so last arg (fTemplate) is true
                if (!script2.GetOp(pc2, opcode2, vch2, true))
                {
                    if (fDebugSolver)
                    {
                        printf("debug solver: cant' get op 2 for pubkeys\n");
                    }
                    break;
                }
                // Normal situation is to fall through
                // to other if/else statements
            }

            if (opcode2 == OP_PUBKEY)
            {
                if (vch1.size() < 33 || vch1.size() > 120)
                {
                    if (fDebugSolver)
                    {
                        printf("debug solver: size wrong for pubkey\n");
                    }
                    break;
                }
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_PUBKEYHASH)
            {
                if (vch1.size() != sizeof(uint160))
                {
                    if (fDebugSolver)
                    {
                        printf("debug solver: size wrong for pubkeyhash\n");
                    }
                    break;
                }
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_SMALLINTEGER)
            {   // Single-byte small integer pushed onto vSolutions
                if (opcode1 == OP_0 ||
                    (opcode1 >= OP_1 && opcode1 <= OP_16))
                {
                    char n = (char)CScript::DecodeOP_N(opcode1);
                    vSolutionsRet.push_back(valtype(1, n));
                }
                else
                {
                    if (fDebugSolver)
                    {
                        printf("debug solver: small integer missmatch\n");
                    }
                    break;
                }
            }
            else if (opcode2 == OP_SMALLDATA)
            {
                // small pushdata,  <= MAX_OP_RETURN_RELAY bytes
                if (vch1.size() > MAX_OP_RETURN_RELAY)
                {
                    if (fDebugSolver)
                    {
                        printf("debug solver: vch1 too big for smalldata\n");
                    }
                    break;
                }
            }
            else if ((opcode1 != opcode2) ||
                     ((opcode2 > OP_PUSHDATA4) && (vch1 != vch2)))
            {
                if (fDebugSolver)
                {
                    printf("debug solver: opcode1 is %d, opcode2 is %d\n",
                           (int)opcode1, (int)opcode2);
                }
                // Others must match exactly, except for push vch
                // Exempting push vch allows for templating pushes
                break;
            }
            else if (opcode2 <= OP_PUSHDATA4)
            {
                vSolutionsRet.push_back(vch1);
            }
        }
    }

    vSolutionsRet.clear();
    typeRet = TX_NONSTANDARD;
    return false;
}


bool Sign1(const CKeyID& address, const CKeyStore& keystore, uint256 hash, int nHashType, CScript& scriptSigRet)
{
    CKey key;
    if (!keystore.GetKey(address, key))
        return false;

    vector<unsigned char> vchSig;
    if (!key.Sign(hash, vchSig))
        return false;
    vchSig.push_back((unsigned char)nHashType);
    scriptSigRet << vchSig;

    return true;
}

bool SignN(const vector<valtype>& multisigdata, const CKeyStore& keystore, uint256 hash, int nHashType, CScript& scriptSigRet)
{
    int nSigned = 0;
    int nRequired = multisigdata.front()[0];
    for (unsigned int i = 1; i < multisigdata.size()-1 && nSigned < nRequired; i++)
    {
        const valtype& pubkey = multisigdata[i];
        CKeyID keyID = CPubKey(pubkey).GetID();
        if (Sign1(keyID, keystore, hash, nHashType, scriptSigRet))
            ++nSigned;
    }
    return nSigned==nRequired;
}

//
// Sign scriptPubKey with private keys stored in keystore, given transaction hash and hash type.
// Signatures are returned in scriptSigRet (or returns false if scriptPubKey can't be signed),
// unless whichTypeRet is TX_SCRIPTHASH, in which case scriptSigRet is the redemption script.
// Returns false if scriptPubKey could not be completely satisfied.
//
bool Solver(const CKeyStore& keystore, const CScript& scriptPubKey, uint256 hash, int nHashType,
                  CScript& scriptSigRet, txnouttype& whichTypeRet)
{
    scriptSigRet.clear();

    vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, whichTypeRet, vSolutions))
    {
        return false;
    }

    CKeyID keyID;
    switch (whichTypeRet)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return false;
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        return Sign1(keyID, keystore, hash, nHashType, scriptSigRet);
    case TX_PUBKEYHASH:
    case TX_CLAIM:
        keyID = CKeyID(uint160(vSolutions.back()));
        if (!Sign1(keyID, keystore, hash, nHashType, scriptSigRet))
            return false;
        else
        {
            CPubKey vch;
            keystore.GetPubKey(keyID, vch);
            scriptSigRet << vch;
        }
        return true;
    case TX_SCRIPTHASH:
        return keystore.GetCScript(uint160(vSolutions[0]), scriptSigRet);
    case TX_MULTISIG:
        scriptSigRet << OP_0; // workaround CHECKMULTISIG bug
        return (SignN(vSolutions, keystore, hash, nHashType, scriptSigRet));
    default:
        break;
    }
    return false;
}

int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vSolutions)
{
    switch (t)
    {
    case TX_PURCHASE1:
    case TX_PURCHASE4:
    case TX_SETOWNER:
    case TX_SETMANAGER:
    case TX_SETDELEGATE:
    case TX_SETCONTROLLER:
    case TX_ENABLE:
    case TX_DISABLE:
    case TX_SETMETA:
    case TX_FEEWORK:
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return -1;
    case TX_PUBKEY:
        return 1;
    case TX_PUBKEYHASH:
    case TX_CLAIM:
        return 2;
    case TX_MULTISIG:
        if (vSolutions.size() < 1 || vSolutions[0].size() < 1)
            return -1;
        return vSolutions[0][0] + 1;
    case TX_SCRIPTHASH:
        return 1; // doesn't include args needed by the script
    }
    return -1;
}

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType)
{
    vector<valtype> vSolutions;
    return Solver(scriptPubKey, whichType, vSolutions);
}

unsigned int HaveKeys(const vector<valtype>& pubkeys, const CKeyStore& keystore)
{
    unsigned int nResult = 0;
    BOOST_FOREACH(const valtype& pubkey, pubkeys)
    {
        CKeyID keyID = CPubKey(pubkey).GetID();
        if (keystore.HaveKey(keyID))
            ++nResult;
    }
    return nResult;
}


class CKeyStoreIsMineVisitor : public boost::static_visitor<bool>
{
private:
    const CKeyStore *keystore;
public:
    CKeyStoreIsMineVisitor(const CKeyStore *keystoreIn) : keystore(keystoreIn) { }
    bool operator()(const CNoDestination &dest) const { return false; }
    bool operator()(const CKeyID &keyID) const { return keystore->HaveKey(keyID); }
    bool operator()(const CScriptID &scriptID) const { return keystore->HaveCScript(scriptID); }
    bool operator()(const CStealthAddress &stxAddr) const
    {
        return stxAddr.scan_secret.size() == ec_secret_size;
    }
};

bool IsMine(const CKeyStore &keystore, const CTxDestination &dest)
{
    return boost::apply_visitor(CKeyStoreIsMineVisitor(&keystore), dest);
}

bool IsMine(const CKeyStore &keystore, const CScript& scriptPubKey)
{
    vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    CKeyID keyID;
    switch (whichType)
    {
        case TX_NONSTANDARD:
        case TX_NULL_DATA:
            return false;
        case TX_PUBKEY:
            keyID = CPubKey(vSolutions[0]).GetID();
            return keystore.HaveKey(keyID);
        case TX_CLAIM:
        case TX_PUBKEYHASH:
            keyID = CKeyID(uint160(vSolutions.back()));
            return keystore.HaveKey(keyID);
        case TX_SCRIPTHASH:
        {
            CScript subscript;
            if (!keystore.GetCScript(CScriptID(uint160(vSolutions[0])), subscript))
            {
                return false;
            }
            return IsMine(keystore, subscript);
        }
        case TX_MULTISIG:
        {
            // Only consider transactions "mine" if we own ALL the
            // keys involved. multi-signature transactions that are
            // partially owned (somebody else has a key that can spend
            // them) enable spend-out-from-under-you attacks, especially
            // in shared-wallet situations.
            vector<valtype> keys(vSolutions.begin()+1, vSolutions.begin()+vSolutions.size()-1);
            return HaveKeys(keys, keystore) == keys.size();
        }
        default:
            break;
    }
    return false;
}

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet)
{
    vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY)
    {
        CPubKey pubKey(vSolutions[0]);
        if (!pubKey.IsValid())
            return false;

        addressRet = pubKey.GetID();
        return true;
    }
    else if ((whichType == TX_PUBKEYHASH) || (whichType == TX_CLAIM))
    {
        addressRet = CKeyID(uint160(vSolutions.back()));
        return true;
    }
    else if (whichType == TX_SCRIPTHASH)
    {
        addressRet = CScriptID(uint160(vSolutions[0]));
        return true;
    }
    // Multisig txns have more than one address...
    return false;
}

class CAffectedKeysVisitor : public boost::static_visitor<void> {
private:
    const CKeyStore &keystore;
    std::vector<CKeyID> &vKeys;

public:
    CAffectedKeysVisitor(const CKeyStore &keystoreIn, std::vector<CKeyID> &vKeysIn) : keystore(keystoreIn), vKeys(vKeysIn) {}

    void Process(const CScript &script) {
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (ExtractDestinations(script, type, vDest, nRequired)) {
            BOOST_FOREACH(const CTxDestination &dest, vDest)
                boost::apply_visitor(*this, dest);
        }
    }

    void operator()(const CKeyID &keyId) {
        if (keystore.HaveKey(keyId))
            vKeys.push_back(keyId);
    }

    void operator()(const CScriptID &scriptId) {
        CScript script;
        if (keystore.GetCScript(scriptId, script))
            Process(script);
    }

    void operator()(const CStealthAddress &stxAddr) {
        CScript script;
    }
    void operator()(const CNoDestination &none) {}
};


void ExtractAffectedKeys(const CKeyStore &keystore, const CScript& scriptPubKey, std::vector<CKeyID> &vKeys) {
    CAffectedKeysVisitor(keystore, vKeys).Process(scriptPubKey);
}

bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, vector<CTxDestination>& addressRet, int& nRequiredRet)
{
    addressRet.clear();
    typeRet = TX_NONSTANDARD;
    vector<valtype> vSolutions;



    if (!Solver(scriptPubKey, typeRet, vSolutions))
        return false;
    if (typeRet == TX_NULL_DATA){
        // This is data, not addresses
        return false;
    }

    if (typeRet == TX_MULTISIG)
    {
        nRequiredRet = vSolutions.front()[0];
        for (unsigned int i = 1; i < vSolutions.size()-1; i++)
        {
            CTxDestination address = CPubKey(vSolutions[i]).GetID();
            addressRet.push_back(address);
        }
    }
    else
    {
        nRequiredRet = 1;
        CTxDestination address;
        if (!ExtractDestination(scriptPubKey, address))
           return false;
        addressRet.push_back(address);
    }

    return true;
}

bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                  unsigned int flags, int nHashType)
{
    vector<vector<unsigned char> > stack, stackCopy;
    if (!EvalScript(stack, scriptSig, txTo, nIn, flags, nHashType))
    {
        printf("VerifyScript(): signature script does not evaluate\n");
        return false;
    }

    stackCopy = stack;

    if (!EvalScript(stack, scriptPubKey, txTo, nIn, flags, nHashType))
    {
        printf("VerifyScript(): pubkey script does not evaluate\n");
        return false;
    }
    if (stack.empty())
    {
        printf("VerifyScript(): stack is empty\n");
        return false;
    }

    if (CastToBool(stack.back()) == false)
    {
        printf("VerifyScript(): back of stack is false\n");
        return false;
    }

    // Additional validation for spend-to-script-hash transactions:
    if (scriptPubKey.IsPayToScriptHash())
    {
        if (!scriptSig.IsPushOnly()) // scriptSig must be literals-only
            return false;            // or validation fails

        const valtype& pubKeySerialized = stackCopy.back();
        CScript pubKey2(pubKeySerialized.begin(), pubKeySerialized.end());
        popstack(stackCopy);

        if (!EvalScript(stackCopy, pubKey2, txTo, nIn, flags, nHashType))
            return false;
        if (stackCopy.empty())
            return false;
        return CastToBool(stackCopy.back());
    }

    return true;
}


// TODO: change to an enumerated return type
uint8_t SignSignature(const CKeyStore &keystore, const CScript& fromPubKey, CTransaction& txTo, unsigned int nIn, int nHashType)
{
    assert(nIn < txTo.vin.size());
    CTxIn& txin = txTo.vin[nIn];

    // Leave out the signature from the hash, since a signature can't sign itself.
    // The checksig op will also drop the signatures from its hash.
    uint256 hash = SignatureHash(fromPubKey, txTo, nIn, nHashType);

    txnouttype whichType;
    if (!Solver(keystore, fromPubKey, hash, nHashType, txin.scriptSig, whichType))
    {
        printf("SignSignature(): input script matches no template\n");
        return 1;
    }

    if (whichType == TX_SCRIPTHASH)
    {
        // Solver returns the subscript that need to be evaluated;
        // the final scriptSig is the signatures from that
        // and then the serialized subscript:
        CScript subscript = txin.scriptSig;

        // Recompute txn hash using subscript in place of scriptPubKey:
        uint256 hash2 = SignatureHash(subscript, txTo, nIn, nHashType);

        txnouttype subType;
        bool fSolved =
            Solver(keystore, subscript, hash2, nHashType, txin.scriptSig, subType) && subType != TX_SCRIPTHASH;
        // Append serialized subscript whether or not it is completely signed:
        txin.scriptSig << static_cast<valtype>(subscript);
        if (!fSolved)
        {
            printf("SignSignature(): script hash matches no template\n");
            return 2;
        }
    }

    // Test solution
    if (VerifyScript(txin.scriptSig, fromPubKey, txTo, nIn, STANDARD_SCRIPT_VERIFY_FLAGS, 0))
    {
        return 0;
    }
    else
    {
        printf("SignSignature(): solution does not verify\n");
        return 3;
    }
}

uint8_t SignSignature(const CKeyStore &keystore, const CTransaction& txFrom, CTransaction& txTo, unsigned int nIn, int nHashType)
{
    assert(nIn < txTo.vin.size());
    CTxIn& txin = txTo.vin[nIn];
    assert(txin.prevout.n < txFrom.vout.size());
    assert(txin.prevout.hash == txFrom.GetHash());
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    return SignSignature(keystore, txout.scriptPubKey, txTo, nIn, nHashType);
}

bool VerifySignature(const CTransaction& txFrom, const CTransaction& txTo, unsigned int nIn, unsigned int flags, int nHashType)
{
    assert(nIn < txTo.vin.size());
    const CTxIn& txin = txTo.vin[nIn];
    if (txin.prevout.n >= txFrom.vout.size())
        return false;
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    if (txin.prevout.hash != txFrom.GetHash())
        return false;

    return VerifyScript(txin.scriptSig, txout.scriptPubKey, txTo, nIn, flags, nHashType);
}

static CScript PushAll(const vector<valtype>& values)
{
    CScript result;
    BOOST_FOREACH(const valtype& v, values)
        result << v;
    return result;
}

static CScript CombineMultisig(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                               const vector<valtype>& vSolutions,
                               vector<valtype>& sigs1, vector<valtype>& sigs2)
{
    // Combine all the signatures we've got:
    set<valtype> allsigs;
    BOOST_FOREACH(const valtype& v, sigs1)
    {
        if (!v.empty())
            allsigs.insert(v);
    }
    BOOST_FOREACH(const valtype& v, sigs2)
    {
        if (!v.empty())
            allsigs.insert(v);
    }

    // Build a map of pubkey -> signature by matching sigs to pubkeys:
    assert(vSolutions.size() > 1);
    unsigned int nSigsRequired = vSolutions.front()[0];
    unsigned int nPubKeys = vSolutions.size()-2;
    map<valtype, valtype> sigs;
    BOOST_FOREACH(const valtype& sig, allsigs)
    {
        for (unsigned int i = 0; i < nPubKeys; i++)
        {
            const valtype& pubkey = vSolutions[i+1];
            if (sigs.count(pubkey))
                continue; // Already got a sig for this pubkey

            if (CheckSig(sig, pubkey, scriptPubKey, txTo, nIn, 0, 0))
            {
                sigs[pubkey] = sig;
                break;
            }
        }
    }
    // Now build a merged CScript:
    unsigned int nSigsHave = 0;
    CScript result; result << OP_0; // pop-one-too-many workaround
    for (unsigned int i = 0; i < nPubKeys && nSigsHave < nSigsRequired; i++)
    {
        if (sigs.count(vSolutions[i+1]))
        {
            result << sigs[vSolutions[i+1]];
            ++nSigsHave;
        }
    }
    // Fill any missing with OP_0:
    for (unsigned int i = nSigsHave; i < nSigsRequired; i++)
        result << OP_0;

    return result;
}

static CScript CombineSignatures(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                                 const txnouttype txType, const vector<valtype>& vSolutions,
                                 vector<valtype>& sigs1, vector<valtype>& sigs2)
{
    switch (txType)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        // Don't know anything about this, assume bigger one is correct:
        if (sigs1.size() >= sigs2.size())
            return PushAll(sigs1);
        return PushAll(sigs2);
    case TX_PUBKEY:
    case TX_PUBKEYHASH:
    case TX_CLAIM:
        // Signatures are bigger than placeholders or empty scripts:
        if (sigs1.empty() || sigs1[0].empty())
            return PushAll(sigs2);
        return PushAll(sigs1);
    case TX_SCRIPTHASH:
        if (sigs1.empty() || sigs1.back().empty())
        {
            return PushAll(sigs2);
        }
        else if (sigs2.empty() || sigs2.back().empty())
        {
            return PushAll(sigs1);
        }
        else
        {
            // Recur to combine:
            valtype spk = sigs1.back();
            CScript pubKey2(spk.begin(), spk.end());

            txnouttype txType2;
            vector<vector<unsigned char> > vSolutions2;
            Solver(pubKey2, txType2, vSolutions2);
            sigs1.pop_back();
            sigs2.pop_back();
            CScript result = CombineSignatures(pubKey2, txTo, nIn, txType2, vSolutions2, sigs1, sigs2);
            result << spk;
            return result;
        }
    case TX_MULTISIG:
        return CombineMultisig(scriptPubKey, txTo, nIn, vSolutions, sigs1, sigs2);
    default:
        break;
    }
    return CScript();
}

CScript CombineSignatures(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                          const CScript& scriptSig1, const CScript& scriptSig2)
{
    txnouttype txType;
    vector<vector<unsigned char> > vSolutions;
    Solver(scriptPubKey, txType, vSolutions);

    vector<valtype> stack1;
    EvalScript(stack1, scriptSig1, CTransaction(), 0, SCRIPT_VERIFY_NONE, 0);
    vector<valtype> stack2;
    EvalScript(stack2, scriptSig2, CTransaction(), 0, SCRIPT_VERIFY_NONE, 0);

    return CombineSignatures(scriptPubKey, txTo, nIn, txType, vSolutions, stack1, stack2);
}

unsigned int CScript::GetSigOpCount(bool fAccurate) const
{
    unsigned int n = 0;
    const_iterator pc = begin();
    opcodetype lastOpcode = OP_INVALIDOPCODE;
    while (pc < end())
    {
        opcodetype opcode;
        if (!GetOp(pc, opcode))
            break;
        if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY)
            n++;
        else if (opcode == OP_CHECKMULTISIG || opcode == OP_CHECKMULTISIGVERIFY)
        {
            if (fAccurate && lastOpcode >= OP_1 && lastOpcode <= OP_16)
                n += DecodeOP_N(lastOpcode);
            else
                n += 20;
        }
        lastOpcode = opcode;
    }
    return n;
}

unsigned int CScript::GetSigOpCount(const CScript& scriptSig) const
{
    if (!IsPayToScriptHash())
        return GetSigOpCount(true);

    // This is a pay-to-script-hash scriptPubKey;
    // get the last item that the scriptSig
    // pushes onto the stack:
    const_iterator pc = scriptSig.begin();
    vector<unsigned char> data;
    while (pc < scriptSig.end())
    {
        opcodetype opcode;
        if (!scriptSig.GetOp(pc, opcode, data))
            return 0;
        if (opcode > OP_16)
            return 0;
    }

    /// ... and return its opcount:
    CScript subscript(data.begin(), data.end());
    return subscript.GetSigOpCount(true);
}

bool CScript::IsPayToScriptHash() const
{
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() == 23 &&
            this->at(0) == OP_HASH160 &&
            this->at(1) == 0x14 &&
            this->at(22) == OP_EQUAL);
}

bool CScript::HasCanonicalPushes() const
{
    const_iterator pc = begin();
    while (pc < end())
    {
        opcodetype opcode;
        std::vector<unsigned char> data;
        if (!GetOp(pc, opcode, data))
            return false;
        if (opcode > OP_16)
            continue;
        if (opcode < OP_PUSHDATA1 && opcode > OP_0 && (data.size() == 1 && data[0] <= 16))
            // Could have used an OP_n code, rather than a 1-byte push.
            return false;
        if (opcode == OP_PUSHDATA1 && data.size() < OP_PUSHDATA1)
            // Could have used a normal n-byte push, rather than OP_PUSHDATA1.
            return false;
        if (opcode == OP_PUSHDATA2 && data.size() <= 0xFF)
            // Could have used an OP_PUSHDATA1.
            return false;
        if (opcode == OP_PUSHDATA4 && data.size() <= 0xFFFF)
            // Could have used an OP_PUSHDATA2.
            return false;
    }
    return true;
}

class CScriptVisitor : public boost::static_visitor<bool>
{
private:
    CScript *script;
public:
    CScriptVisitor(CScript *scriptin) { script = scriptin; }

    bool operator()(const CNoDestination &dest) const {
        script->clear();
        return false;
    }

    bool operator()(const CKeyID &keyID) const {
        script->clear();
        *script << OP_DUP << OP_HASH160 << keyID << OP_EQUALVERIFY << OP_CHECKSIG;
        return true;
    }

    bool operator()(const CScriptID &scriptID) const {
        script->clear();
        *script << OP_HASH160 << scriptID << OP_EQUAL;
        return true;
    }

    bool operator()(const CStealthAddress &stxAddr) const {
        script->clear();
        printf("TODO\n");
        return false;
    }
};

void CScript::SetDestination(const CTxDestination& dest)
{
    boost::apply_visitor(CScriptVisitor(this), dest);
}

void CScript::SetMultisig(int nRequired, const std::vector<CKey>& keys)
{
    this->clear();

    *this << EncodeOP_N(nRequired);
    BOOST_FOREACH(const CKey& key, keys)
        *this << key.GetPubKey();
    *this << EncodeOP_N(keys.size()) << OP_CHECKMULTISIG;
}

