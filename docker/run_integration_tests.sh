#!/bin/sh

git clone --depth=1 --branch master https://github.com/alicevision/SfM_quality_evaluation.git 
cd SfM_quality_evaluation/ 
git fetch origin 130d12a0f59ebe1695dafdc42dfe109fe08407ea
git checkout 130d12a0f59ebe1695dafdc42dfe109fe08407ea 
export LD_LIBRARY_PATH=${AV_INSTALL}/lib64:${DEPS_INSTALL_DIR}/lib64:${DEPS_INSTALL_DIR}/lib:${LD_LIBRARY_PATH} 
echo "ldd aliceVision_cameraInit" 
ldd ${AV_INSTALL}/bin/aliceVision_cameraInit 

apt update && apt install -y python2.7 python-is-python2

python EvaluationLauncher.py -s ${AV_INSTALL}/bin -i $PWD/Benchmarking_Camera_Calibration_2008/ -o $PWD/reconstructions/ -r $PWD/results.json -v
rm -rf SfM_quality_evaluation/ 