//#############################################################################
//  File:      SLSceneView.h
//  Author:    Marc Wacker, Marcus Hudritsch
//  Date:      July 2014
//  Codestyle: https://github.com/cpvrlab/SLProject/wiki/Coding-Style-Guidelines
//  Copyright: Marcus Hudritsch
//             This software is provide under the GNU General Public License
//             Please visit: http://opensource.org/licenses/GPL-3.0
//#############################################################################

#ifndef SLSCENEVIEWHANDLER_H
#define SLSCENEVIEWHANDLER_H

#include <vector>
#include <SLSceneView.h>
#include <SLInputManager.h>

class SLSceneViewHandler
{
    public:
    SLSceneViewHandler();
    ~SLSceneViewHandler();

    void addSceneView(SLSceneView * sv);
    bool updateAndPaint(SLInputManager &inputManager);

    private:
    std::vector<SLSceneView*> svs;
    std::vector<SLSceneView*> toUpdate;
};
//-----------------------------------------------------------------------------
#endif
