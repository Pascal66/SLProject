//#############################################################################
//  File:      SLSceneViewHandler.cpp
//  Author:    Luc Girod
//  Date:      Aout 2019
//  Codestyle: https://github.com/cpvrlab/SLProject/wiki/Coding-Style-Guidelines
//  Copyright: Marcus Hudritsch
//             This software is provide under the GNU General Public License
//             Please visit: http://opensource.org/licenses/GPL-3.0
//#############################################################################

#include <stdafx.h> // Must be the 1st include followed by  an empty line

#include <SLSceneViewHandler.h>

SLSceneViewHandler::SLSceneViewHandler() { }

SLSceneViewHandler::~SLSceneViewHandler()
{
    for (SLSceneView * sv : svs)
    {
        if (sv == nullptr)
            continue;

        delete(sv);
    }
}

void SLSceneViewHandler::addSceneView(SLSceneView * sv)
{
    svs.push_back(sv);
    toUpdate.resize(svs.size());
}

bool SLSceneViewHandler::updateAndPaint(SLInputManager &inputManager)
{
    bool ev = inputManager.pollAndProcessEvents(svs);

    bool needUpdate = false;

    for (auto sv : svs)
    {
        if (sv == nullptr)
            continue;

        bool viewNeedsRepaint;

        needUpdate |= sv->scene()->onUpdate();
        needUpdate |= sv->onPaint();
    }
    return needUpdate;
}
