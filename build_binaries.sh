#!/bin/bash
set -ex
INSTALL_PATH=$PWD/image
#Surelog
cd Surelog && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_POSITION_INDEPENDENT_CODE=ON -S . -B build && cmake --build build -j $(nproc) && cmake --install build && cd ..
#Yosys
make PREFIX=$INSTALL_PATH install -j $(nproc)
#sv2v
wget -qO- https://get.haskellstack.org/ | sh -s - -d $INSTALL_PATH/bin
export PATH=$INSTALL_PATH/bin:${PATH} && make -C $PWD/sv2v && cp $PWD/sv2v/bin/sv2v $INSTALL_PATH/bin
