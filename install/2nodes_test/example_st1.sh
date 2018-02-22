#!/bin/sh

mkdir regtest
if [ $? -ne 0 ]; then
	exit -1
fi

ln -s ../ucoincli ucoincli
ln -s ../ucoind ucoind
ln -s ../showdb showdb
ln -s ../routing routing
ln -s ../testfiles/fund-test-in.sh fund-test-in.sh
ln -s ../testfiles/regtest.conf regtest.conf
ln -s ../testfiles/generate.sh generate.sh

bitcoind -conf=`pwd`/regtest.conf -datadir=`pwd`/regtest -daemon
sleep 3
bitcoin-cli -conf=`pwd`/regtest.conf -datadir=`pwd`/regtest generate 432

# 1台のPCで複数ノードを立ち上げるので、ディレクトリをそれぞれ作る。
# 起動したディレクトリに関連ファイルを作成するためだ。
mkdir -p node_3333 node_4444
ln -s ../../script node_3333/script
ln -s ../../script node_4444/script