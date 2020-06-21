# StealthCoind Configuration Tuning

Use environment variables to customize config
([see docker run environment options](https://docs.docker.com/engine/reference/run/#/env-environment-variables)):

The following example uses custom `rpcuser` and `rpcpassword` values,
as well as explicitly refusing to bootstrap, which is not recommended.

        docker run \
            -p 127.0.0.1:56502:46502 \
            -e RPCUSER=mysecretrpcuser \
            -e RPCPASSWORD=mysecretrpcpassword \
            -e DOWNLOAD_BOOTSTRAP=0 \
            --volume stealthcoind-data:/stealth \
            --name=xst-main \
            --detatch  \
            stealth-xenial

Use a custom `SttealthCoin.conf` configuration file:

        docker run \
            -v stealthcoind-data:/stealth \
            --name=xst-main \
            --detatch \
            -p 127.0.0.1:46502:46502 \
            -v /etc/__MY_STEALTH_CONFIG_FILE_.conf:/stealth/.StealthCoin/StealthCoin.conf \
            stealth-xenial
