#!/bin/bash

## Config ###############################
TARGET=0 # 0: PC, 1: ARM64
DEBUG=y  # n: Release mode, y: Debug mode
#########################################

CUR_DIR=`pwd`

if [ $# -eq 1 ] && [ $1 == "clean" ]; then
    # Clean funtrik module
    cd ${CUR_DIR}/mod
    make CONFIG_TARGET=${TARGET} clean
    # Clean funtrik application
    cd ${CUR_DIR}/app
    make CONFIG_TARGET=${TARGET} clean
    # Remove bin directory
    if [ -d ${CUR_DIR}/bin ]; then
        rm -rf ${CUR_DIR}/bin
    fi
    exit 0
fi

# Create bin directory
if [ ! -d ${CUR_DIR}/bin ]; then
    mkdir ${CUR_DIR}/bin
fi

# Build KLFER module
cd ${CUR_DIR}/mod
make CONFIG_TARGET=${TARGET} CONFIG_DEBUG=${DEBUG}
if [ -e ${CUR_DIR}/mod/klfer.ko ]; then
    cp ${CUR_DIR}/mod/klfer.ko ${CUR_DIR}/bin/
fi

# Build KLFER application
cd ${CUR_DIR}/app
make CONFIG_TARGET=${TARGET} CONFIG_DEBUG=${DEBUG}
if [ -e ${CUR_DIR}/app/klferctl ]; then
    cp ${CUR_DIR}/app/klferctl ${CUR_DIR}/bin/
fi

