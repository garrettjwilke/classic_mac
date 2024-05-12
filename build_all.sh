#!/usr/bin/env bash
# if set to true, it will only create a makefile and not build
MAKEFILE_ONLY=false

if [ "$1" == "--makefile_only" ]
then
  MAKEFILE_ONLY=true
fi

# if set to true, it will compile the next project if the current project fails to compile
CONTINUE_IF_FAIL=false
PROJECT_DIR=projects
PROJECT_LIST=$(ls $PROJECT_DIR)
for i in $PROJECT_LIST
do
  if [ $MAKEFILE_ONLY == true ]
  then
    ./create_makefile.sh ${PROJECT_DIR}/$i
  else
    ./build.sh ${PROJECT_DIR}/$i
    if [ $? -gt 0 ]
    then
      echo "build failed"
      echo "project: ${PROJECT_DIR}/$i"
      if ! [ $CONTINUE_IF_FAIL ]
      then
        exit
      fi
    fi
  fi
done
