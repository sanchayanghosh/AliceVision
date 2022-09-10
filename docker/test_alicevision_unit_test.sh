#!/bin/sh

test -e docker/fetch.sh || {
	echo This script must be run from the top level of the AliceVision tree
	exit 1
}


test -z "$AV_VERSION" && AV_VERSION="$(git rev-parse --abbrev-ref HEAD)-$(git rev-parse --short HEAD)"
test -z "$CUDA_VERSION" && CUDA_VERSION=11.0
test -z "$UBUNTU_VERSION" && UBUNTU_VERSION=20.04

AV_BUILD=/tmp/AliceVision_build
docker run --name=alicevision-container -w ${AV_BUILD} --rm alicevision/alicevision:${AV_VERSION}-ubuntu${UBUNTU_VERSION}-cuda${CUDA_VERSION} make test