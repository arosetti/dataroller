#!/bin/sh

GDB="debug.gdb"
BINARY="dataroller"
BINARY_ARGS="$1"

DLOG="debug-`date`.log"

echo -e "run\nbt\nbt full\ninfo thread\nthread apply all backtrace full" > $GDB

gdb -x $GDB --batch --args ./$BINARY $BINARY_ARGS > $DLOG

