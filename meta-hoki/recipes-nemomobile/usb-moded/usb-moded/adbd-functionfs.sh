#!/bin/sh
set -eu
# FunctionFS is registered only after a configfs ffs instance is created.
# This unit runs before adbd's gadget setup, including on a cold boot.
config=/sys/kernel/config
if ! mountpoint -q "$config"; then
    mkdir -p "$config"
    mount -t configfs none "$config"
fi
mkdir -p "$config/usb_gadget/g1/functions/ffs.adb"
mkdir -p /dev/usb-ffs/adb
if ! mountpoint -q /dev/usb-ffs/adb; then
    mount -t functionfs adb /dev/usb-ffs/adb
fi
