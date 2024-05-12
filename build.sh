#!/usr/bin/env bash

BLUESCSI_CONVERT=true

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# set the project name based on the project directory name
PROJECT_NAME=$(basename $1)

# set the directory that the project will be compiled to
BUILD_DIR=$SCRIPT_DIR/target/$PROJECT_NAME

# the final build floppies are set here
# if the directory does not exist, it will be created
FLOPPY_DIR=$SCRIPT_DIR/target/floppy_images

# it is very important to have Retro68 already compiled.
# after compiling Retro68, set the Retro68-build directory here
TOOLCHAIN_DIR=${SCRIPT_DIR}/Retro68-build

# check if the Retro68-build toolchain exsists
if ! [ -d $TOOLCHAIN_DIR ]
then
  echo "no toolchain directory."
  echo "build toolchain at github.com/autc04/Retro68"
  exit 1
fi

# check if user input a path to the source they wish to compile
if [ $# -ne 1 ]
then
  echo "provide source directory. example below:"
  echo "./build.sh path/to/project"
  exit 1
fi

# check if the directory the user input actually exists
if [ ! -d "$1" ]
then
  echo $1
  echo "the project directory does not exist"
  exit 1
fi

# change working directory to the project name provided by user
pushd "$1" &>/dev/null

# check if the build directory exists and remove it to rebuild
if [ -d $BUILD_DIR ]
then
  rm -rf $BUILD_DIR
fi

# make new build directory and cd into it
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# build the files
cmake $SCRIPT_DIR/$1 -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_DIR/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make

# function to convert compiled disk images to bluescsi drives
convert_image() {
  IMAGE_FILE=$1
  if ! [ $(which djjr) ]
  then
    echo "djjr is not installed."
    exit 1
  fi
  OUTPUT_NAME=$(basename -s ".dsk" $IMAGE_FILE)
  if ! [ -d ${FLOPPY_DIR}/00_bluescsi_images ]
  then
    mkdir ${FLOPPY_DIR}/00_bluescsi_images
  fi
  djjr convert to-device $FLOPPY_DIR/${IMAGE_FILE} ${FLOPPY_DIR}/00_bluescsi_images/FDx-${OUTPUT_NAME}.hda
  if [ $? -gt 0 ]
  then
    echo "conversion failed. not sure why but it did."
    echo "image attempted: $FLOPPY_DIR/$IMAGE_FILE"
    exit 1
  fi
}

# check if the floppy directory exists before copying over the compiled disk images
if ! [ -d $FLOPPY_DIR ]
then
  mkdir $FLOPPY_DIR
fi

# find all compiled .dsk files
FLOPPY_FILES=$(ls *.dsk)
if [ "$FLOPPY_FILES" == "" ]
then
  echo "build floppy was not created"
  exit 1
fi

# copy disk images to the floppy folder then ask if user wants to convert to bluescsi
for i in $FLOPPY_FILES
do
  cp $i $FLOPPY_DIR
  if $BLUESCSI_CONVERT
  then
    convert_image $i
  fi
done

popd &>/dev/null

echo ""
echo "build complete. floppies coped to: $FLOPPY_DIR"
