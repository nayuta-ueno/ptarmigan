#!/bin/sh
# 3333がfulfillしない: 4444-->3333
#	HTLCが+1され、4444だけが支払った状態になる

ROUTECONF=pay_route.conf
AMOUNT=10000000
PAY_BEGIN=4444
PAY_END=3333

PAYER=node_${PAY_BEGIN}
PAYER_PORT=$(( ${PAY_BEGIN} + 1 ))
PAYEE=node_${PAY_END}
PAYEE_PORT=$(( ${PAY_END} + 1 ))

nodeid() {
	cat conf/peer$1.conf | awk '(NR==3) { print $1 }' | cut -d '=' -f2
}

./routing -d $PAYER -s `nodeid $PAY_BEGIN` -r `nodeid $PAY_END` -a $AMOUNT
if [ $? -ne 0 ]; then
	echo no routing
	exit -1
fi

INVOICE=`./ptarmcli -i $AMOUNT $PAYEE_PORT`
if [ $? -ne 0 ]; then
	echo fail get invoice
	exit -1
fi
HASH=`echo $INVOICE | jq -r '.result.hash'`

./routing -d $PAYER -s `nodeid $PAY_BEGIN` -r `nodeid $PAY_END` -a $AMOUNT > $ROUTECONF

# fulfillしない
./ptarmcli --debug 1 $PAYEE_PORT

# 送金実施
./ptarmcli -p $ROUTECONF,$HASH $PAYER_PORT

sleep 2

# 戻す
./ptarmcli --debug 1 $PAYEE_PORT
