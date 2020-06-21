#!/bin/bash
set -e

testAlias+=(
	[stealthcoind:trusty]='stealthcoind'
)

imageTests+=(
	[stealthcoind]='
		rpcpassword
	'
)
