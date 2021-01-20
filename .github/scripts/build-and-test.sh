#! /bin/bash

set -e

source .github/scripts/common.sh

##########################################################################

echo
echo 'Configuring...' && echo -en '::group::start:script.configure\\r'
echo

if [ "$CONFIG" = "gcc" ]; then
	echo "Configuring for gcc."
	make config-gcc
elif [ "$CONFIG" = "clang" ]; then
	echo "Configuring for clang."
	make config-clang
fi

echo
echo -en '::endgroup::'
echo

##########################################################################

echo
echo 'Building...' && echo -en '::group::start:script.build\\r'
echo

make CC=$CC CXX=$CC LD=$CC

echo
echo -en '::endgroup::'
echo

##########################################################################

./yosys tests/simple/fiedler-cooley.v

echo
echo 'Testing...' && echo -en '::group::start:script.test\\r'
echo

make test

echo
echo -en '::endgroup::'
echo

##########################################################################
