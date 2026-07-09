# Install Docker

**References**
 - OS X: [https://docs.docker.com/docker-for-mac/install/](https://docs.docker.com/docker-for-mac/install/)
 - Ubuntu: [https://docs.docker.com/engine/install/ubuntu/](https://docs.docker.com/engine/install/ubuntu/)
 - Windows: [https://docs.docker.com/docker-for-windows/install/](https://docs.docker.com/docker-for-windows/install/)

# Build the Docker Image

**Reference**: [https://docs.docker.com/get-started/part2/](https://docs.docker.com/get-started/part2/)

The docker image can be built in the directory of the `Dockerfile`. The command is:

    docker build --tag stealth-16.04 -f Dockerfile-ubuntu-16.04 .

This command tags the image "stealth-16.04", named after the Ubuntu version (16.04) on which the image is based.

Change the tag to `--tag stealth-t-16.04` if the image is intended to be used with testnet (*i.e.* `docker build --tag stealth-t-16.04 .`).

List the image to confirm the build after completion:

    docker image ls

Output will resemble the following:

    \REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
    stealth-16.04       latest              a14f33165963        20 minutes ago      360MB
    <none>              <none>              2b1c93348e09        20 minutes ago      1.47GB
    ubuntu              16.04               005d2078bdfa        6 weeks ago         125MB
    phusion/baseimage   0.10.2              4ca439e72536        22 months ago       233MB

# Initialize the Container by Running It

The first run of a container determines whether it will join the mainnet or testnet. This decision is specified by potentially setting the `TESTNET` environment variable on the first run, using the `docker run` flag `-e`. Several environment variables are described in the file `docs/config.md`.

Runs subsequent to the first run do not need to specify the `TESTNET` environment variable because this flag is set in the `StealthCoin.conf` configuration file when the image is initialized.

Unless specifically refused, the container will download and bootstrap the client on first run. See the file `docs/config.md` for how to refuse bootstrapping.

## Mainnet

The following command runs the docker container for the Steatlh mainnet and also exposes the proxy port for the `StealthCoind` daemon:

    docker run \
        -p 127.0.0.1:56502:46502 \
        --volume stealth-data:/stealth \
        --detach \
        --name xst-main \
        stealth-xenial

This latter command exposes the client's mainnet RPC port on the host port 56502. If the user wishes, the RPC port could be exposed on the host at the default of 46502 (i.e. `-p 127.0.0.1:46502:46502`). The host port `56502` is chosen so that it is possible to run a Stealth client on the host computer without an RPC port conflict.

Adding `127.0.0.1` to the host port numbers ensures that the ports of the docker instance are not exposed beyond the host.

It is also possible to expose the P2P ports (mainnet 4437, testnet 4438) using the `-p` option, although this is not typically useful because the P2P port is proxied through Tor by default.

This mainnet container uses the [volume](https://docs.docker.com/storage/volumes/) called `stealth-data`, which can be shared with a testnet container because testnet data is kept separated from mainnet data.

## Testnet

Users may want to run testnet instead of mainnet. For this purpose, simply add `-e TESTNET=1` to the `run` command and expose the testnet RPC port (46503).
  
    docker run \
        -p 127.0.0.1:56503:46503 \
        -e TESTNET=1 \
        --volume stealth-data:/stealth \
        --detach \
        --name xst-test \
        stealth-t-xenial

The testnet RPC port will be exposed at host port 56503.

# Container Inspection

Docker allows users to open an interactive shell to a running image with the `docker exec` command, using the `-it` flag. The following command opens a bash shell to the image:

    docker exec -it xst-main bash -l

For example, the following command (**used with care!**) may stop the `StealthCoind` process, which would also halt the running image.

    StealthCoind stop

# Using the Container's RPC Interface

With the RPC port tunneled as described above, it is possible to access the client's RPC interface from the host, without executing commands explicitly within the container.

First, however, it is necessary to learn the container's RPC password. One way to do this is with the following command:

    docker exec -it xst-main grep -oP '(?<=password=)\w+' .StealthCoin/StealthCoin.conf

For testnet, reference the correct configuration file:

    docker exec -it xst-main grep -oP '(?<=password=)\w+' .StealthCoin/testnet/StealthCoin.conf

Assuming the password is `RPCPASSWORD`, the following command runs the RPC `getinfo`:

    curl --user stealthrpc:RPCPASSWORD --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getinfo", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:56502/

Parameters are passed through the `params` attribute:

    curl --user stealthrpc:RPCPASSWORD --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getblockbynumber", "params": [0] }' -H 'content-type: text/plain;' http://127.0.0.1:56502/

    curl --user stealthrpc:RPCPASSWORD --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "gettransaction", "params": ["c7e5646c6c3b6b3962ff443aa6d29681f39467a493b7d301fd994a22c5e45f15"] }' -H 'content-type: text/plain;' http://127.0.0.1:46502/

