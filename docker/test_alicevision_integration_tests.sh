#!/bin/sh

test -e docker/fetch.sh || {
	echo This script must be run from the top level of the AliceVision tree
	exit 1
}


test -z "$AV_VERSION" && export AV_VERSION="$(git rev-parse --abbrev-ref HEAD)-$(git rev-parse --short HEAD)"
test -z "$CUDA_VERSION" && export CUDA_VERSION=11.0
test -z "$UBUNTU_VERSION" && export UBUNTU_VERSION=20.04

docker run --name=alicevision-container -w /docker -v $(pwd)/docker:/docker --rm alicevision/alicevision:${AV_VERSION}-ubuntu${UBUNTU_VERSION}-cuda${CUDA_VERSION} bash -c 'chmod +x /docker/run_integration_tests.sh && ./run_integration_tests.sh'