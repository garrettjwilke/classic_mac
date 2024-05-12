#!/usr/bin/env bash

PROJECT_DIR=projects

message() {
  TITLE=$1
  MESSAGE_1=$2
  cat << EOF
-- $TITLE --

  $MESSAGE_1

EOF
}

get_name() {
  message "new project creator thingy" "Type the name of the new project you want to create:"
  read PROJECT_NAME
  echo $PROJECT_DIR/$PROJECT_NAME
  if [ -d $PROJECT_DIR/$PROJECT_NAME ]
  then
    if [ $PROJECT_NAME == "testing" ]
    then
      rm -rf $PROJECT_DIR/$PROJECT_NAME
    else
      clear
      message "project directory exists" "The project name you have chosen already exists"
      get_name
    fi
  fi
}

create_cmake() {
  echo set\(APP_NAME\ \"$PROJECT_NAME\"\) > $FINAL_DIR/CMakeLists.txt
  echo add_application\(\$\{APP_NAME\} >> $FINAL_DIR/CMakeLists.txt
  echo \ \ \ \ \$\{APP_NAME\}.c >> $FINAL_DIR/CMakeLists.txt
  echo \ \ \ \ CONSOLE >> $FINAL_DIR/CMakeLists.txt
  echo \ \ \ \ \) >> $FINAL_DIR/CMakeLists.txt
  echo "" >> $FINAL_DIR/CMakeLists.txt
  echo set_target_properties\(\$\{APP_NAME\}\ PROPERTIES COMPILE_OPTIONS\ -ffunction-sections\) >> $FINAL_DIR/CMakeLists.txt
  echo if\(CMAKE_SYSTEM_NAME MATCHES\ Retro68\) >> $FINAL_DIR/CMakeLists.txt
  echo \ \ \ \ set_target_properties\(\$\{APP_NAME\}\ PROPERTIES LINK_FLAGS \"-Wl,-gc-sections\ -Wl,--mac-strip-macsbug\"\) >> $FINAL_DIR/CMakeLists.txt
  echo else\(\) >> $FINAL_DIR/CMakeLists.txt
  echo \ \ \ \ set_target_properties\(\$\{APP_NAME\}\ PROPERTIES\ LINK_FLAGS\ \"-Wl,-gc-sections\"\) >> $FINAL_DIR/CMakeLists.txt
  echo endif\(\) >> $FINAL_DIR/CMakeLists.txt
}

create_app() {
  PROJECT_APP=$FINAL_DIR/${PROJECT_NAME}.c
  echo \#include\ \<stdio.h\> > $PROJECT_APP
  echo "" >> $PROJECT_APP
  echo int\ main\(void\) >> $PROJECT_APP
  echo \{ >> $PROJECT_APP
  echo \ \ printString\(\"$PROJECT_NAME\\ntest\ string\"\)\; >> $PROJECT_APP
  echo \ \ printf\(\"\\npress\ any\ key\ to\ exit\\n\\n\"\)\; >> $PROJECT_APP
  echo \ \ getchar\(\)\; >> $PROJECT_APP
  echo \ \ return\ 0\; >> $PROJECT_APP
  echo \} >> $PROJECT_APP
}

get_name
FINAL_DIR=$PROJECT_DIR/$PROJECT_NAME
mkdir -p $FINAL_DIR
create_cmake
create_app
