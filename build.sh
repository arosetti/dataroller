#!/bin/sh

BINARY="dataroller";
CONF_OPTS="";

help()
{
    echo -e "usage\t$0 -[hdprtc]}\n" \
            "  -d  : enable debug\n" \
            "  -p  : enable profile\n" \
            "  -r  : enable release\n" \
            "  -t  : build tar package\n" \
            "  -c  : clean build system\n" \
            "  -i  : disable inline functions\n" \
            "  -s  : enable hash statistics" ;
    exit 0;
}

if [ ! -d autoconf ] ; then
    mkdir autoconf
fi

while getopts ":hdprtci" opt; do
  case $opt in
    h)
      help;
      ;;
    d)
      CONF_OPTS="$CONF_OPTS --enable-debug"
      ;;
    p)
      CONF_OPTS="$CONF_OPTS --enable-profile"
      ;;
    r)
      CONF_OPTS="$CONF_OPTS --enable-release"
      ;;
    i)
      CONF_OPTS="$CONF_OPTS --enable-inline=no"
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      ;;
  esac
done

aclocal
autoheader
automake --add-missing --copy
autoconf
./configure $CONF_OPTS

count=$(cat /proc/cpuinfo | grep 'model name' | sed -e 's/.*: //' | wc -l)

if [ $count -le 0 ] ;then
count=1; 
fi

make -j$count

OPTIND=0
while getopts ":hdprtci" opt; do
  case $opt in
    t) 
      make dist-bzip2
      ;;
    c)
      ./clean
      ;;
    r)
      strip $BINARY
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      ;;
  esac
done
