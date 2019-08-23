//#############################################################################
//  File:      CVTracked.h
//  Author:    Michael Goettlicher, Marcus Hudritsch
//  Date:      Winter 2016
//  Codestyle: https://github.com/cpvrlab/SLProject/wiki/SLProject-Coding-Style
//  Copyright: Marcus Hudritsch, Michael Goettlicher
//             This software is provide under the GNU General Public License
//             Please visit: http://opensource.org/licenses/GPL-3.0
//#############################################################################

#ifndef CVTRACKER_H
#define CVTRACKER_H

/*
The OpenCV library version 3.4 or above with extra module must be present.
If the application captures the live video stream with OpenCV you have
to define in addition the constant SL_USES_CVCAPTURE.
All classes that use OpenCV begin with CV.
See also the class docs for CVCapture, CVCalibration and CVTracked
for a good top down information.
*/

#include <CVTypedefs.h>
#include <CVCalibration.h>
#include <opencv2/aruco.hpp>
#include <opencv2/xfeatures2d.hpp>

//-----------------------------------------------------------------------------
//! SLCVTracked is the pure virtual base class for tracking features in video.
/*! The static vector trackers can hold multiple of CVTracked that are
 tracked in scenes that require a live video image from the device camera.
 A tracker is bound to a scene node. If the node is the camera node the tracker
 calculates the relative position of the camera to the tracker. This is the
 standard augmented reality case. If the camera is a normal scene node, the
 tracker calculates the object matrix relative to the scene camera.
 See also the derived classes SLCVTrackedAruco, SLCVTrackedChessboard,
 SLCVTrackedFaces and SLCVTrackedFeature for example implementations.
 The update of the tracking per frame is implemented in onUpdateTracking in
 AppDemoTracking.cpp and called once per frame within the main render loop.
*/
//-----------------------------------------------------------------------------
class CVTracked
{
    public:
    explicit CVTracked() : _isVisible(false) {}

    virtual bool track(CVMat          imageGray,
                       CVMat          imageRgb,
                       CVCalibration* calib) = 0;

    SLMat4f createGLMatrix(const CVMat& tVec,
                           const CVMat& rVec);
    void    createRvecTvec(const SLMat4f& glMat,
                           CVMat&         tVec,
                           CVMat&         rVec);
    SLMat4f calcObjectMatrix(const SLMat4f& cameraObjectMat,
                             const SLMat4f& objectViewMat);

    // Setters
    void drawDetection(bool draw) { _drawDetection = draw; }

    // Getters
    bool    isVisible() { return _isVisible; }
    bool    drawDetection() { return _drawDetection; }
    SLMat4f objectViewMat() { return _objectViewMat; }

    // Statics: These statics are used directly in application code (e.g. in )
    static void     resetTimes();    //!< Resets all static variables
    static AvgFloat trackingTimesMS; //!< Averaged time for video tracking in ms
    static AvgFloat detectTimesMS;   //!< Averaged time for video feature detection & description in ms
    static AvgFloat detect1TimesMS;  //!< Averaged time for video feature detection subpart 1 in ms
    static AvgFloat detect2TimesMS;  //!< Averaged time for video feature detection subpart 2 in ms
    static AvgFloat matchTimesMS;    //!< Averaged time for video feature matching in ms
    static AvgFloat optFlowTimesMS;  //!< Averaged time for video feature optical flow tracking in ms
    static AvgFloat poseTimesMS;     //!< Averaged time for video feature pose estimation in ms

    protected:
    bool         _isVisible;     //!< Flag if marker is visible
    bool         _drawDetection; //! Flag if detection should be drawn into image
    SLMat4f      _objectViewMat; //!< view transformation matrix
    HighResTimer _timer;         //!< High resolution timer
};
//-----------------------------------------------------------------------------
typedef std::vector<CVTracked*> SLVCVTracked; //!< Vector of CV tracker pointer
//-----------------------------------------------------------------------------
#endif