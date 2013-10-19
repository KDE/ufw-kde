#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.rc`  >> rc.cpp
$XGETTEXT rc.cpp */*.cpp */*.h -o $podir/kcm_ufw.pot
rm -f rc.cpp
