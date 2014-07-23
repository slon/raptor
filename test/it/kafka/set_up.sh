#!/bin/sh

CONF=`pwd`/conf

TMP=/tmp/raptor-kafka-test

mkdir $TMP
cd $TMP

yabs-zookeeper-server $CONF/zk.properties 2>&1 >> $TMP/zk.log &
echo $! > $TMP/zk.pid

kafka-server $CONF/kafka1.properties 2>&1 >> $TMP/kafka1.log &
echo $! > $TMP/kafka1.pid

kafka-server $CONF/kafka2.properties 2>&1 >> $TMP/kafka2.log &
echo $! > $TMP/kafka2.pid

kafka-server $CONF/kafka3.properties 2>&1 >> $TMP/kafka3.log &
echo $! > $TMP/kafka3.pid

sleep 2

kafka-admin-topic --zookeeper localhost:2189 --create --topic empty-topic --partitions 1 --replication-factor 1

sleep 2