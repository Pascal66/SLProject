//#############################################################################
//  File:      SLSpheric.cpp
//  Author:    Marcus Hudritsch
//  Date:      July 2014
//  Codestyle: https://github.com/cpvrlab/SLProject/wiki/Coding-Style-Guidelines
//  Copyright: Marcus Hudritsch
//             This software is provide under the GNU General Public License
//             Please visit: http://opensource.org/licenses/GPL-3.0
//#############################################################################

#include <stdafx.h> // Must be the 1st include followed by  an empty line

#ifdef SL_MEMLEAKDETECT    // set in SL.h for debug config only
#    include <debug_new.h> // memory leak detector
#endif

#include <SLSpheric.h>

//-----------------------------------------------------------------------------
/*!
SLSpheric::SLSpheric ctor for spheric revolution object around the z-axis.
*/
SLSpheric::SLSpheric(SLfloat     sphereRadius,
                     SLfloat     thetaStartDEG,
                     SLfloat     thetaEndDEG,
                     SLuint      stacks,
                     SLuint      slices,
                     SLstring    name,
                     SLMaterial* mat) : SLRevolver(name)
{
    assert(slices >= 3 && "Error: Not enough slices.");
    assert(slices > 0 && "Error: Not enough stacks.");
    assert(thetaStartDEG >= 0.0f && thetaStartDEG < 180.0f &&
           "Error: Polar start angle < 0 or > 180");
    assert(thetaEndDEG > 0.0f && thetaEndDEG <= 180.0f &&
           "Error: Polar end angle < 0 or > 180");
    assert(thetaStartDEG < thetaEndDEG &&
           "Error: Polar start angle > end angle");

    _radius        = sphereRadius;
    _stacks        = stacks;
    _thetaStartDEG = thetaStartDEG;
    _thetaEndDEG   = thetaEndDEG;

    _slices      = slices;
    _smoothFirst = (thetaStartDEG == 0.0f);
    _smoothLast  = (thetaEndDEG == 180.0f);
    _isVolume    = (thetaStartDEG == 0.0f && thetaEndDEG == 180.0f);
    _revAxis.set(0, 0, 1);
    _revPoints.reserve(stacks + 1);

    SLfloat theta  = -SL_PI + thetaStartDEG * SL_DEG2RAD;
    SLfloat phi    = 0;
    SLfloat dTheta = (thetaEndDEG - thetaStartDEG) * SL_DEG2RAD / stacks;

    for (SLuint i = 0; i <= stacks; ++i)
    {
        SLVec3f p;
        p.fromSpherical(sphereRadius, theta, phi);
        _revPoints.push_back(p);
        theta += dTheta;
    }

    buildMesh(mat);
}
//-----------------------------------------------------------------------------
