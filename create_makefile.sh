#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

SOURCE_DIR=$1

OUTPUT_DIR=build

RETRO68_DIR=${SCRIPT_DIR}/Retro68-build

# check if user input a path to the source they wish to compile
if [ $# -ne 1 ]
then
  echo "provide source directory. example below:"
  echo "./create_makefile.sh path/to/project"
  exit
fi

# check if the directory the user input actually exists
if ! [ -d "$SOURCE_DIR" ]
then
  echo $SOURCE_DIR
  echo "the project directory does not exist"
  exit
fi

PROJECT_NAME=$(basename $SOURCE_DIR)

pushd "$SOURCE_DIR" &>/dev/null

if ! [ -f CMakeLists.txt ]
then
  echo "CMakeLists.txt file does not exist. exiting."
  exit
fi

popd &>/dev/null

FINAL_DIR=${OUTPUT_DIR}/makefile_$PROJECT_NAME

if [ -d $FINAL_DIR ]
then
  rm -rf $FINAL_DIR
fi
mkdir -p $FINAL_DIR
pushd $FINAL_DIR &>/dev/null

cmake ../../$SOURCE_DIR -DCMAKE_TOOLCHAIN_FILE=$RETRO68_DIR/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
popd

echo "Makefile created in: $FINAL_DIR"

