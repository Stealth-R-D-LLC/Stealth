
#ifndef DNSSEEDNODES_H
#define DNSSEEDNODES_H


// Clear Net DNS Seeds
// Each pair gives a source name and a seed name.
// The first name is used as information source for addrman.
// The second name should resolve to a LIST of seed addresses.
static const char *strMainNetDNSSeed[][2] = {
    {"mseeds", "mainnet-seeds.stealth-coin.com"},
    {NULL, NULL}
};


static const char *strTestNetDNSSeed[][2] = {
    {"tseeds", "testnet-seeds.stealth-coin.com"},
    {NULL, NULL}
};


#endif  // DNSSEEDNODES_H
