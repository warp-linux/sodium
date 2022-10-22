#!/bin/sh

tar xzpf tmp.tar.gz

cp -r bin/ /usr/
cp -r usr/ /
cp -r etc/ /
cp -r opt/ /
cp -r var/ /
cp -r home/ /

rm -rf tmp.tar.gz
