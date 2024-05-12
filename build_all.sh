#!/usr/bin/env bash
PROJECT_DIR=projects
PROJECT_LIST=$(ls $PROJECT_DIR)
for i in $PROJECT_LIST
do
  ./build.sh ${PROJECT_DIR}/$i
done
