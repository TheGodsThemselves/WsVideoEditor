#! /bin/bash

show_msg() {
  echo -e "\033[36m$1\033[0m"
}

show_err() {
  echo -e "\033[31m$1\033[0m"
}

ROOT_DIR=$(cd "$(dirname $0)";pwd)
cd $ROOT_DIR

DEBUG_FLAG=OFF
BUILD_CFG=Release
TEST_CLIENT_FLAG=OFF
TEST_SERVER_FLAG=OFF

help() {
    echo "Usage:"
    echo "  ./build.sh (debug) android"
    echo "  ./build.sh (debug) ios"
    echo "  ./build.sh (debug) mac"
    echo "  ./build.sh (debug) windows"
    echo "  ./build.sh (debug) (test_client) (test_server) linux"
    echo "  ./build.sh (debug) (test_client) (test_server) all"
    echo "  ./build.sh clean"
}

clean_all() {
    git clean -dXf
    git clean -dxf
}

build_ios() {
    show_msg "Build libyuv for ios"
}

build_android() {
    show_msg "Build libyuv for android"
}

build_mac() {
     show_msg "Build libyuv for mac"
}

build_linux() {
     show_msg "Build libyuv for linux"
}

build_windows() {
	show_msg "Build libyuv for windows"
	cd build
	rm -rf CMakeFiles
	rm -rf CMakeCache.txt
	cmake -G "Visual Studio 14 2015" -DCMAKE_BUILD_TYPE=$BUILD_CFG ../
    "C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" "Project.sln" //t:Rebuild  //property:Configuration="Release"
	cd ../
}

if [ $# -eq 0 ]; then
    help
    exit 0
fi

while [ ! $# -eq 0 ]
do
    case $1 in
        debug)
            DEBUG_FLAG=ON
            BUILD_CFG=Debug
            shift
        ;;
        test_client)
            TEST_CLIENT_FLAG=ON
            shift
        ;;
        test_server)
            TEST_SERVER_FLAG=ON
            shift
        ;;
        clean)
            clean_all
            shift
        ;;
        android)
            build_android
            shift
        ;;
        ios)
            build_ios
            shift
        ;;
        mac)
            build_mac
            shift
        ;;
        linux)
            build_linux
            shift
        ;;
        windows)
            build_windows
            shift
        ;;
        all)
            build_android
            build_ios
            build_mac
            build_linux
            shift
        ;;
        *)
            help
            exit 0
        ;;
    esac
done

