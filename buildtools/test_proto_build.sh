#!/bin/bash
cd ../sharedproto

rm -rf ../aaa
mkdir ../aaa/
protoc --objc_out=../aaa *.proto

if [ $? -eq 0 ]
then
  echo OK!
else
  echo Failed.
fi

rm -rf ../aaa
