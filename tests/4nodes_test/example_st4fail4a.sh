#!/bin/sh
#	途中でfulfillしない(5555)
#	4444 --> 3333 --> 5555 --> 6666

ROUTECONF=pay_route.conf
AMOUNT=10000000
PAY_BEGIN=4444
PAY_END=6666
ACCIDENT=5555

PAYER=node_${PAY_BEGIN}
PAYER_PORT=$(( ${PAY_BEGIN} + 1 ))
PAYEE=node_${PAY_END}
PAYEE_PORT=$(( ${PAY_END} + 1 ))
ACCIDENT_PORT=$(( ${ACCIDENT} + 1 ))

nodeid() {
	cat conf/peer$1.conf | awk '(NR==3) { print $1 }' | cut -d '=' -f2
}

./routing -d $PAYER -s `nodeid $PAY_BEGIN` -r `nodeid $PAY_END` -a $AMOUNT
if [ $? -ne 0 ]; then
	echo no routing
	exit -1
fi

INVOICE=`./ucoincli -i $AMOUNT $PAYEE_PORT`
if [ $? -ne 0 ]; then
	echo fail get invoice
	exit -1
fi
HASH=`echo $INVOICE | jq -r '.result.hash'`

./routing -d $PAYER -s `nodeid $PAY_BEGIN` -r `nodeid $PAY_END` -a $AMOUNT > $ROUTECONF

# fulfillしない
./ucoincli --debug 1 $ACCIDENT_PORT

# 送金実施
./ucoincli -p $ROUTECONF,$HASH $PAYER_PORT

sleep 2

# 戻す
./ucoincli --debug 1 $PAYEE_PORT
