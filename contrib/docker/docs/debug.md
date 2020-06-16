# Debugging

## Things to Check

* RAM utilization -- StealthCoind is very hungry and typically needs in excess of 1GB.  A swap file might be necessary.
* Disk utilization -- The StealthCoin blockchain will continue growing and growing.

## Viewing StealthCoin Logs

    docker logs stealthcoind-node


## Running Bash in Docker Container

*Note:* This container will be run in the same way as the StealthCoind node, but will not connect to already running containers or processes.

    docker run -v stealthcoind-data:/stealth --rm -it stealthsend/stealth bash -l

You can also attach bash into running container to debug running StealthCoind

    docker exec -it stealthcoind-node bash -l


