stealthcoind config tuning
======================

You can use environment variables to customize config ([see docker run environment options](https://docs.docker.com/engine/reference/run/#/env-environment-variables)):

(Set DOWNLOAD_BOOTSTRAP=1 if you want to download and start with bootstrap file)

        docker run -v stealthcoind-data:/stealth --name=stealthcoind-node -d \
            -p 4437:4437 \
            -p 127.0.0.1:46502:46502 \
            -e DISABLEWALLET=1 \
            -e PRINTTOCONSOLE=1 \
            -e RPCUSER=mysecretrpcuser \
            -e RPCPASSWORD=mysecretrpcpassword \
            -e DOWNLOAD_BOOTSTRAP=0 ivanr/stealthcoind

Or you can use your very own config file like that:

        docker run -v stealthcoind-data:/stealth --name=stealthcoind-node -d \
            -p 4437:4437 \
            -p 127.0.0.1:46502:46502 \
            -v /etc/__MY_STEALTH_CONFIG_FILE_.conf:/stealth/.StealthCoin/StealthCoin.conf ivanr/stealthcoind