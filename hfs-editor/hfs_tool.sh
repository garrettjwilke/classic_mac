#!/usr/bin/env bash

echo "not finished at all. it can extract files from an image."
echo "if files in image have a space, then this tool will fail."
echo ""

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

OUTPUT_DIR=extracted_files

mount_disk() {
  echo "opening disk image: $DISK_IMAGE"
  open $DISK_IMAGE
  sleep 1
  echo "attempting to mount using hmount from hfsutils package"
  for i in $(ls /dev/disk*)
  do
    DISK_CHECK=$(diskutil info $i | grep "$DISK_SIZE" | awk '{print $8}')
    if [ "$DISK_CHECK" == "$DISK_SIZE" ]
    then
      sudo hmount $i
      if [ $? -gt 0 ]
      then
        echo "mounting image failed: $DISK_IMAGE"
        exit
      fi
      DISK_ID=$(basename $i)
      echo "disk image opened and mounted as: \"$DISK_ID\""
    fi
  done
}

unmount_disk() {
  echo "attempting to unmount hfs"
  sudo humount
  if [ $? -gt 0 ]
  then
    echo "unmount of hfs failed"
  else
    echo "hfs drive unmounted"
  fi
  echo ""
  echo "attempting to eject: $i"
  for i in $(ls /dev/disk*)
  do
    DISK_CHECK=$(diskutil info $i | grep "$DISK_SIZE" | awk '{print $8}')
    if [ "$DISK_CHECK" == "$DISK_SIZE" ]
    then
      DISK_ID=$(basename $i)
      diskutil eject $DISK_ID
      if [ $? -gt 0 ]
      then
        echo "disk eject failed: $DISK_ID"
      fi
    fi
  done
}

extract_data() {
  if [ -d $OUTPUT_DIR ]
  then
    rm -rf $OUTPUT_DIR
  fi
  mkdir $OUTPUT_DIR
  echo "getting file list in hfs disk"
  FILE_LIST=$(sudo hls :)
  for i in $FILE_LIST
  do
    echo "attempting to copy from hfs: $i"
    sudo hcopy :$i $OUTPUT_DIR
    if [ $? -gt 0 ]
    then
      echo "copy file failed: $i"
      NEW_FILE_LIST=$(sudo hls :$i)
      mkdir $OUTPUT_DIR/$i
      for j in $NEW_FILE_LIST
      do
        sudo hcopy :$i:$j $OUTPUT_DIR/$i
      done
    else
      echo "file copied to: ${OUTPUT_DIR}/$i"
    fi
    echo ""
  done
}

case $1 in
  "-e"|"--extract")
    OPTION="extract"
    ;;
  "-i"|"--inject")
    OPTION="inject"
    ;;
  *)
    echo "invalid option. see --help"
    exit
    ;;
esac

if [ "$OPTION" == "" ]
then
  echo "no option set. use -e or -i to extract or inject"
  exit
elif [ "$OPTION" == "extract" ]
then
  if [ "$2" == "" ]
  then
    echo "no image file to extract. see --help"
    exit
  else
    DISK_IMAGE=$2
  fi
elif [ "$OPTION" == "inject" ]
then
  if [ "$2" == "" ]
  then
    echo "no directory or files to inject. see --help"
    exit
  fi
fi

DISK_NAME="$(basename $DISK_IMAGE .img)"
DISK_SIZE="$(du $DISK_IMAGE | awk '{print $1}')"

pushd $SCRIPT_DIR

if [ "$(which hmount)" == "" ]
then
  echo "install hfsutils"
  echo "  brew install hfsutils"
  exit
fi

CURRENT_DISK=$(sudo hvol | grep $DISK_NAME | awk '{ print $4 }' | tr -d '"')

if [ "$CURRENT_DISK" == "" ]
then
  echo ""
  echo "no volume is mounted. attempting to mount: $DISK_IMAGE"
  mount_disk
fi

if [ "$OPTION" == "extract" ]
then
  extract_data
fi
unmount_disk
