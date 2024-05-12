#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd $SCRIPT_DIR
APP_LOCATION="/Applications/Mini vMac.app"
BOOT_DISK=$SCRIPT_DIR/minivmac/boot-disk.dsk

if ! [ -f $BOOT_DISK ]
then
  pushd minivmac &>/dev/null
  tar -xzf vmac_stuffs.tar.gz
  popd &>/dev/null
fi
open $BOOT_DISK -a "$APP_LOCATION"
