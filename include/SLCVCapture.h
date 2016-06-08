//#############################################################################
//  File:      SLCVCapture
//  Purpose:   OpenCV Capture Device
//  Author:    Marcus Hudritsch
//  Date:      June 2016
//  Codestyle: https://github.com/cpvrlab/SLProject/wiki/Coding-Style-Guidelines
//  Copyright: Marcus Hudritsch
//             This software is provide under the GNU General Public License
//             Please visit: http://opensource.org/licenses/GPL-3.0
//#############################################################################


#ifndef SLCVCapture_H
#define SLCVCapture_H

#include <stdafx.h>
#include <opencv2/opencv.hpp>

using namespace cv;

//-----------------------------------------------------------------------------
//! Encapsulation of the OpenCV Capture Device
class SLCVCapture
{   public:
    static  SLVec2i         open            (int deviceNum);
    static  void            grabAndCopyToSL ();
    static  void            copyFrameToSL   ();
    static  bool            isOpened        () { return _captureDevice.isOpened();}
    static  void            release         () {_captureDevice.release();}
    
    static  Mat             lastFrame;      //!< last frame grabbed
    static  SLPixelFormat   format;         //!< SL pixel format

    private:
    static  VideoCapture    _captureDevice; //!< OpenCV capture device
};
//-----------------------------------------------------------------------------
#endif
