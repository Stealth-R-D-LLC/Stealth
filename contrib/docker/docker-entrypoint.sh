#!/bin/sh
set -e

# first arg is `-f` or `--some-option`
# or first arg is `something.conf`
if [ "${1#-}" != "$1" ] || [ "${1%.conf}" != "$1" ]; then
	set -- stealth_oneshot "$@"
fi

# allow the container to be started with `--user`
if [ "$1" = 'stealth_oneshot' -a "$(id -u)" = '0' ]; then
	chown -R stealth .
	exec gosu stealth "$0" "$@"
fi

<<<<<<< HEAD
exec "$@"
=======
exec "$@"
>>>>>>> 4cd08c1e322db9e08929def6fce4b8095306744f
