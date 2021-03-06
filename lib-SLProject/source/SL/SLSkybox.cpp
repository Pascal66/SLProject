//#############################################################################
//  File:      SLSkybox
//  Author:    Marcus Hudritsch
//  Date:      December 2017
//  Codestyle: https://github.com/cpvrlab/SLProject/wiki/Coding-Style-Guidelines
//  Copyright: Marcus Hudritsch
//             This software is provide under the GNU General Public License
//             Please visit: http://opensource.org/licenses/GPL-3.0
//#############################################################################

#include <stdafx.h> // Must be the 1st include followed by  an empty line

#ifdef SL_MEMLEAKDETECT    // set in SL.h for debug config only
#    include <debug_new.h> // memory leak detector
#endif

#include <SLBox.h>
#include <SLCamera.h>
#include <SLGLTexture.h>
#include <SLMaterial.h>
#include <SLSceneView.h>
#include <SLSkybox.h>

//-----------------------------------------------------------------------------
//! Default constructor
SLSkybox::SLSkybox(SLstring name) : SLNode(name)
{
}
//-----------------------------------------------------------------------------
//! Cubemap Constructor with cubemap images
/*! All resources allocated are stored in the SLScene vectors for textures,
materials, programs and meshes and get deleted at scene destruction.
*/
SLSkybox::SLSkybox(SLstring cubeMapXPos,
                   SLstring cubeMapXNeg,
                   SLstring cubeMapYPos,
                   SLstring cubeMapYNeg,
                   SLstring cubeMapZPos,
                   SLstring cubeMapZNeg,
                   SLstring name) : SLNode(name)
{
    // Create texture, material and program
    SLGLTexture* cubeMap    = new SLGLTexture(cubeMapXPos,
                                           cubeMapXNeg,
                                           cubeMapYPos,
                                           cubeMapYNeg,
                                           cubeMapZPos,
                                           cubeMapZNeg);
    SLMaterial*  matCubeMap = new SLMaterial("matCubeMap");
    matCubeMap->textures().push_back(cubeMap);
    SLGLProgram* sp = new SLGLGenericProgram("SkyBox.vert", "SkyBox.frag");
    matCubeMap->program(sp);

    // Create a box with max. point at min. parameter and vice versa.
    // Like this the boxes normals will point to the inside.
    this->addMesh(new SLBox(10, 10, 10, -10, -10, -10, "box", matCubeMap));
}
//-----------------------------------------------------------------------------
//! Draw the skybox with a cube map with the camera in its center.
void SLSkybox::drawAroundCamera(SLSceneView* sv)
{
    assert(sv && "No SceneView passed to SLSkybox::drawAroundCamera");

    // Set the view transform
    _stateGL->modelViewMatrix.setMatrix(_stateGL->viewMatrix);

    // Put skybox at the cameras position
    this->translation(sv->camera()->translationWS());

    // Apply world transform
    _stateGL->modelViewMatrix.multiply(this->updateAndGetWM().m());

    // Freeze depth buffer
    _stateGL->depthMask(false);

    // Draw the box
    this->drawMeshes(sv);

    // Unlock depth buffer
    _stateGL->depthMask(true);
}
//-----------------------------------------------------------------------------
//! Returns the color in the skybox at the the specified direction dir
SLCol4f SLSkybox::colorAtDir(SLVec3f dir)
{
    assert(_meshes.size() > 0);
    assert(_meshes[0]->mat()->textures().size() > 0);

    SLGLTexture* tex = _meshes[0]->mat()->textures()[0];

    return tex->getTexelf(dir);
}
//-----------------------------------------------------------------------------
