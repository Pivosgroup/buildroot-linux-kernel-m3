#!/bin/bash

#
# Build U-Boot image when `mkimage' tool is available.
#

if [ -z ${MKUIMAGE}  ];then
MKUIMAGE=$(type -path "${CROSS_COMPILE}mkimage")
fi
if [ -z "${MKUIMAGE}" ]; then
	MKUIMAGE=$(type -path mkimage)
	if [ -z "${MKUIMAGE}" ]; then
		# Doesn't exist
		echo '"mkimage" command not found - U-Boot images will not be built' >&2
		exit 0;
	fi
fi

# Call "mkimage" to create U-Boot image
${MKUIMAGE} "$@"
