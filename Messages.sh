#! /usr/bin/env bash
$EXTRACTRC `find . src -name \*.rc -o -name \*.ui -o -name \*.ui4` >> rc.cpp || exit 11
$XGETTEXT `find . src -name "*.cpp" -o -name "*.cc" -o -name "*.h" | grep -v "/tests"` -o $podir/kio5.pot
