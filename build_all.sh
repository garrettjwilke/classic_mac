#!/usr/bin/env bash
# if set to true, it will compile the next project if the current project fails to compile
CONTINUE_IF_FAIL=false
PROJECT_DIR=projects
PROJECT_LIST=$(ls $PROJECT_DIR)
for i in $PROJECT_LIST
do
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
done
