#!/bin/bash
set -ex
INSTALL_PATH=$PWD/image
#Surelog
cd Surelog && make PREFIX=$INSTALL_PATH release install -j $(nproc) && cd ..
#Yosys
make PREFIX=$INSTALL_PATH install -j $(nproc)
#vcddiff
make -C vcddiff PREFIX=$INSTALL_PATH -j $(nproc)
cp -p vcddiff/vcddiff $INSTALL_PATH/bin/vcddiff
#sv2v
wget -qO- https://get.haskellstack.org/ | sh -s - -d $INSTALL_PATH/bin
export PATH=$INSTALL_PATH/bin:${PATH} && make -C $PWD/sv2v && cp $PWD/sv2v/bin/sv2v $INSTALL_PATH/bin
