#! /bin/bash

set -e

source .github/scripts/common.sh

##########################################################################

# Output status information.
(
	set +e
	set -x
	git status
	git branch -v
	git log -n 5 --graph
	git log --format=oneline -n 20 --graph
)
echo
echo -en '::endgroup::'
echo

##########################################################################

# Mac OS X specific setup.
if [[ "$OS_NAME" == "osx" ]]; then
	(
		echo
		echo 'Setting up brew...' && echo -en '::group::start:before_install.brew\\r'
		echo
		brew update
		brew tap Homebrew/bundle
		brew bundle
		brew install ccache
		echo
		echo -en '::endgroup::'
		echo
	)
fi

##########################################################################

# Install iverilog
(
	if [ ! -e ~/.local-bin/bin/iverilog ]; then
		echo
		echo 'Building iverilog...' && echo -en '::group::start:before_install.iverilog\\r'
		echo
		mkdir -p ~/.local-src
		mkdir -p ~/.local-bin
		cd ~/.local-src
		git clone git://github.com/steveicarus/iverilog.git
		cd iverilog
		autoconf
		CC=gcc CXX=g++ ./configure --prefix=$HOME/.local-bin
		make
		make install
		echo
		echo -en '::endgroup::'
		echo
	fi
)

##########################################################################
