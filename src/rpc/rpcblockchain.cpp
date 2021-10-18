// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"

using namespace json_spirit;
using namespace std;

extern QPRegistry *pregistryMain;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry);


double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (pindexBest == NULL)
        {
            return 1.0;
        }
        else
        {
            blockindex = GetLastBlockIndex(pindexBest, false);
        }
    }

    if (GetFork(blockindex->nHeight) >= XST_FORKQPOS)
    {
        return 0.0;
    }

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

double GetPoSKernelPS()
{
    int nPoSInterval = 72;
    double dStakeKernelsTriedAvg = 0;
    int nStakesHandled = 0, nStakesTime = 0;

    CBlockIndex* pindex = pindexBest;;
    CBlockIndex* pindexPrevStake = NULL;

    while (pindex && nStakesHandled < nPoSInterval)
    {
        if (pindex->IsProofOfStake())
        {
            dStakeKernelsTriedAvg += GetDifficulty(pindex) * 4294967296.0;
            nStakesTime += pindexPrevStake ? (pindexPrevStake->nTime - pindex->nTime) : 0;
            pindexPrevStake = pindex;
            nStakesHandled++;
        }

        pindex = pindex->pprev;
    }

    return nStakesTime ? dStakeKernelsTriedAvg / nStakesTime : 0;
}

Object blockToJSON(const CBlock& block,
                   const CBlockIndex* blockindex,
                   bool fPrintTransactionDetail)
{
    Object result;
    result.push_back(Pair("hash", block.GetHash().GetHex()));

    if (block.IsQuantumProofOfStake())
    {
        int nConfs = blockindex ?
                        (pindexBest->nHeight + 1 - blockindex->nHeight) : 0;
        if (blockindex && blockindex->IsInMainChain())
        {
            result.push_back(Pair("isinmainchain", true));
            result.push_back(Pair("confirmations", nConfs));
        }
        else
        {

            result.push_back(Pair("isinmainchain", false));
            result.push_back(Pair("confirmations", 0));
            result.push_back(Pair("depth", nConfs));
        }
        result.push_back(Pair("pico_power",
                              blockindex ? blockindex->nPicoPower : 0));
    }
    else
    {
        CMerkleTx txGen(block.vtx[0]);
        txGen.SetMerkleBranch(&block);
        result.push_back(Pair("confirmations",
                              (int)txGen.GetDepthInMainChain()));
    }
    result.push_back(Pair("size", blockindex ?
                                     (int)blockindex->nBlockSize : -1));
    result.push_back(Pair("height", blockindex ? blockindex->nHeight : -1));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    if (blockindex && blockindex->IsQuantumProofOfStake())
    {
        result.push_back(Pair("staker_id", (boost::int64_t)block.nStakerID));
        string sAlias;
        if (pregistryMain->GetAliasForID(block.nStakerID, sAlias))
        {
            result.push_back(Pair("staker_alias", sAlias));
        }
        if (blockindex->pprev != NULL)
        {
            result.push_back(Pair("block_reward",
               ValueFromAmount(GetQPoSReward(blockindex->pprev))));
        }
    }
    result.push_back(Pair("mint",
                          blockindex ?
                             ValueFromAmount(blockindex->nMint) : -1));
    result.push_back(Pair("time", (boost::int64_t)block.GetBlockTime()));
    result.push_back(Pair("nonce", (boost::uint64_t)block.nNonce));
    result.push_back(Pair("bits", HexBits(block.nBits)));
    result.push_back(Pair("difficulty",
                          blockindex ? GetDifficulty(blockindex) : -1));
    result.push_back(Pair("trust",
                          blockindex ?
                             blockindex->bnChainTrust.ToString() : "???"));

    if (blockindex && blockindex->pprev)
    {
        result.push_back(Pair("previousblockhash",
                              blockindex->pprev->GetBlockHash().GetHex()));
    }
    else
    {
        result.push_back(Pair("previousblockhash",
                              block.hashPrevBlock.GetHex()));
    }
    if (blockindex && blockindex->pnext)
    {
        result.push_back(Pair("nextblockhash",
                              blockindex->pnext->GetBlockHash().GetHex()));
    }
    string sFlags;
    if (!blockindex)
    {
        sFlags = "???";
    }
    else if (blockindex->IsQuantumProofOfStake())
    {
        sFlags = "quantum-proof-of-stake";
    }
    else if (blockindex->IsProofOfStake())
    {
        sFlags = "proof-of-stake";
    }
    else
    {
        sFlags = "proof-of-work";
    }
    result.push_back(Pair("flags",
                          strprintf("%s%s",
                                    sFlags.c_str(),
                                    (blockindex &&
                                     blockindex->GeneratedStakeModifier()) ?
                                        " stake-modifier" : "")));
    result.push_back(Pair("proofhash",
                          (blockindex && blockindex->IsProofOfStake()) ?
                              blockindex->hashProofOfStake.GetHex() :
                              block.GetHash().GetHex()));
    if (blockindex && blockindex->IsProofOfStake())
    {
        result.push_back(Pair("entropybit",
                         (int)blockindex->GetStakeEntropyBit()));
        result.push_back(Pair("modifier",
                              strprintf("%016" PRIx64,
                                        blockindex->nStakeModifier)));
        result.push_back(Pair("modifierchecksum",
                              strprintf("%08x",
                                        blockindex->nStakeModifierChecksum)));
    }

    Array txinfo;
    BOOST_FOREACH (const CTransaction& tx, block.vtx)
    {
        if (fPrintTransactionDetail)
        {
            Object entry;

            entry.push_back(Pair("txid", tx.GetHash().GetHex()));
            TxToJSON(tx, 0, entry);

            txinfo.push_back(entry);
        }
        else
            txinfo.push_back(tx.GetHash().GetHex());
    }

    result.push_back(Pair("tx", txinfo));
    result.push_back(Pair("signature", HexStr(block.vchBlockSig.begin(),
                                              block.vchBlockSig.end())));

    return result;
}

Value getbestblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbestblockhash\n"
            "Returns the hash of the best block in the longest block chain.");

    return hashBestChain.GetHex();
}

Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "Returns the number of blocks in the longest block chain.");
    return nBestHeight;
}


Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "Returns the difficulty as a multiple of the minimum difficulty.");

    Object obj;
    obj.push_back(Pair("proof-of-work",        GetDifficulty()));
    obj.push_back(Pair("proof-of-stake",       GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    obj.push_back(Pair("search-interval",      (int)nLastCoinStakeSearchInterval));
    return obj;
}


Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp ||
        (params.size() < 1) ||
        (params.size() > 1) ||
        (AmountFromValue(params[0]) < chainParams.MIN_TX_FEE))
    {
        throw runtime_error(
            "settxfee <amount>\n"
            "<amount> is a real and is rounded to the nearest 0.01");
    }
    nTransactionFee = AmountFromValue(params[0]);
    nTransactionFee = (nTransactionFee / CENT) * CENT;  // round to cent

    return true;
}

Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getrawmempool\n"
            "Returns all transaction ids in memory pool.");

    vector<uint256> vtxid;
    mempool.queryHashes(vtxid);

    Array a;
    BOOST_FOREACH(const uint256& hash, vtxid)
        a.push_back(hash.ToString());

    return a;
}

Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash <index>\n"
            "Returns hash of block in best-block-chain at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlockIndex* pblockindex = FindBlockByHeight(nHeight);
    return pblockindex->phashBlock->GetHex();
}

Value getblockhash9(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash9 <index>\n"
            "Returns hash9 of block in best-block-chain at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlockIndex* pblockindex = FindBlockByHeight(nHeight);

    CBlock block;
    block.ReadFromDisk(pblockindex, true);

    return block.GetHash9().GetHex();
}


Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock <hash> [txinfo]\n"
            "txinfo optional to print more detailed tx info\n"
            "Returns details of a block with given block-hash.");

    string strHash = params[0].get_str();
    uint256 hash(strHash);

    CBlock block;
    CBlockIndex* pblockindex = NULL;
    if (mapBlockIndex.count(hash))
    {
        pblockindex = mapBlockIndex[hash];
        block.ReadFromDisk(pblockindex, true);
    }
    else if (mapOrphanBlocks.count(hash))
    {
        block = *mapOrphanBlocks[hash];
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
    }

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

Value getblockbynumber(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
    {
        throw runtime_error(
            "getblockbynumber <number> [txinfo]\n"
            "txinfo optional to print more detailed tx info\n"
            "Returns details of a block with given block-number.");
    }

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
    {
        throw runtime_error("Block number out of range.");
    }

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > nHeight)
    {
        pblockindex = pblockindex->pprev;
    }

    uint256 hash = *pblockindex->phashBlock;

    pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex, true);

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

Value getbestblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
     {
        throw runtime_error(
            "getbestblock [txinfo]\n"
            "txinfo optional to print more detailed tx info (default=false)\n"
            "Returns the newest block from the longest block chain.");
    }
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    CBlock block;
    block.ReadFromDisk(pblockindex, true);
    return blockToJSON(block, pblockindex,
                       params.size() > 1 ? params[1].get_bool() : false);
}

Value getnewestblockbeforetime(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getnewestblockbeforetime <time>\n"
            "Returns the hash of the newest block that has\n"
            "   a time stamp earlier than <time>\n"
            "<time> is a unix epoch (seconds)");

    unsigned int nTime = params[0].get_int();

    CBlockIndex* pindex = pindexBest;

    bool fFound = false;
    while (pindex->pprev)
    {
        if (pindex->nTime < nTime)
        {
            fFound = true;
            break;
        }
        pindex = pindex->pprev;
    }

    if (!fFound)
    {
        throw runtime_error("Time precedes genesis block.");
    }

    return pindex->GetBlockHash().ToString();
}


// ppcoin: get information of sync-checkpoint
Value getcheckpoint(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getcheckpoint\n"
            "Show info of synchronized checkpoint.\n");

    Object result;
    CBlockIndex* pindexCheckpoint;

    result.push_back(Pair("synccheckpoint", Checkpoints::hashSyncCheckpoint.ToString().c_str()));
    pindexCheckpoint = mapBlockIndex[Checkpoints::hashSyncCheckpoint];
    result.push_back(Pair("height", pindexCheckpoint->nHeight));
    result.push_back(Pair("timestamp", DateTimeStrFormat(pindexCheckpoint->GetBlockTime()).c_str()));
    if (mapArgs.count("-checkpointkey"))
        result.push_back(Pair("checkpointmaster", true));

    return result;
}
