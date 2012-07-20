#!/bin/sh

BINARY="dataroller"

echo "* clean"

if [ -e $BINARY ] ; then
    cp $BINARY tmp
fi

if [ -e "Makefile" ] ; then
    make clean > /dev/null 2>&1
fi

if [ -e "tmp" ] ; then
    mv tmp $BINARY
fi

rm -f *~

find -name *~ -exec rm -f {} \; > /dev/null 2>&1
find -name *.o -exec rm -f {} \; > /dev/null 2>&1

find -name .dirstamp -exec rm -f {} \; > /dev/null 2>&1
find -name .deps -exec rm -rf {} \; > /dev/null 2>&1

rm -rf autom4te.cache/ stamp-h1 config.status install-sh \
        config.log aclocal.m4 Makefile Makefile.in configure config.status config.h \
       config.h.in depcomp missing  INSTALL *.gdb configure.lineno > /dev/null 2>&1
