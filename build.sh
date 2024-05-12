#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# set the project name based on the project directory name
PROJECT_NAME=$(basename $1)

# set the directory that the project will be compiled to
BUILD_DIR=$SCRIPT_DIR/target

# the final build floppies are set here
# if the directory does not exist, it will be created
FLOPPY_DIR=${BUILD_DIR}/00_floppy_images

# it is very important to have Retro68 already compiled.
# after compiling Retro68, set the Retro68-build directory here
TOOLCHAIN_DIR=${SCRIPT_DIR}/Retro68-build

# function to convert compiled disk images to bluescsi drives
convert_image() {
  IMAGE_FILE=$1
  if ! [ $(which djjr) ]
  then
    echo "djjr is not installed."
    exit 1
  fi
  OUTPUT_NAME=$(basename -s ".dsk" $IMAGE_FILE)
  if ! [ -d ${BUILD_DIR}/00_bluescsi_images ]
  then
    mkdir ${BUILD_DIR}/00_bluescsi_images
  fi
  djjr convert to-device $FLOPPY_DIR/${IMAGE_FILE} ${BUILD_DIR}/00_bluescsi_images/FDx-${OUTPUT_NAME}.hda
  if [ $? -gt 0 ]
  then
    echo "conversion failed. not sure why but it did."
    echo "image attempted: $FLOPPY_DIR/$IMAGE_FILE"
    exit 1
  fi
}

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

# check if the build directory exists and remove it to rebuild
if [ -d ${BUILD_DIR}/$PROJECT_NAME ]
then
  rm -rf ${BUILD_DIR}/$PROJECT_NAME
fi

# make new build directory and cd into it
mkdir -p ${BUILD_DIR}/$PROJECT_NAME
pushd ${BUILD_DIR}/$PROJECT_NAME

# build the files
cmake $SCRIPT_DIR/$1 -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_DIR/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make

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

for i in $FLOPPY_FILES
do
  cp $i $FLOPPY_DIR
  if [ "$(which djjr2)" != "" ]
  then
    convert_image $i
    BLUESCSI_CONVERT=true
  else
    BLUESCSI_CONVERT=false
  fi
done

popd &>/dev/null

echo ""
echo "build complete. floppies copied to: $FLOPPY_DIR"
if [ $BLUESCSI_CONVERT == true ]
then
  echo "bluescsi images copied to: ${BUILD_DIR}/00_bluescsi_images"
fi

