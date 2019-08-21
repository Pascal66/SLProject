//#############################################################################
//  File:      SLCVCalibration.cpp
//  Author:    Michael Goettlicher, Marcus Hudritsch
//  Date:      Winter 2016
//  Codestyle: https://github.com/cpvrlab/SLProject/wiki/SLProject-Coding-Style
//  Copyright: Marcus Hudritsch, Michael Goettlicher
//             This software is provide under the GNU General Public License
//             Please visit: http://opensource.org/licenses/GPL-3.0
//#############################################################################

#include <stdafx.h> // Must be the 1st include followed by  an empty line // precompiled headers

/*
The OpenCV library version 3.4 or above with extra module must be present.
If the application captures the live video stream with OpenCV you have
to define in addition the constant SL_USES_CVCAPTURE.
All classes that use OpenCV begin with SLCV.
See also the class docs for SLCVCapture, SLCVCalibration and SLCVTracked
for a good top down information.
*/

#include <SLApplication.h>
#include <SLCVCalibration.h>
#include <SLCVCapture.h>
#include <Utils.h>
#include <ftplib.h>

using namespace cv;
using namespace std;
//-----------------------------------------------------------------------------
off64_t ftpUploadSizeMax = 0;
//-----------------------------------------------------------------------------
//! Calibration Upload callback for progress feedback
int ftpCallbackUpload(off64_t xfered, void* arg)
{
    if (ftpUploadSizeMax)
    {
        int xferedPC = (int)((float)xfered / (float)ftpUploadSizeMax * 100.0f);
        cout << "Bytes saved: " << xfered << " (" << xferedPC << ")" << endl;
        SLApplication::jobProgressNum(xferedPC);
    }
    else
    {
        cout << "Bytes saved: " << xfered << endl;
    }
    return xfered ? 1 : 0;
}
//-----------------------------------------------------------------------------
//! Default path for calibration files
//! Is overwritten in slCreateAppAndScene.
SLstring SLCVCalibration::calibIniPath = SLstring(SL_PROJECT_ROOT) + "/data/calibrations/";
//-----------------------------------------------------------------------------
//! FTP credentials for calibration up- and download
const SLstring SLCVCalibration::FTP_HOST = "pallas.bfh.ch:21";
const SLstring SLCVCalibration::FTP_USER = "upload";
const SLstring SLCVCalibration::FTP_PWD  = "FaAdbD3F2a";
const SLstring SLCVCalibration::FTP_DIR  = "calibrations";
//-----------------------------------------------------------------------------
//! Increase the _CALIBFILEVERSION each time you change the file format
// Version 6, Date: 6.JUL.2019: Added device parameter from Android
const SLint SLCVCalibration::_CALIBFILEVERSION = 6;
//-----------------------------------------------------------------------------
SLCVCalibration::SLCVCalibration()
  : _state(CS_uncalibrated),
    _cameraFovHDeg(0.0f),
    _cameraFovVDeg(0.0f),
    _calibFileName(""), // is set in load
    _calibParamsFileName("calib_in_params.yml"),
    _calibFixPrincipalPoint(true),
    _calibFixAspectRatio(true),
    _calibZeroTangentDist(true),
    _boardSize(0, 0),
    _boardSquareMM(0.0f),
    _numOfImgsToCapture(0),
    _numCaptured(0),
    _reprojectionError(-1.0f),
    _camSizeIndex(-1),
    _showUndistorted(false),
    _calibrationTime("-")
{
}
//-----------------------------------------------------------------------------
//! Resets the calibration to the uncalibrated state
void SLCVCalibration::clear()
{
    _numCaptured       = 0;
    _reprojectionError = -1.0f;
    _imagePoints.clear();
    _cameraFovHDeg   = 0.0f;
    _cameraFovVDeg   = 0.0f;
    _calibrationTime = "-";
    _undistortMapX.release();
    _undistortMapY.release();
    _state = CS_uncalibrated;
}
//-----------------------------------------------------------------------------
//! Loads the calibration information from the config file
bool SLCVCalibration::load(const SLstring& calibDir,
                           const SLstring& calibFileName,
                           SLbool          mirrorHorizontally,
                           SLbool          mirrorVertically)
{
    _calibDir      = Utils::unifySlashes(calibDir);
    _calibFileName = calibFileName;
    _isMirroredH   = mirrorHorizontally;
    _isMirroredV   = mirrorVertically;

    //load camera parameter
    SLstring fullPathAndFilename = _calibDir + _calibFileName;

    // try to download from ftp if no calibration exists locally
    if (!Utils::fileExists(fullPathAndFilename))
        downloadCalibration(fullPathAndFilename);

    // try to open the local calibration file
    FileStorage fs(fullPathAndFilename, FileStorage::READ);
    if (!fs.isOpened())
    {
        SL_LOG("Calibration     : %s\n", calibFileName.c_str());
        SL_LOG("Calib. created  : No. Calib. will be estimated\n");
        _numCaptured       = 0;
        _isMirroredH       = mirrorHorizontally;
        _isMirroredV       = mirrorVertically;
        _reprojectionError = 0;
        _calibrationTime   = "-";
        _state             = CS_uncalibrated;
        _camSizeIndex      = -1;
        return false;
    }

    // Reset if new file format version is available
    SLint calibFileVersion = 0;
    fs["CALIBFILEVERSION"] >> calibFileVersion;
    if (calibFileVersion < _CALIBFILEVERSION)
    {
        _numCaptured            = 0;
        _isMirroredH            = mirrorHorizontally;
        _isMirroredV            = mirrorVertically;
        _calibFixAspectRatio    = true;
        _calibFixPrincipalPoint = true;
        _calibZeroTangentDist   = true;
        _reprojectionError      = -1;
        _calibrationTime        = "-";
        _state                  = CS_uncalibrated;
        _camSizeIndex           = -1;
    }
    else
    {
        fs["imageSizeWidth"] >> _imageSize.width;
        fs["imageSizeHeight"] >> _imageSize.height;
        fs["numCaptured"] >> _numCaptured;
        fs["isMirroredH"] >> _isMirroredH;
        fs["isMirroredV"] >> _isMirroredV;
        fs["calibFixAspectRatio"] >> _calibFixAspectRatio;
        fs["calibFixPrincipalPoint"] >> _calibFixPrincipalPoint;
        fs["calibZeroTangentDist"] >> _calibZeroTangentDist;
        fs["cameraMat"] >> _cameraMat;
        fs["distortion"] >> _distortion;
        fs["reprojectionError"] >> _reprojectionError;
        fs["calibrationTime"] >> _calibrationTime;
        fs["camSizeIndex"] >> _camSizeIndex;
        _state = _numCaptured ? CS_calibrated : CS_uncalibrated;
    }

    // close the input file
    fs.release();

    //calculate FOV and undistortion maps
    if (_state == CS_calibrated)
    {
        calcCameraFov();
        buildUndistortionMaps();
    }

    SL_LOG("Calib. loaded  : %s\n", fullPathAndFilename.c_str());
    SL_LOG("Calib. created : %s\n", _calibrationTime.c_str());
    SL_LOG("Camera FOV H/V : %3.1f/%3.1f\n", _cameraFovVDeg, _cameraFovHDeg);

    return true;
}
//-----------------------------------------------------------------------------
//! Saves the camera calibration parameters to the config file
void SLCVCalibration::save()
{
    SLGLState*      stateGL             = SLGLState::instance();
    SLstring        fullPathAndFilename = _calibDir + _calibFileName;
    cv::FileStorage fs(fullPathAndFilename, FileStorage::WRITE);

    if (!fs.isOpened())
    {
        SL_LOG("Failed to write calib. %s\n", fullPathAndFilename.c_str());
        return;
    }

    SLchar buf[1024];
    if (_calibFlags)
    {
        sprintf(buf,
                "flags:%s%s%s%s",
                _calibFlags & CALIB_USE_INTRINSIC_GUESS ? " +use_intrinsic_guess" : "",
                _calibFlags & CALIB_FIX_ASPECT_RATIO ? " +fix_aspectRatio" : "",
                _calibFlags & CALIB_FIX_PRINCIPAL_POINT ? " +fix_principal_point" : "",
                _calibFlags & CALIB_ZERO_TANGENT_DIST ? " +zero_tangent_dist" : "");
        fs.writeComment(buf, 0);
    }

    fs << "CALIBFILEVERSION" << _CALIBFILEVERSION;
    fs << "calibrationTime" << _calibrationTime;
    fs << "computerUser" << SLApplication::computerUser;
    fs << "computerName" << SLApplication::computerName;
    fs << "computerBrand" << SLApplication::computerBrand;
    fs << "computerModel" << SLApplication::computerModel;
    fs << "computerArch" << SLApplication::computerArch;
    fs << "computerOS" << SLApplication::computerOS;
    fs << "computerOSVer" << SLApplication::computerOSVer;
    fs << "OpenGLVersion" << stateGL->glVersionNO();
    fs << "OpenGLVendor" << stateGL->glVendor();
    fs << "OpenGLRenderer" << stateGL->glRenderer();
    fs << "GLSLVersion" << stateGL->glSLVersionNO();
    fs << "SLProjectVersion" << SLApplication::version;
    fs << "imageSizeWidth" << _imageSize.width;
    fs << "imageSizeHeight" << _imageSize.height;
    fs << "boardSizeWidth" << _boardSize.width;   // do not reload
    fs << "boardSizeHeight" << _boardSize.height; // do not reload
    fs << "boardSquareMM" << _boardSquareMM;      // do not reload
    fs << "numCaptured" << _numCaptured;
    fs << "calibFlags" << _calibFlags;
    fs << "isMirroredH" << _isMirroredH;
    fs << "isMirroredV" << _isMirroredV;
    fs << "calibFixAspectRatio" << _calibFixAspectRatio;
    fs << "calibFixPrincipalPoint" << _calibFixPrincipalPoint;
    fs << "calibZeroTangentDist" << _calibZeroTangentDist;
    fs << "cameraMat" << _cameraMat;
    fs << "distortion" << _distortion;
    fs << "reprojectionError" << _reprojectionError;
    fs << "cameraFovVDeg" << _cameraFovVDeg;
    fs << "cameraFovHDeg" << _cameraFovHDeg;
    fs << "camSizeIndex" << _camSizeIndex;
    fs << "DeviceLensFocalLength" << SLApplication::deviceParameter["DeviceLensFocalLength"];
    fs << "DeviceLensAperture" << SLApplication::deviceParameter["DeviceLensAperture"];
    fs << "DeviceLensFocusDistanceCalibration" << SLApplication::deviceParameter["DeviceLensFocusDistanceCalibration"];
    fs << "DeviceLensMinimumFocusDistance" << SLApplication::deviceParameter["DeviceLensMinimumFocusDistance"];
    fs << "DeviceSensorPhysicalSizeW" << SLApplication::deviceParameter["DeviceSensorPhysicalSizeW"];
    fs << "DeviceSensorPhysicalSizeH" << SLApplication::deviceParameter["DeviceSensorPhysicalSizeH"];
    fs << "DeviceSensorActiveArraySizeW" << SLApplication::deviceParameter["DeviceSensorActiveArraySizeW"];
    fs << "DeviceSensorActiveArraySizeH" << SLApplication::deviceParameter["DeviceSensorActiveArraySizeH"];

    // close file
    fs.release();
    SL_LOG("Calib. saved    : %s\n", fullPathAndFilename.c_str());
    uploadCalibration(fullPathAndFilename);
}
//-----------------------------------------------------------------------------
//! Loads the chessboard calibration pattern parameters
bool SLCVCalibration::loadCalibParams()
{
    FileStorage fs;
    fs.open(calibIniPath + _calibParamsFileName, FileStorage::READ);
    if (!fs.isOpened())
    {
        cout << "Could not open the calibration parameter file: "
             << (calibIniPath + _calibParamsFileName) << endl;
        _state = CS_uncalibrated;
        return false;
    }

    //assign paramters
    fs["numInnerCornersWidth"] >> _boardSize.width;
    fs["numInnerCornersHeight"] >> _boardSize.height;
    fs["squareSizeMM"] >> _boardSquareMM;
    fs["numOfImgsToCapture"] >> _numOfImgsToCapture;

    return true;
}
//-----------------------------------------------------------------------------
//! Calculates the vertical field of view angle in degrees
void SLCVCalibration::calcCameraFov()
{
    if (_cameraMat.rows != 3 || _cameraMat.cols != 3)
        SL_EXIT_MSG("SLCVCalibration::calcCameraFovV: No intrinsic parameter available");

    //calculate vertical field of view
    SLfloat fx     = (SLfloat)_cameraMat.at<double>(0, 0);
    SLfloat fy     = (SLfloat)_cameraMat.at<double>(1, 1);
    SLfloat cx     = (SLfloat)_cameraMat.at<double>(0, 2);
    SLfloat cy     = (SLfloat)_cameraMat.at<double>(1, 2);
    _cameraFovHDeg = 2 * (SLfloat)atan2(cx, fx) * SL_RAD2DEG;
    _cameraFovVDeg = 2 * (SLfloat)atan2(cy, fy) * SL_RAD2DEG;
}
//-----------------------------------------------------------------------------
//! Calculates the 3D positions of the chessboard corners
void SLCVCalibration::calcBoardCorners3D(const SLCVSize& boardSize,
                                         SLfloat         squareSize,
                                         SLCVVPoint3f&   objectPoints3D)
{
    // Because OpenCV image coords are top-left we define the according
    // 3D coords also top-left.
    objectPoints3D.clear();
    for (SLint y = boardSize.height - 1; y >= 0; --y)
        for (SLint x = 0; x < boardSize.width; ++x)
            objectPoints3D.push_back(SLCVPoint3f(x * squareSize,
                                                 y * squareSize,
                                                 0));
}
//-----------------------------------------------------------------------------
//! Calculates the reprojection error of the calibration
SLfloat SLCVCalibration::calcReprojectionErr(const SLCVVVPoint3f& objectPoints,
                                             const SLCVVMat&      rvecs,
                                             const SLCVVMat&      tvecs,
                                             SLVfloat&            perViewErrors)
{
    SLCVVPoint2f imagePoints2;
    size_t       totalPoints = 0;
    double       totalErr    = 0, err;
    perViewErrors.resize(objectPoints.size());

    for (size_t i = 0; i < objectPoints.size(); ++i)
    {
        cv::projectPoints(objectPoints[i],
                          rvecs[i],
                          tvecs[i],
                          _cameraMat,
                          _distortion,
                          imagePoints2);

        err = norm(_imagePoints[i], imagePoints2, NORM_L2);

        size_t n         = objectPoints[i].size();
        perViewErrors[i] = (SLfloat)std::sqrt(err * err / n);
        totalErr += err * err;
        totalPoints += n;
    }

    return (SLfloat)std::sqrt(totalErr / totalPoints);
}
//-----------------------------------------------------------------------------
//!< Finds the inner chessboard corners in the given image
bool SLCVCalibration::findChessboard(SLCVMat imageColor,
                                     SLCVMat imageGray,
                                     bool    drawCorners)
{
    assert(!imageGray.empty() &&
           "SLCVCalibration::findChessboard: imageGray is empty!");
    assert(!imageColor.empty() &&
           "SLCVCalibration::findChessboard: imageColor is empty!");
    assert(_boardSize.width && _boardSize.height &&
           "SLCVCalibration::findChessboard: _boardSize is not set!");

    //debug save image
    //stringstream ss;
    //ss << "imageIn_" << _numCaptured << ".png";
    //cv::imwrite(ss.str(), imageColor);

    _imageSize = imageColor.size();

    SLCVVPoint2f corners2D;

    bool found = cv::findChessboardCorners(imageGray,
                                           _boardSize,
                                           corners2D,
                                           cv::CALIB_CB_FAST_CHECK);

    if (found)
    {
        if (_state == CS_calibrateGrab)
        {
            SLCVVPoint2f preciseCorners2D;
            SLint        flags = CALIB_CB_ADAPTIVE_THRESH |
                          CALIB_CB_NORMALIZE_IMAGE |
                          CALIB_CB_FAST_CHECK;
            bool foundPrecisely = cv::findChessboardCorners(imageGray,
                                                            _boardSize,
                                                            preciseCorners2D,
                                                            flags);

            if (foundPrecisely)
            {
                cv::cornerSubPix(imageGray,
                                 corners2D,
                                 SLCVSize(11, 11),
                                 SLCVSize(-1, -1),
                                 TermCriteria(TermCriteria::EPS + TermCriteria::COUNT,
                                              30,
                                              0.1));

                //add detected points
                _imagePoints.push_back(corners2D);
                _numCaptured++;

                //simulate a snapshot
                cv::bitwise_not(imageColor, imageColor);

                _state = CS_calibrateStream;

                //overwrite corners2D for visualization with precise corners
                corners2D.swap(preciseCorners2D);
            }
        }

        if (drawCorners)
        {
            cv::drawChessboardCorners(imageColor,
                                      _boardSize,
                                      SLCVMat(corners2D),
                                      found);
        }
    }
    return found;
}
//-----------------------------------------------------------------------------
//! Calculates the reprojection error of the calibration
static double calcReprojectionErrors(const SLCVVVPoint3f& objectPoints,
                                     const SLCVVVPoint2f& imagePoints,
                                     const SLCVVMat&      rvecs,
                                     const SLCVVMat&      tvecs,
                                     const SLCVMat&       cameraMatrix,
                                     const SLCVMat&       distCoeffs,
                                     SLVfloat&            perViewErrors)
{
    SLCVVPoint2f imagePoints2;
    size_t       totalPoints = 0;
    double       totalErr    = 0, err;
    perViewErrors.resize(objectPoints.size());

    for (size_t i = 0; i < objectPoints.size(); ++i)
    {
        cv::projectPoints(objectPoints[i],
                          rvecs[i],
                          tvecs[i],
                          cameraMatrix,
                          distCoeffs,
                          imagePoints2);

        err = norm(imagePoints[i], imagePoints2, NORM_L2);

        size_t n         = objectPoints[i].size();
        perViewErrors[i] = (SLfloat)std::sqrt(err * err / n);
        totalErr += err * err;
        totalPoints += n;
    }

    return std::sqrt(totalErr / totalPoints);
}
//-----------------------------------------------------------------------------
//! Calculates the calibration with the given set of image points
static bool calcCalibration(SLCVSize&     imageSize,
                            SLCVMat&      cameraMatrix,
                            SLCVMat&      distCoeffs,
                            SLCVVVPoint2f imagePoints,
                            SLCVVMat&     rvecs,
                            SLCVVMat&     tvecs,
                            SLVfloat&     reprojErrs,
                            SLfloat&      totalAvgErr,
                            SLCVSize&     boardSize,
                            SLfloat       squareSize,
                            SLint         flag)
{
    // Init camera matrix with the eye setter
    cameraMatrix = SLCVMat::eye(3, 3, CV_64F);

    // We need to set eleme at 0,0 to 1 if we want a fix aspect ratio
    if (flag & CALIB_FIX_ASPECT_RATIO)
        cameraMatrix.at<double>(0, 0) = 1.0;

    // init the distortion coeffitients to zero
    distCoeffs = SLCVMat::zeros(8, 1, CV_64F);

    SLCVVVPoint3f objectPoints(1);

    SLCVCalibration::calcBoardCorners3D(boardSize,
                                        squareSize,
                                        objectPoints[0]);

    objectPoints.resize(imagePoints.size(), objectPoints[0]);

    ////////////////////////////////////////////////
    //Find intrinsic and extrinsic camera parameters
    double rms = cv::calibrateCamera(objectPoints,
                                     imagePoints,
                                     imageSize,
                                     cameraMatrix,
                                     distCoeffs,
                                     rvecs,
                                     tvecs,
                                     flag);
    ////////////////////////////////////////////////

    cout << "Re-projection error reported by calibrateCamera: " << rms << endl;

    bool ok = cv::checkRange(cameraMatrix) && cv::checkRange(distCoeffs);

    totalAvgErr = (SLfloat)calcReprojectionErrors(objectPoints,
                                                  imagePoints,
                                                  rvecs,
                                                  tvecs,
                                                  cameraMatrix,
                                                  distCoeffs,
                                                  reprojErrs);
    return ok;
}
//-----------------------------------------------------------------------------
//! Initiates the final calculation
bool SLCVCalibration::calculate()
{
    _state = CS_startCalculating;

    SLCVVMat rvecs, tvecs;
    SLVfloat reprojErrs;

    _calibFlags = 0;
    if (_calibFixPrincipalPoint) _calibFlags |= CALIB_FIX_PRINCIPAL_POINT;
    if (_calibZeroTangentDist) _calibFlags |= CALIB_ZERO_TANGENT_DIST;
    if (_calibFixAspectRatio) _calibFlags |= CALIB_FIX_ASPECT_RATIO;

    bool ok = calcCalibration(_imageSize,
                              _cameraMat,
                              _distortion,
                              _imagePoints,
                              rvecs,
                              tvecs,
                              reprojErrs,
                              _reprojectionError,
                              _boardSize,
                              _boardSquareMM,
                              _calibFlags);

    //cout << "ok: " << ok << endl;
    //cout << "_cameraMat: " << _cameraMat << endl;
    //cout << "_distortion: " << _distortion << endl;

    if (!rvecs.empty() || !reprojErrs.empty())
        _numCaptured = (int)std::max(rvecs.size(), reprojErrs.size());
    else
        _numCaptured = 0;

    if (ok)
    {
        buildUndistortionMaps();
        calcCameraFov();
        _calibrationTime = Utils::getDateTime2String();
        _state           = CS_calibrated;
        save();

        cout << "Calibration succeeded. Reprojection error = " << _reprojectionError << endl;
        cout << "cameraMat:" << _cameraMat << endl;
        cout << "distortion:" << _distortion << endl;
    }
    else
    {
        _cameraFovVDeg   = 0.0f;
        _cameraFovHDeg   = 0.0f;
        _calibrationTime = "-";
        _undistortMapX.release();
        _undistortMapY.release();
        _state = CS_uncalibrated;
        cout << "Calibration failed." << endl;
    }
    return ok;
}
//-----------------------------------------------------------------------------
//! Builds undistortion maps after calibration or loading
void SLCVCalibration::buildUndistortionMaps()
{
    // An alpha of 0 leads to no black borders
    // An alpha of 1 leads to black borders
    double alpha = 0.5;

    // Create optimal camera matrix for undistorted image
    _cameraMatUndistorted = cv::getOptimalNewCameraMatrix(_cameraMat,
                                                          _distortion,
                                                          _imageSize,
                                                          alpha,
                                                          _imageSize,
                                                          nullptr,
                                                          true);
    // Create undistortion maps
    _undistortMapX.release();
    _undistortMapY.release();

    cv::initUndistortRectifyMap(_cameraMat,
                                _distortion,
                                cv::Mat(), // Identity matrix R
                                _cameraMatUndistorted,
                                _imageSize,
                                CV_32FC1,
                                _undistortMapX,
                                _undistortMapY);

    if (_undistortMapX.empty() || _undistortMapY.empty())
        SL_EXIT_MSG("SLCVCalibration::buildUndistortionMaps failed.");
}
//-----------------------------------------------------------------------------
//! Undistorts the inDistorted image into the outUndistorted
void SLCVCalibration::remap(SLCVMat& inDistorted,
                            SLCVMat& outUndistorted)
{
    assert(!inDistorted.empty() &&
           "Input image is empty!");

    assert(!_undistortMapX.empty() &&
           !_undistortMapY.empty() &&
           "Undistortion Maps are empty!");

    cv::remap(inDistorted,
              outUndistorted,
              _undistortMapX,
              _undistortMapY,
              cv::INTER_LINEAR);
}
//-----------------------------------------------------------------------------
//! Calculates camera intrinsics from a guessed FOV angle
/*! Most laptop-, webcam- or mobile camera have a horizontal view angle or
so called field of view (FOV) of around 65 degrees. From this parameter we
can calculate the most important intrinsic parameter the focal length. All
other parameters are set as if the lens would be perfect: No lens distortion
and the view axis goes through the center of the image.
If the focal length and sensor size is provided by the device we deduce the
the fov from it.
*/
void SLCVCalibration::createFromGuessedFOV(SLint imageWidthPX,
                                           SLint imageHeightPX)
{
    // aspect ratio
    SLfloat withOverHeight = (float)imageWidthPX / (float)imageHeightPX;

    // Try to read device lens and sensor information
    SLstring strF = SLApplication::deviceParameter["DeviceLensFocalLength"];
    SLstring strW = SLApplication::deviceParameter["DeviceSensorPhysicalSizeW"];
    SLstring strH = SLApplication::deviceParameter["DeviceSensorPhysicalSizeH"];
    float    devF = strF.empty() ? 0.0f : stof(strF);
    float    devW = strW.empty() ? 0.0f : stof(strW);
    float    devH = strH.empty() ? 0.0f : stof(strH);

    // average horizontal view angle in degrees
    SLfloat fovH = 65.0f;

    // the vertical fov is derived from the width because it could be cropped
    SLfloat fovV = fovH / withOverHeight;

    // overwrite if device lens and sensor information exist and are reasonable
    if (devF > 0.0f && devW > 0.0f && devH > 0.0f)
    {
        SLfloat devFovH = 2.0f * atan(devW / (2.0f * devF)) * SL_RAD2DEG;
        SLfloat devFovV = devFovH / withOverHeight;
        if (devFovH > 60.0f && devFovH < 70.0f)
        {
            fovH = devFovH;
            fovV = devFovV;
        }
    }

    // Create standard camera matrix
    // fx, fx, cx, cy are all in pixel values not mm
    // We asume that we have an ideal image sensor with square pixels
    // so that the focal length fx and fy are identical
    // See the OpenCV documentation for more details:
    // http://docs.opencv.org/3.1.0/dc/dbb/tutorial_py_calibration.html

    SLfloat cx = (float)imageWidthPX * 0.5f;
    SLfloat cy = (float)imageHeightPX * 0.5f;
    SLfloat fx = cx / tanf(fovH * 0.5f * SL_DEG2RAD);
    SLfloat fy = fx;

    _imageSize.width  = imageWidthPX;
    _imageSize.height = imageHeightPX;
    _cameraMat        = (Mat_<double>(3, 3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
    _distortion       = (Mat_<double>(5, 1) << 0, 0, 0, 0, 0); // No distortion
    _cameraFovHDeg    = fovH;
    _cameraFovVDeg    = fovV;
    _calibrationTime  = Utils::getDateTime2String();
    _state            = CS_guessed;
}
//-----------------------------------------------------------------------------
//! Adapts an allready calibrated camera to a new resolution
void SLCVCalibration::adaptForNewResolution(const SLCVSize& newSize)
{
    // allow adaptation only for calibrated cameras
    if (_state != CS_calibrated) return;

    // new center and focal length in pixels not mm
    SLfloat cx = (float)newSize.width * 0.5f;
    SLfloat cy = (float)newSize.height * 0.5f;
    SLfloat fx = cx / tanf(_cameraFovHDeg * 0.5f * SL_DEG2RAD);
    SLfloat fy = fx;

    _imageSize.width  = newSize.width;
    _imageSize.height = newSize.height;
    _cameraMat        = (Mat_<double>(3, 3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
    //_distortion remains unchanged
    _calibrationTime = Utils::getDateTime2String();

    save();
}
//-----------------------------------------------------------------------------
//! Uploads the active calibration to the ftp server
void SLCVCalibration::uploadCalibration(const SLstring& fullPathAndFilename)
{
    if (!Utils::fileExists(fullPathAndFilename))
    {
        SL_LOG("Calib. file doesn't exist: %s\n", fullPathAndFilename.c_str());
        return;
    }

    if (state() != CS_calibrated)
    {
        SL_LOG("Camera is not calibrated.");
        return;
    }

    ftplib ftp;

    if (ftp.Connect(FTP_HOST.c_str()))
    {
        if (ftp.Login(FTP_USER.c_str(), FTP_PWD.c_str()))
        {
            if (ftp.Chdir(FTP_DIR.c_str()))
            {
                // Get the latest calibration filename on the ftp
                string latestCalibFile = getLatestCalibFilename(ftp, fullPathAndFilename);

                // Set the calibfile version
                int versionNO = 0;
                if (!latestCalibFile.empty())
                {
                    versionNO = getVersionInCalibFilename(latestCalibFile);
                }

                // Increase the version
                versionNO++;
                stringstream versionSS;
                versionSS << "(" << versionNO << ")";
                versionSS.str();

                // Build new filename on ftp with version number
                string fileWOExt          = Utils::getFileNameWOExt(fullPathAndFilename);
                string newVersionFilename = fileWOExt + versionSS.str() + ".xml";

                // Upload
                if (!ftp.Put(fullPathAndFilename.c_str(),
                             newVersionFilename.c_str(),
                             ftplib::transfermode::image))
                    SL_LOG("*** ERROR: ftp.Put failed. ***\n");
            }
            else
                SL_LOG("*** ERROR: ftp.Chdir failed. ***\n");
        }
        else
            SL_LOG("*** ERROR: ftp.Login failed. ***\n");
    }
    else
        SL_LOG("*** ERROR: ftp.Connect failed. ***\n");

    ftp.Quit();
}
//-----------------------------------------------------------------------------
//! Uploads the active calibration to the ftp server
void SLCVCalibration::downloadCalibration(const SLstring& fullPathAndFilename)
{
    ftplib ftp;

    if (ftp.Connect(FTP_HOST.c_str()))
    {
        if (ftp.Login(FTP_USER.c_str(), FTP_PWD.c_str()))
        {
            if (ftp.Chdir(FTP_DIR.c_str()))
            {
                // Get the latest calibration filename on the ftp
                string latestCalibFile = getLatestCalibFilename(ftp, fullPathAndFilename);
                int    remoteSize      = 0;
                ftp.Size(latestCalibFile.c_str(),
                         &remoteSize,
                         ftplib::transfermode::image);

                if (remoteSize > 0)
                {
                    string targetFilename = Utils::getFileName(fullPathAndFilename);
                    if (!ftp.Get(fullPathAndFilename.c_str(),
                                 latestCalibFile.c_str(),
                                 ftplib::transfermode::image))
                        SL_LOG("*** ERROR: ftp.Get failed. ***\n");
                }
                else
                    SL_LOG("*** No calibration to download ***\n");
            }
            else
                SL_LOG("*** ERROR: ftp.Chdir failed. ***\n");
        }
        else
            SL_LOG("*** ERROR: ftp.Login failed. ***\n");
    }
    else
        SL_LOG("*** ERROR: ftp.Connect failed. ***\n");

    ftp.Quit();
}
//-----------------------------------------------------------------------------
//! Returns the latest calibration filename of the same fullPathAndFilename
SLstring SLCVCalibration::getLatestCalibFilename(ftplib&         ftp,
                                                 const SLstring& fullPathAndFilename)
{
    // Get a list of calibrations of the same device
    string dirResult         = SLApplication::configPath + "dirResult.txt";
    string filenameWOExt     = Utils::getFileNameWOExt(fullPathAndFilename);
    string filenameWOExtStar = filenameWOExt + "*";

    // Get result of ftp.Dir into the textfile dirResult
    if (ftp.Dir(dirResult.c_str(), filenameWOExtStar.c_str()))
    {
        SLVstring vecFilesInDir;
        SLVstring strippedFiles;

        if (Utils::getFileContent(dirResult, vecFilesInDir))
        {
            for (SLstring& fileInfoLine : vecFilesInDir)
            {
                size_t foundAt = fileInfoLine.find(filenameWOExt);
                if (foundAt != string::npos)
                {
                    string fileWExt  = fileInfoLine.substr(foundAt);
                    string fileWOExt = Utils::getFileNameWOExt(fileWExt);
                    strippedFiles.push_back(fileWOExt);
                }
            }
        }

        if (!strippedFiles.empty())
        {
            // sort filename naturally as many file systems do.
            std::sort(strippedFiles.begin(), strippedFiles.end(), Utils::compareNatural);
            string latest = strippedFiles.back() + ".xml";
            return latest;
        }
        else
            return "";
    }

    // Return empty for not found
    return "";
}
//-----------------------------------------------------------------------------
//! Returns the version number at the end of the calibration filename
int SLCVCalibration::getVersionInCalibFilename(const string& calibFilename)
{
    string calibFilenameWOExt = Utils::getFileNameWOExt(calibFilename);

    int versionNO = 0;
    if (!calibFilenameWOExt.empty())
    {
        SLulong len = calibFilenameWOExt.length();
        if (calibFilenameWOExt.at(len - 1) == ')')
        {
            size_t leftPos = calibFilenameWOExt.rfind("(");
            string verStr  = calibFilenameWOExt.substr(leftPos + 1, len - leftPos - 2);
            versionNO      = stoi(verStr);
        }
    }
    return versionNO;
}
//-----------------------------------------------------------------------------
