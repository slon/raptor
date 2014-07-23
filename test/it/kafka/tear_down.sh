#!/bin/sh

TMP=/tmp/raptor-kafka-test

kill `cat $TMP/zk.pid`
kill `cat $TMP/kafka1.pid`
kill `cat $TMP/kafka2.pid`
kill `cat $TMP/kafka3.pid`

sleep 3

rm -rf $TMP
