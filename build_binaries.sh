#!/bin/bash
cd Surelog && make PREFIX=$PWD/../image/ release install -j $(nproc) && cd ..
make PREFIX=$PWD/image install
make -C vcddiff PREFIX=./image
cp -p vcddiff/vcddiff image/bin/vcddiff
cd verilator/Surelog && make PREFIX=$PWD/../image/ release install -j $(nproc) && cd ..
autoconf && ./configure --prefix=$PWD/../image && make install && cd ..
