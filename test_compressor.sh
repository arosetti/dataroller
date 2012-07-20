#!/bin/sh

binary=./dataroller

if [ $# -lt 1 ]; then
  echo "usage: $0 <file> [optional arguments]"
  exit 1
fi

src="$1"
dst="${1}_2"
shift

echo -e "[compression]\n"
$binary -c $src $*
rm -f $dst
echo -e "\n[decompression]\n"
$binary -d ${src}.lzw -o $dst

if diff -u $src $dst > /dev/null; then
  echo -e "\nsuccess!! *** files don't differ ***"
else
  echo -e "\nfailure!! *** files differ!! ***"
  gvimdiff $src $dedst
fi

rm -f $dst ${src}.lzw
