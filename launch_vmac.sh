#!/usr/bin/env bash

APP_LOCATION="/Applications/Mini vMac.app"
BOOT_DISK=minivmac/vmac-boot-disk.dsk

if ! [ -f $BOOT_DISK ]
then
  pushd minivmac &>/dev/null
  tar -xzf vmac_stuffs.tar.gz
  popd
fi
open $BOOT_DISK -a "$APP_LOCATION"
