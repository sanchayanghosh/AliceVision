// This file is part of the AliceVision project.
// Copyright (c) 2016 AliceVision contributors.
// Copyright (c) 2012 openMVG contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "AlembicExporter.hpp"
#include <aliceVision/version.hpp>

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/Abc/OObject.h>

#include <dependencies/stlplus3/filesystemSimplified/file_system.hpp>

#include <numeric>

namespace aliceVision {
namespace sfm {

using namespace Alembic::Abc;
using namespace Alembic::AbcGeom;

struct AlembicExporter::DataImpl
{
    explicit DataImpl(const std::string& filename)
    : _archive(Alembic::AbcCoreOgawa::WriteArchive(), filename)
    , _topObj(_archive, Alembic::Abc::kTop)
  {
  // create MVG hierarchy
    _mvgRoot = Alembic::Abc::OObject(_topObj, "mvgRoot");
    _mvgCameras = Alembic::Abc::OObject(_mvgRoot, "mvgCameras");
    _mvgCamerasUndefined = Alembic::Abc::OObject(_mvgRoot, "mvgCamerasUndefined");
    _mvgCloud = Alembic::Abc::OObject(_mvgRoot, "mvgCloud");
    _mvgPointCloud = Alembic::Abc::OObject(_mvgCloud, "mvgPointCloud");

    // add version as custom property
    const std::vector<::uint32_t> abcVersion = {1, 1};
    const std::vector<::uint32_t> aliceVisionVersion = {ALICEVISION_VERSION_MAJOR, ALICEVISION_VERSION_MINOR, ALICEVISION_VERSION_REVISION};

    auto userProps = _mvgRoot.getProperties();

    OUInt32ArrayProperty(userProps, "mvg_ABC_version").set(abcVersion);
    OUInt32ArrayProperty(userProps, "mvg_aliceVision_version").set(aliceVisionVersion);

    // hide mvgCamerasUndefined
    Alembic::AbcGeom::CreateVisibilityProperty(_mvgCamerasUndefined, 0).set(Alembic::AbcGeom::kVisibilityHidden);
  }

  /**
   * @brief Add a camera
   * @param[in] name The camera identifier
   * @param[in] view The corresponding view
   * @param[in] pose The camera pose (nullptr if undefined)
   * @param[in] intrinsic The camera intrinsic (nullptr if undefined)
   * @param[in,out] parent The Alembic parent node
   */
  void addCamera(const std::string& name,
               const View& view,
               const geometry::Pose3* pose = nullptr,
               const camera::IntrinsicBase* intrinsic = nullptr,
               const Vec6* uncertainty = nullptr,
               Alembic::Abc::OObject* parent = nullptr);
  
  Alembic::Abc::OArchive _archive;
  Alembic::Abc::OObject _topObj;
  Alembic::Abc::OObject _mvgRoot;
  Alembic::Abc::OObject _mvgCameras;
  Alembic::Abc::OObject _mvgCamerasUndefined;
  Alembic::Abc::OObject _mvgCloud;
  Alembic::Abc::OObject _mvgPointCloud;
  Alembic::AbcGeom::OXform _xform;
  Alembic::AbcGeom::OCamera _camObj;
  Alembic::AbcGeom::OUInt32ArrayProperty _propSensorSize_pix;
  Alembic::AbcGeom::OStringProperty _imagePlane;
  Alembic::AbcGeom::OUInt32Property _propViewId;
  Alembic::AbcGeom::OUInt32Property _propIntrinsicId;
  Alembic::AbcGeom::OStringProperty _mvgIntrinsicType;
  Alembic::AbcGeom::ODoubleArrayProperty _mvgIntrinsicParams;
};


void AlembicExporter::DataImpl::addCamera(const std::string& name,
               const View& view,
               const geometry::Pose3* pose,
               const camera::IntrinsicBase* intrinsic,
               const Vec6* uncertainty,
               Alembic::Abc::OObject* parent)
{
  if(parent == nullptr)
    parent = &_mvgCameras;

  XformSample xformsample;

  // set camera pose
  if(pose != nullptr)
  {
    const Mat3& R = pose->rotation();
    const Vec3& center = pose->center();

    // compensate translation with rotation
    // build transform matrix
    Abc::M44d xformMatrix;
    xformMatrix[0][0] = R(0, 0);
    xformMatrix[0][1] = R(0, 1);
    xformMatrix[0][2] = R(0, 2);
    xformMatrix[1][0] = R(1, 0);
    xformMatrix[1][1] = R(1, 1);
    xformMatrix[1][2] = R(1, 2);
    xformMatrix[2][0] = R(2, 0);
    xformMatrix[2][1] = R(2, 1);
    xformMatrix[2][2] = R(2, 2);
    xformMatrix[3][0] = center(0);
    xformMatrix[3][1] = center(1);
    xformMatrix[3][2] = center(2);
    xformMatrix[3][3] = 1.0;

    // correct camera orientation for alembic
    M44d scale; // by default this is an identity matrix
    scale[0][0] = 1;
    scale[1][1] = -1;
    scale[2][2] = -1;

    xformMatrix = scale * xformMatrix;
    xformsample.setMatrix(xformMatrix);
  }

  std::stringstream ssLabel;
  ssLabel << "camxform_" << std::setfill('0') << std::setw(5) << view.getResectionId() << "_" << view.getPoseId();
  ssLabel << "_" << name << "_" << view.getViewId();

  Alembic::AbcGeom::OXform xform(*parent, ssLabel.str());
  xform.getSchema().set(xformsample);

  OCamera camObj(xform, "camera_" + ssLabel.str());
  auto userProps = camObj.getSchema().getUserProperties();

  // set view custom properties
  if(!view.getImagePath().empty())
    OStringProperty(userProps, "mvg_imagePath").set(view.getImagePath());

  OUInt32Property(userProps, "mvg_viewId").set(view.getViewId());
  OUInt32Property(userProps, "mvg_poseId").set(view.getPoseId());
  OUInt32Property(userProps, "mvg_intrinsicId").set(view.getIntrinsicId());
  OUInt32Property(userProps, "mvg_resectionId").set(view.getResectionId());

  if(view.isPartOfRig())
  {
    OUInt32Property(userProps, "mvg_rigId").set(view.getRigId());
    OUInt32Property(userProps, "mvg_subPoseId").set(view.getSubPoseId());
  }

  // set view metadata
  {
    std::vector<std::string> rawMetadata(view.getMetadata().size() * 2);
    std::map<std::string, std::string>::const_iterator it = view.getMetadata().cbegin();

    for(std::size_t i = 0; i < rawMetadata.size(); i+=2)
    {
      rawMetadata.at(i) = it->first;
      rawMetadata.at(i + 1) = it->second;
      std::advance(it,1);
    }
    OStringArrayProperty(userProps, "mvg_metadata").set(rawMetadata);
  }

  // set intrinsic properties
  const bool isIntrinsicValid = (intrinsic != nullptr &&
                                 intrinsic->isValid() &&
                                 camera::isPinhole(intrinsic->getType()));

  if(isIntrinsicValid)
  {
    const camera::Pinhole* pinhole = dynamic_cast<const camera::Pinhole*>(intrinsic);
    CameraSample camSample;

    // Use a common sensor width if we don't have this information.
    // We chose a full frame 24x36 camera
    float sensorWidth_mm = 36.0;

    if(view.hasMetadata("sensor_width"))
      sensorWidth_mm = std::stof(view.getMetadata("sensor_width"));

    // Take the max of the image size to handle the case where the image is in portrait mode
    const float imgWidth = pinhole->w();
    const float imgHeight = pinhole->h();
    const float sensorWidth_pix = std::max(imgWidth, imgHeight);
    const float focalLength_pix = pinhole->focal();
    const float focalLength_mm = sensorWidth_mm * focalLength_pix / sensorWidth_pix;
    const float pix2mm = sensorWidth_mm / sensorWidth_pix;

    // aliceVision: origin is (top,left) corner and orientation is (bottom,right)
    // ABC: origin is centered and orientation is (up,right)
    // Following values are in cm, hence the 0.1 multiplier
    const float haperture_cm = 0.1 * imgWidth * pix2mm;
    const float vaperture_cm = 0.1 * imgHeight * pix2mm;

    camSample.setFocalLength(focalLength_mm);
    camSample.setHorizontalAperture(haperture_cm);
    camSample.setVerticalAperture(vaperture_cm);

    // Add sensor width (largest image side) in pixels as custom property
    std::vector<::uint32_t> sensorSize_pix = {pinhole->w(), pinhole->h()};

    OUInt32ArrayProperty(userProps, "mvg_sensorSizePix").set(sensorSize_pix);
    OStringProperty(userProps, "mvg_intrinsicType").set(pinhole->getTypeStr());
    ODoubleArrayProperty(userProps, "mvg_intrinsicParams").set(pinhole->getParams());

    camObj.getSchema().set(camSample);
  }

  if(uncertainty)
  {
     std::vector<double> uncertaintyParams(uncertainty->data(), uncertainty->data()+6);
     ODoubleArrayProperty mvg_uncertaintyParams(userProps, "mvg_uncertaintyEigenValues");
     mvg_uncertaintyParams.set(uncertaintyParams);
  }

  if(pose == nullptr || !isIntrinsicValid)
  {
    // hide camera
    Alembic::AbcGeom::CreateVisibilityProperty(xform, 0).set(Alembic::AbcGeom::kVisibilityHidden);
  }
}

AlembicExporter::AlembicExporter(const std::string& filename)
  : _dataImpl(new DataImpl(filename))
{}

AlembicExporter::~AlembicExporter()
{}

std::string AlembicExporter::getFilename() const
{
  return _dataImpl->_archive.getName();
}

void AlembicExporter::addSfM(const SfMData& sfmData, ESfMData flagsPart)
{
  OCompoundProperty userProps = _dataImpl->_mvgRoot.getProperties();

  OStringArrayProperty(userProps, "mvg_featuresFolders").set(sfmData.getRelativeFeaturesFolders());
  OStringArrayProperty(userProps, "mvg_matchesFolders").set(sfmData.getRelativeMatchesFolders());

  if(flagsPart & sfm::ESfMData::STRUCTURE)
  {
    const sfm::LandmarksUncertainty noUncertainty;

    addLandmarks(sfmData.GetLandmarks(),
              (flagsPart & sfm::ESfMData::LANDMARKS_UNCERTAINTY) ? sfmData._landmarksUncertainty : noUncertainty,
              (flagsPart & sfm::ESfMData::OBSERVATIONS));
  }

  if(flagsPart & ESfMData::VIEWS ||
     flagsPart & ESfMData::EXTRINSICS)
  {
    std::map<IndexT, std::map<IndexT, std::vector<IndexT>>> rigsViewIds; //map<rigId,map<poseId,viewId>>

    // save all single views
    for(const auto& viewPair : sfmData.GetViews())
    {
      const View& view = *(viewPair.second);

      if(view.isPartOfRig())
      {
        // save rigId, poseId, viewId in a temporary structure, will process later
        rigsViewIds[view.getRigId()][view.getPoseId()].push_back(view.getViewId());
        continue;
      }
      addSfMSingleCamera(sfmData, view);
    }

    // save rigs views
    for(const auto& rigPair : sfmData.getRigs())
    {
      const IndexT rigId = rigPair.first;
      for(const auto& poseViewIds : rigsViewIds.at(rigId))
        addSfMCameraRig(sfmData, rigId, poseViewIds.second); // add one camera rig per rig pose
    }
  }
}

void AlembicExporter::addSfMSingleCamera(const SfMData& sfmData, const View& view)
{
  const std::string name = stlplus::basename_part(view.getImagePath());
  const geometry::Pose3* pose = (sfmData.existsPose(view)) ? &(sfmData.GetPoses().at(view.getPoseId())) :  nullptr;
  const camera::IntrinsicBase* intrinsic = sfmData.GetIntrinsicPtr(view.getIntrinsicId());

  if(sfmData.IsPoseAndIntrinsicDefined(&view))
    _dataImpl->addCamera(name, view, pose, intrinsic, nullptr, &_dataImpl->_mvgCameras);
  else
    _dataImpl->addCamera(name, view, pose, intrinsic, nullptr, &_dataImpl->_mvgCamerasUndefined);
}

void AlembicExporter::addSfMCameraRig(const SfMData& sfmData, IndexT rigId, const std::vector<IndexT>& viewIds)
{
  const Rig& rig = sfmData.getRigs().at(rigId);
  const std::size_t nbSubPoses = rig.getNbSubPoses();
  if(viewIds.size() != rig.getNbSubPoses())
    throw std::runtime_error("Can't save rig " + std::to_string(rigId) + " in " + getFilename()
                             + ":\n\t- # sub-poses in rig structure: " + std::to_string(nbSubPoses)
                             + "\n\t- # sub-poses find in views: " + std::to_string(viewIds.size()));

  const View& firstView = *(sfmData.GetViews().at(viewIds.front()));

  XformSample xformsample;
  const IndexT rigPoseId = firstView.getPoseId();

  if(sfmData.GetPoses().find(rigPoseId) != sfmData.GetPoses().end())
  {
    // rig pose
    const geometry::Pose3 rigPose = sfmData.GetPoses().at(rigPoseId);

    const aliceVision::Mat3& R = rigPose.rotation();
    const aliceVision::Vec3& center = rigPose.center();

    Abc::M44d xformMatrix;

    // compensate translation with rotation
    // build transform matrix
    xformMatrix[0][0] = R(0, 0);
    xformMatrix[0][1] = R(0, 1);
    xformMatrix[0][2] = R(0, 2);
    xformMatrix[1][0] = R(1, 0);
    xformMatrix[1][1] = R(1, 1);
    xformMatrix[1][2] = R(1, 2);
    xformMatrix[2][0] = R(2, 0);
    xformMatrix[2][1] = R(2, 1);
    xformMatrix[2][2] = R(2, 2);
    xformMatrix[3][0] = center(0);
    xformMatrix[3][1] = center(1);
    xformMatrix[3][2] = center(2);
    xformMatrix[3][3] = 1.0;

    xformsample.setMatrix(xformMatrix);
  }

  std::stringstream ssLabel;
  ssLabel << "rigxform_" << std::setfill('0') << std::setw(5) << rigId << "_" << rigPoseId;

  std::map<bool, Alembic::AbcGeom::OXform> rigObj;
  for(const IndexT viewId : viewIds)
  {
    const View& view = *(sfmData.GetViews().at(viewId));
    const RigSubPose& rigSubPose = rig.getSubPose(view.getSubPoseId());
    const bool isReconstructed = (rigSubPose.status != ERigSubPoseStatus::UNINITIALIZED);
    const std::string name = stlplus::basename_part(view.getImagePath());
    const geometry::Pose3* subPose = isReconstructed ? &(rigSubPose.pose) : nullptr;
    const camera::IntrinsicBase* intrinsic = sfmData.GetIntrinsicPtr(view.getIntrinsicId());

    Alembic::Abc::OObject& parent = isReconstructed ? _dataImpl->_mvgCameras : _dataImpl->_mvgCamerasUndefined;

    if(rigObj.find(isReconstructed) == rigObj.end())
    {
      // The first time we declare a view, we have to create a RIG entry.
      // The RIG entry will be different if the view is reconstructed or not.
      rigObj[isReconstructed] = Alembic::AbcGeom::OXform(parent, ssLabel.str());
      auto schema = rigObj.at(isReconstructed).getSchema();
      schema.set(xformsample);
      {
        auto userProps = schema.getUserProperties();
        OUInt32Property(userProps, "mvg_rigId").set(rigId);
        OUInt32Property(userProps, "mvg_poseId").set(rigPoseId);
        OUInt16Property(userProps, "mvg_nbSubPoses").set(nbSubPoses);
      }
    }
    _dataImpl->addCamera(name, view, subPose, intrinsic, nullptr, &(rigObj.at(isReconstructed)));
  }
}

void AlembicExporter::addLandmarks(const Landmarks& landmarks, const sfm::LandmarksUncertainty &landmarksUncertainty, bool withVisibility)
{
  if(landmarks.empty())
    return;

  // Fill vector with the values taken from AliceVision
  std::vector<V3f> positions;
  std::vector<Imath::C3f> colors;
  std::vector<Alembic::Util::uint32_t> descTypes;
  positions.reserve(landmarks.size());
  descTypes.reserve(landmarks.size());

  // For all the 3d points in the hash_map
  for(const auto& landmark : landmarks)
  {
    const Vec3& pt = landmark.second.X;
    const image::RGBColor& color = landmark.second.rgb;
    positions.emplace_back(pt[0], pt[1], pt[2]);
    colors.emplace_back(color.r()/255.f, color.g()/255.f, color.b()/255.f);
    descTypes.emplace_back(static_cast<Alembic::Util::uint8_t>(landmark.second.descType));
  }

  std::vector<Alembic::Util::uint64_t> ids(positions.size());
  std::iota(begin(ids), end(ids), 0);

  OPoints partsOut(_dataImpl->_mvgPointCloud, "particleShape1");
  OPointsSchema &pSchema = partsOut.getSchema();

  OPointsSchema::Sample psamp(std::move(V3fArraySample(positions)), std::move(UInt64ArraySample(ids)));
  pSchema.set(psamp);

  OCompoundProperty arbGeom = pSchema.getArbGeomParams();

  C3fArraySample cval_samp(&colors[0], colors.size());
  OC3fGeomParam::Sample color_samp(cval_samp, kVertexScope);

  OC3fGeomParam rgbOut(arbGeom, "color", false, kVertexScope, 1);
  rgbOut.set(color_samp);

  OCompoundProperty userProps = pSchema.getUserProperties();

  OUInt32ArrayProperty(userProps, "mvg_describerType").set(descTypes);

  if(withVisibility)
  {
    std::vector<::uint32_t> visibilitySize;
    visibilitySize.reserve(positions.size());
    for(const auto& landmark : landmarks)
    {
      visibilitySize.emplace_back(landmark.second.observations.size());
    }
    std::size_t nbObservations = std::accumulate(visibilitySize.begin(), visibilitySize.end(), 0);

    // Use std::vector<::uint32_t> and std::vector<float> instead of std::vector<V2i> and std::vector<V2f>
    // Because Maya don't import them correctly
    std::vector<::uint32_t> visibilityIds;
    visibilityIds.reserve(nbObservations*2);
    std::vector<float>featPos2d;
    featPos2d.reserve(nbObservations*2);

    for(Landmarks::const_iterator itLandmark = landmarks.cbegin(), itLandmarkEnd = landmarks.cend();
       itLandmark != itLandmarkEnd; ++itLandmark)
    {
      const Observations& observations = itLandmark->second.observations;
      for(const auto vObs: observations )
      {
        const Observation& obs = vObs.second;
        // (View ID, Feature ID)
        visibilityIds.emplace_back(vObs.first);
        visibilityIds.emplace_back(obs.id_feat);
        // Feature 2D position (x, y))
        featPos2d.emplace_back(obs.x[0]);
        featPos2d.emplace_back(obs.x[1]);
      }
    }

    OUInt32ArrayProperty(userProps, "mvg_visibilitySize" ).set(visibilitySize);
    OUInt32ArrayProperty(userProps, "mvg_visibilityIds" ).set(visibilityIds); // (viewID, featID)
    OFloatArrayProperty(userProps, "mvg_visibilityFeatPos" ).set(featPos2d); // feature position (x,y)
  }
  if(!landmarksUncertainty.empty())
  {
    std::vector<V3d> uncertainties;

    std::size_t indexLandmark = 0;
    for (Landmarks::const_iterator itLandmark = landmarks.begin(); itLandmark != landmarks.end(); ++itLandmark, ++indexLandmark)
    {
      const IndexT idLandmark = itLandmark->first;
      const Vec3& u = landmarksUncertainty.at(idLandmark);
      uncertainties.emplace_back(u[0], u[1], u[2]);
    }
    // Uncertainty eigen values (x,y,z)
    OV3dArrayProperty propUncertainty(userProps, "mvg_uncertaintyEigenValues");
    propUncertainty.set(uncertainties);
  }
}

void AlembicExporter::addCamera(const std::string& name,
                                const View& view,
                                const geometry::Pose3* pose,
                                const camera::IntrinsicBase* intrinsic,
                                const Vec6* uncertainty)
{
  _dataImpl->addCamera(name, view, pose, intrinsic, uncertainty);
}

void AlembicExporter::initAnimatedCamera(const std::string& cameraName)
{
  // Sample the time in order to have one keyframe every frame
  // nb: it HAS TO be attached to EACH keyframed properties
  TimeSamplingPtr tsp( new TimeSampling(1.0 / 24.0, 1.0 / 24.0) );
  
  // Create the camera transform object
  std::stringstream ss;
  ss << cameraName;
  _dataImpl->_xform = Alembic::AbcGeom::OXform(_dataImpl->_mvgCameras, "animxform_" + ss.str());
  _dataImpl->_xform.getSchema().setTimeSampling(tsp);
  
  // Create the camera parameters object (intrinsics & custom properties)
  _dataImpl->_camObj = OCamera(_dataImpl->_xform, "animcam_" + ss.str());
  _dataImpl->_camObj.getSchema().setTimeSampling(tsp);
  
  // Add the custom properties
  auto userProps = _dataImpl->_camObj.getSchema().getUserProperties();
  // Sensor size
  _dataImpl->_propSensorSize_pix = OUInt32ArrayProperty(userProps, "mvg_sensorSizePix", tsp);
  // Image path
  _dataImpl->_imagePlane = OStringProperty(userProps, "mvg_imagePath", tsp);
  // View id
  _dataImpl->_propViewId = OUInt32Property(userProps, "mvg_viewId", tsp);
  // Intrinsic id
  _dataImpl->_propIntrinsicId = OUInt32Property(userProps, "mvg_intrinsicId", tsp);
  // Intrinsic type (ex: PINHOLE_CAMERA_RADIAL3)
  _dataImpl->_mvgIntrinsicType = OStringProperty(userProps, "mvg_intrinsicType", tsp);
  // Intrinsic parameters
  _dataImpl->_mvgIntrinsicParams = ODoubleArrayProperty(userProps, "mvg_intrinsicParams", tsp);
}

void AlembicExporter::addCameraKeyframe(const geometry::Pose3& pose,
                                        const camera::Pinhole* cam,
                                        const std::string& imagePath,
                                        IndexT viewId,
                                        IndexT intrinsicId,
                                        float sensorWidth_mm)
{
  const aliceVision::Mat3& R = pose.rotation();
  const aliceVision::Vec3& center = pose.center();
  // POSE
  // Compensate translation with rotation
  // Build transform matrix
  Abc::M44d xformMatrix;
  xformMatrix[0][0] = R(0, 0);
  xformMatrix[0][1] = R(0, 1);
  xformMatrix[0][2] = R(0, 2);
  xformMatrix[1][0] = R(1, 0);
  xformMatrix[1][1] = R(1, 1);
  xformMatrix[1][2] = R(1, 2);
  xformMatrix[2][0] = R(2, 0);
  xformMatrix[2][1] = R(2, 1);
  xformMatrix[2][2] = R(2, 2);
  xformMatrix[3][0] = center(0);
  xformMatrix[3][1] = center(1);
  xformMatrix[3][2] = center(2);
  xformMatrix[3][3] = 1.0;

  // Correct camera orientation for alembic
  M44d scale;
  scale[0][0] = 1;
  scale[1][1] = -1;
  scale[2][2] = -1;
  xformMatrix = scale*xformMatrix;

  // Create the XformSample
  XformSample xformsample;
  xformsample.setMatrix(xformMatrix);
  
  // Attach it to the schema of the OXform
  _dataImpl->_xform.getSchema().set(xformsample);
  
  // Camera intrinsic parameters
  CameraSample camSample;

  // Take the max of the image size to handle the case where the image is in portrait mode 
  const float imgWidth = cam->w();
  const float imgHeight = cam->h();
  const float sensorWidth_pix = std::max(imgWidth, imgHeight);
  const float focalLength_pix = cam->focal();
  const float focalLength_mm = sensorWidth_mm * focalLength_pix / sensorWidth_pix;
  const float pix2mm = sensorWidth_mm / sensorWidth_pix;

  // aliceVision: origin is (top,left) corner and orientation is (bottom,right)
  // ABC: origin is centered and orientation is (up,right)
  // Following values are in cm, hence the 0.1 multiplier
  const float haperture_cm = 0.1 * imgWidth * pix2mm;
  const float vaperture_cm = 0.1 * imgHeight * pix2mm;

  camSample.setFocalLength(focalLength_mm);
  camSample.setHorizontalAperture(haperture_cm);
  camSample.setVerticalAperture(vaperture_cm);
  
  // Add sensor size in pixels as custom property
  std::vector<::uint32_t> sensorSize_pix = {cam->w(), cam->h()};
  _dataImpl->_propSensorSize_pix.set(sensorSize_pix);
  
  // Set custom attributes
  // Image path
  _dataImpl->_imagePlane.set(imagePath);

  // View id
  _dataImpl->_propViewId.set(viewId);
  // Intrinsic id
  _dataImpl->_propIntrinsicId.set(intrinsicId);
  // Intrinsic type
  _dataImpl->_mvgIntrinsicType.set(cam->getTypeStr());
  // Intrinsic parameters
  std::vector<double> intrinsicParams = cam->getParams();
  _dataImpl->_mvgIntrinsicParams.set(intrinsicParams);
  
  // Attach intrinsic parameters to camera object
  _dataImpl->_camObj.getSchema().set(camSample);
}

void AlembicExporter::jumpKeyframe(const std::string& imagePath)
{
  if(_dataImpl->_xform.getSchema().getNumSamples() == 0)
  {
    camera::Pinhole default_intrinsic;
    this->addCameraKeyframe(geometry::Pose3(), &default_intrinsic, imagePath, 0, 0);
  }
  else
  {
    _dataImpl->_xform.getSchema().setFromPrevious();
    _dataImpl->_camObj.getSchema().setFromPrevious();
  }
}

} //namespace sfm
} //namespace aliceVision
