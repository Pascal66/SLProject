//#############################################################################
//  File:      SLInputDevice.cpp
//  Author:    Marc Wacker
//  Date:      January 2015
//  Codestyle: https://github.com/cpvrlab/SLProject/wiki/Coding-Style-Guidelines
//  Copyright: Marcus Hudritsch
//             This software is provide under the GNU General Public License
//             Please visit: http://opensource.org/licenses/GPL-3.0
//#############################################################################

#include <stdafx.h> // Must be the 1st include followed by  an empty line

#ifdef SL_MEMLEAKDETECT    // set in SL.h for debug config only
#    include <debug_new.h> // memory leak detector
#endif
#include <SLApplication.h>
#include <SLInputManager.h>

//-----------------------------------------------------------------------------
/*! Constructor for SLInputDevices. This will automatically enable the device,
adding them to the SLInputManager.
 */
SLInputDevice::SLInputDevice()
{
    // enable any input device on creation
    enable();
}
//-----------------------------------------------------------------------------
/*! The destructor removes the device from SLInputManager again if necessary.
*/
SLInputDevice::~SLInputDevice()
{
    disable();
}
//-----------------------------------------------------------------------------
/*! Enabling an SLInputDevice will add it to the device list kept by
SLInputManager
*/
void SLInputDevice::enable()
{
    SLApplication::inputManager.devices().push_back(this);
}
//-----------------------------------------------------------------------------
/*! Enabling an SLInputDevice will remove it from the device list kept by
SLInputManager
*/
void SLInputDevice::disable()
{
    SLVInputDevice& dl = SLApplication::inputManager.devices();
    dl.erase(remove(dl.begin(), dl.end(), this), dl.end());
}
//-----------------------------------------------------------------------------
