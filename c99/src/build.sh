#!/bin/sh
TARGET='
patch-locale-archive.c
wcwidth.c
ctype.c
archive.c
log-helper.c
unicode-helper.c
md5.c
'
gcc -std=c99 -o patch-locale-archive $TARGET
