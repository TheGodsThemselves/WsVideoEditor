#!/bin/bash

show_msg() {
  echo -e "\033[36m$1\033[0m"
}

show_err() {
  echo -e "\033[31m$1\033[0m"
}

v3_0_0="v3.0.0"

os=`uname`
script_path=$(cd `dirname $0`; pwd)
protoc_path=$script_path/tools/protoc
protoc_src=$script_path/protobuf

build_host() {
  mkdir -p $protoc_src/host
  mkdir -p $protoc_path/$1
  cd $protoc_src/host
  ../configure --prefix=$protoc_path/$1 && make -j8 && make install

  if test $? != 0; then
    show_err "Build protobuf failed"
    exit 1
  fi

  cd $script_path
  rm -rf $protoc_src/host
}

build_protobuf() {
  git clone https://github.com/google/protobuf.git

  # We just build for android
  # Ios uses built script!
  show_msg "Building android protobuff source code"
  cd protobuf
  git checkout $v3_0_0
  git cherry-pick bba446b  # fix issue https://github.com/google/protobuf/issues/2063
  ./autogen.sh
  build_host $v3_0_0

  show_msg "Build protobuf complete"
  cd $script_path
  rm -rf protobuf
}

if [ ! -x "$protoc_path/$v3_0_0/bin/protoc" ]; then
  build_protobuf
fi

java_target_path="$script_path/../android/wsvideoeditor-sdk/src/main"
cpp_target_path="$script_path/../sharedcpp/wsvideoeditorsdk/prebuilt_protobuf"

# delete old files
rm $java_target_path/java/com/whensunset/wsvideoeditorsdk/model/*.java
rm $cpp_target_path/*.pb.cc $cpp_target_path/*.pb.h

cd $script_path/../sharedproto
mkdir -p java cpp
$protoc_path/$v3_0_0/bin/protoc *.proto --java_out=java --cpp_out=cpp

cp -r java $java_target_path
mkdir -p  $cpp_target_path
cp cpp/* $cpp_target_path
rm -rf java cpp

show_msg "Build proto complete!"
