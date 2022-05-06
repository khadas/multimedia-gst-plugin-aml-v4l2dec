#!/bin/sh

echo "start: autoscan"
autoscan
echo "start: aclocal"
aclocal
echo "start: autoconf"
autoconf
echo "start: autoheader"
autoheader
echo "start: automake --add-missing"
automake --add-missing
echo "start: ./configure"
./configure
echo "auto setup finish."
