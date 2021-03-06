//#############################################################################
//  File:      SLSceneView.cpp
//  Author:    Marc Wacker, Marcus Hudritsch
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

#include <SLAnimManager.h>
#include <SLAnimation.h>
#include <SLApplication.h>
#include <SLCVCalibration.h>
#include <SLCVCapture.h>
#include <SLCamera.h>
#include <SLImporter.h>
#include <SLInterface.h>
#include <SLLight.h>
#include <SLLightRect.h>
#include <SLLightSpot.h>
#include <SLSceneView.h>
#include <SLTexFont.h>

//-----------------------------------------------------------------------------
// Milliseconds duration of a long touch event
const SLint SLSceneView::LONGTOUCH_MS = 500;
//-----------------------------------------------------------------------------
//! SLSceneView default constructor
/*! The default constructor adds the this pointer to the sceneView vector in 
SLScene. If an in between element in the vector is zero (from previous sceneviews) 
it will be replaced. The sceneviews _index is the index in the sceneview vector.
It never changes throughout the life of a sceneview. 
*/
SLSceneView::SLSceneView() : SLObject()
{
    SLScene* s = SLApplication::scene;
    assert(s && "No SLApplication::scene instance.");

    // Find first a zero pointer gap in
    for (SLuint i = 0; i < s->sceneViews().size(); ++i)
    {
        if (s->sceneViews()[i] == nullptr)
        {
            s->sceneViews()[i] = this;
            _index             = i;
            return;
        }
    }

    // No gaps, so add it and get the index back.
    s->sceneViews().push_back(this);
    _index = (SLuint)s->sceneViews().size() - 1;
}
//-----------------------------------------------------------------------------
SLSceneView::~SLSceneView()
{
    // Set pointer in SLScene::sceneViews vector to zero but leave it.
    // The remaining sceneviews must keep their index in the vector
    SLApplication::scene->sceneViews()[(SLuint)_index] = nullptr;

    _gui.deleteOpenGLObjects();

    SL_LOG("Destructor      : ~SLSceneView\n");
}
//-----------------------------------------------------------------------------
/*! SLSceneView::init initializes default values for an empty scene
\param name Name of the sceneview
\param screenWidth Width of the OpenGL frame buffer.
\param screenHeight Height of the OpenGL frame buffer.
\param onWndUpdateCallback Callback for ray tracing update
\param onSelectNodeMeshCallback Callback on node and mesh selection
\param onImGuiBuild Callback for the external ImGui build function
*/
void SLSceneView::init(SLstring name,
                       SLint    screenWidth,
                       SLint    screenHeight,
                       void*    onWndUpdateCallback,
                       void*    onSelectNodeMeshCallback,
                       void*    onImGuiBuild)
{
    _name       = name;
    _scrW       = screenWidth;
    _scrH       = screenHeight;
    _gotPainted = true;

    // The window update callback function is used to refresh the ray tracing
    // image during the rendering process. The ray tracing image is drawn by OpenGL
    // as a texture on a single quad.
    onWndUpdate = (cbOnWndUpdate)onWndUpdateCallback;

    // The on select node callback is called when a node got selected on double
    // click, so that the UI can react on it.
    onSelectedNodeMesh = (cbOnSelectNodeMesh)onSelectNodeMeshCallback;

    // Set the ImGui build function. Every sceneview could have it's own GUI.
    _gui.build = (cbOnImGuiBuild)onImGuiBuild;

    _stateGL = nullptr;

    _camera = nullptr;

    // enables and modes
    _mouseDownL = false;
    _mouseDownR = false;
    _mouseDownM = false;
    _touchDowns = 0;

    _doDepthTest      = true;
    _doMultiSampling  = true; // true=OpenGL multisampling is turned on
    _doFrustumCulling = true; // true=enables view frustum culling
    _doWaitOnIdle     = true;
    _drawBits.allOff();

    _stats3D.clear();

    _scrWdiv2 = _scrW >> 1;
    _scrHdiv2 = _scrH >> 1;
    _scrWdivH = (SLfloat)_scrW / (SLfloat)_scrH;

    _renderType = RT_gl;

    _skybox = nullptr;

    _gui.init();

    onStartup();
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::onInitialize is called by the window system before the first 
rendering. It applies all scene rendering attributes with the according 
OpenGL function.
*/
void SLSceneView::initSceneViewCamera(const SLVec3f& dir, SLProjection proj)
{
    _sceneViewCamera.camAnim(CA_turntableYUp);
    _sceneViewCamera.name("SceneView Camera");
    _sceneViewCamera.clipNear(.1f);
    _sceneViewCamera.clipFar(2000.0f);
    _sceneViewCamera.maxSpeed(40);
    _sceneViewCamera.eyeSeparation(_sceneViewCamera.focalDist() / 30.0f);
    _sceneViewCamera.setProjection(this, ET_center);
    _sceneViewCamera.projection(proj);

    // fit scenes bounding box in view frustum
    SLScene* s = SLApplication::scene;
    if (s->root3D())
    {
        // we want to fit the scenes combined aabb in the view frustum
        SLAABBox* sceneBounds = s->root3D()->aabb();

        _sceneViewCamera.translation(sceneBounds->centerWS(), TS_world);
        _sceneViewCamera.lookAt(sceneBounds->centerWS() + dir, SLVec3f::AXISY, TS_parent);

        SLfloat minX = sceneBounds->minWS().x;
        SLfloat minY = sceneBounds->minWS().y;
        SLfloat minZ = sceneBounds->minWS().z;
        SLfloat maxX = sceneBounds->maxWS().x;
        SLfloat maxY = sceneBounds->maxWS().y;
        SLfloat maxZ = sceneBounds->maxWS().z;

        // calculate the min and max points in view space
        SLVec4f vsCorners[8];

        vsCorners[0] = SLVec4f(minX, minY, minZ);
        vsCorners[1] = SLVec4f(maxX, minY, minZ);
        vsCorners[2] = SLVec4f(minX, maxY, minZ);
        vsCorners[3] = SLVec4f(maxX, maxY, minZ);
        vsCorners[4] = SLVec4f(minX, minY, maxZ);
        vsCorners[5] = SLVec4f(maxX, minY, maxZ);
        vsCorners[6] = SLVec4f(minX, maxY, maxZ);
        vsCorners[7] = SLVec4f(maxX, maxY, maxZ);

        SLVec3f vsMin(FLT_MAX, FLT_MAX, FLT_MAX);
        SLVec3f vsMax(FLT_MIN, FLT_MIN, FLT_MIN);

        SLMat4f vm = _sceneViewCamera.updateAndGetWMI();

        for (SLint i = 0; i < 8; ++i)
        {
            vsCorners[i] = vm * vsCorners[i];

            vsMin.x = min(vsMin.x, vsCorners[i].x);
            vsMin.y = min(vsMin.y, vsCorners[i].y);
            vsMin.z = min(vsMin.z, vsCorners[i].z);

            vsMax.x = max(vsMax.x, vsCorners[i].x);
            vsMax.y = max(vsMax.y, vsCorners[i].y);
            vsMax.z = max(vsMax.z, vsCorners[i].z);
        }

        SLfloat dist    = 0.0f;
        SLfloat distX   = 0.0f;
        SLfloat distY   = 0.0f;
        SLfloat halfTan = tan(SL_DEG2RAD * _sceneViewCamera.fov() * 0.5f);

        // @todo There is still a bug when OSX doesn't pass correct GLWidget size
        // correctly set the camera distance...
        SLfloat ar = _sceneViewCamera.aspect();

        // special case for orthographic cameras
        if (proj == P_monoOrthographic)
        {
            // NOTE, the orthographic camera has the ability to zoom by using the following:
            // tan(SL_DEG2RAD*_fov*0.5f) * pos.length();

            distX = vsMax.x / (ar * halfTan);
            distY = vsMax.y / halfTan;
        }
        else
        {
            // for now we treat all other cases as having a single frustum
            distX = (vsMax.x - vsMin.x) * 0.5f / (ar * halfTan);
            distY = (vsMax.y - vsMin.y) * 0.5f / halfTan;

            distX += vsMax.z;
            distY += vsMax.z;
        }

        dist = max(distX, distY);

        // set focal distance
        _sceneViewCamera.focalDist(dist);
        _sceneViewCamera.translate(SLVec3f(0, 0, dist), TS_object);
    }

    _stateGL->modelViewMatrix.identity();
    _sceneViewCamera.updateAABBRec();
    _sceneViewCamera.setInitialState();

    // if no camera exists or in VR mode use the sceneViewCamera
    if (_camera == nullptr)
        _camera = &_sceneViewCamera;

    _camera->needUpdate();
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::switchToSceneViewCamera the general idea for this function is
to switch to the editor camera from a scene camera. It could provide
functionality to stay at the position of the previous camera, or to be reset
to the init position etc..
*/
void SLSceneView::switchToSceneViewCamera()
{
    // if we have an active camera, use its position and orientation
    if (_camera)
    {
        SLMat4f currentWM = _camera->updateAndGetWM();
        SLVec3f position  = currentWM.translation();
        SLVec3f forward(-currentWM.m(8), -currentWM.m(9), -currentWM.m(10));
        _sceneViewCamera.translation(position);
        _sceneViewCamera.lookAt(position + forward);
    }

    _camera = &_sceneViewCamera;
}
//-----------------------------------------------------------------------------
//! Sets the acitve camera to the next in the scene
void SLSceneView::switchToNextCameraInScene()
{
    SLCamera* nextCam = SLApplication::scene->nextCameraInScene(this);

    if (nextCam == nullptr)
        return;

    if (nextCam != _camera)
        _camera = nextCam;
    else
        _camera = &_sceneViewCamera;

    _camera->background().rebuild();
    return;
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::onInitialize is called by the window system before the first 
rendering. It applies all scene rendering attributes with the according 
OpenGL function.
*/
void SLSceneView::onInitialize()
{
    postSceneLoad();

    SLScene* s = SLApplication::scene;
    _stateGL   = SLGLState::getInstance();

    if (_camera)
        _stateGL->onInitialize(_camera->background().colors()[0]);
    else
        _stateGL->onInitialize(SLCol4f::GRAY);

    _blendNodes.clear();
    _visibleNodes.clear();
    _visibleNodes2D.clear();

    _raytracer.clearData();
    _renderType   = RT_gl;
    _isFirstFrame = true;

    // init 3D scene with initial depth 1
    if (s->root3D() && s->root3D()->aabb()->radiusOS() < 0.0001f)
    {
        // Init camera so that its frustum is set
        _camera->setProjection(this, ET_center);

        // build axis aligned bounding box hierarchy after init
        clock_t t = clock();
        s->root3D()->updateAABBRec();

        for (auto mesh : s->meshes())
            mesh->updateAccelStruct();

        SL_LOG("Time for AABBs  : %5.3f sec.\n",
               (SLfloat)(clock() - t) / (SLfloat)CLOCKS_PER_SEC);

        // Collect node statistics
        _stats3D.clear();
        _stats2D.clear();
        s->root3D()->statsRec(_stats3D);

        // Warn if there are no light in scene
        if (s->lights().size() == 0)
            SL_LOG("\n**** No Lights found in scene! ****\n");
    }

    // init 2D scene with initial depth 1
    if (s->root2D() && s->root2D()->aabb()->radiusOS() < 0.0001f)
    {
        // build axis aligned bounding box hierarchy after init
        s->root2D()->updateAABBRec();

        // Collect node statistics
        _stats2D.clear();
        s->root2D()->statsRec(_stats2D);
    }

    initSceneViewCamera();

    _gui.onResize(_scrW, _scrH);
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::onResize is called by the window system before the first 
rendering and whenever the window changes its size.
*/
void SLSceneView::onResize(SLint width, SLint height)
{
    SLScene* s = SLApplication::scene;

    // On OSX and Qt this can be called with invalid values > so exit
    if (width == 0 || height == 0) return;

    if (_scrW != width || _scrH != height)
    {
        _scrW     = width;
        _scrH     = height;
        _scrWdiv2 = _scrW >> 1; // width / 2
        _scrHdiv2 = _scrH >> 1; // height / 2
        _scrWdivH = (SLfloat)_scrW / (SLfloat)_scrH;

        _gui.onResize(width, height);

        // Resize Oculus framebuffer
        if (_camera && _camera->projection() == P_stereoSideBySideD)
        {
            _oculusFB.updateSize((SLint)(s->oculus()->resolutionScale() * (SLfloat)_scrW),
                                 (SLint)(s->oculus()->resolutionScale() * (SLfloat)_scrH));
            s->oculus()->renderResolution(_scrW, _scrH);
        }

        // Stop raytracing & pathtracing on resize
        if (_renderType != RT_gl)
        {
            _renderType = RT_gl;
            _raytracer.doContinuous(false);
        }
    }
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::onPaint is called by window system whenever the window and therefore 
the scene needs to be painted. Depending on the renderer it calls first
SLSceneView::draw3DGL, SLSceneView::draw3DRT or SLSceneView::draw3DPT and
then SLSceneView::draw2DGL for all UI in 2D. The method returns true if either
the 2D or 3D graph was updated or waitEvents is false.
*/
SLbool SLSceneView::onPaint()
{
    SLScene* s          = SLApplication::scene;
    SLbool   camUpdated = false;

    // Init and build GUI for all projections except distorted stereo
    if (_camera && _camera->projection() != P_stereoSideBySideD)
        _gui.onInitNewFrame(s, this);

    // Clear NO. of draw calls afer UI creation
    SLGLVertexArray::totalDrawCalls = 0;

    if (_camera)
    { // Render the 3D scenegraph by raytracing, pathtracing or OpenGL
        switch (_renderType)
        {
            case RT_gl: camUpdated = draw3DGL(s->elapsedTimeMS()); break;
            case RT_rt: camUpdated = draw3DRT(); break;
            case RT_pt: camUpdated = draw3DPT(); break;
        }
    };

    // Render the 2D stuff inclusive the ImGui
    draw2DGL();

    _stateGL->unbindAnythingAndFlush();

    // Finish Oculus framebuffer
    if (_camera && _camera->projection() == P_stereoSideBySideD)
        s->oculus()->renderDistortion(_scrW,
                                      _scrH,
                                      _oculusFB.texID(),
                                      _camera->background().colors()[0]);

    // Set gotPainted only to true if RT is not busy
    _gotPainted = _renderType == RT_gl || raytracer()->state() != rtBusy;

    // Return true if it is the first frame or a repaint is needed
    if (_isFirstFrame)
    {
        _isFirstFrame = false;
        return true;
    }

    return !_doWaitOnIdle || camUpdated;
}
//-----------------------------------------------------------------------------
//! Draws the 3D scene with OpenGL
/*! This is the main routine for updating and drawing the 3D scene for one frame. 
The following steps are processed:
<ol>
<li>
<b>Updates the camera</b>:
If the camera has an animation it gets updated first.
The camera animation is the only animation that is view dependent.
</li>
<li>
<b>Clear Buffers</b>:
The color and depth buffer are cleared in this step. If the projection is
the Oculus stereo projection also the framebuffer target is bound. 
</li>
<li>
<b>Set Projection and View</b>:
Depending on the projection we set the camera projection and the view 
for the center or left eye.
</li>
<li>
<b>Frustum Culling</b>:
The frustum culling traversal fills the vectors SLSceneView::_visibleNodes 
and SLSceneView::_blendNodes with the visible transparent nodes. 
Nodes that are not visible with the current camera are not drawn. 
</li>
<li>
<b>Draw Skybox</b>:
The skybox is draw as first object with frozen depth buffer.
The skybox is allways around the active camera.
</li>
<li>
<b>Draw Opaque and Blended Nodes</b>:
By calling the SLSceneView::draw3D all nodes in the vectors 
SLSceneView::_visibleNodes and SLSceneView::_blendNodes will be drawn.
_blendNodes is a vector with all nodes that contain 1-n meshes with 
alpha material. _visibleNodes is a vector with all visible nodes. 
Even if a node contains alpha meshes it still can contain meshes with 
opaque material. If a stereo projection is set, the scene gets drawn 
a second time for the right eye.
</li>
<li>
<b>Draw Oculus Framebuffer</b>:
If the projection is the Oculus stereo projection the framebuffer image
is drawn.
</li>
</ol>
*/
SLbool SLSceneView::draw3DGL(SLfloat elapsedTimeMS)
{
    SLScene* s = SLApplication::scene;

    preDraw();

    /////////////////////////
    // 1. Do camera Update //
    /////////////////////////

    SLfloat startMS = s->timeMilliSec();

    // Update camera animation separately (smooth transition on key movement)
    SLbool camUpdated = _camera->camUpdate(elapsedTimeMS);

    ///////////////////////////////////////
    // 2. Clear Buffers & set Background //
    ///////////////////////////////////////

    // Render into framebuffer if Oculus stereo projection is used
    if (_camera->projection() == P_stereoSideBySideD)
    {
        s->oculus()->beginFrame();
        _oculusFB.bindFramebuffer((SLint)(s->oculus()->resolutionScale() * (SLfloat)_scrW),
                                  (SLint)(s->oculus()->resolutionScale() * (SLfloat)_scrH));
    }

    // Clear color buffer
    // Apply gamma correction only on uniform background. No shader runs on it!
    SLCol4f backColor = _camera->background().colors()[0];
    backColor.gammaCorrect(_stateGL->oneOverGamma);
    _stateGL->clearColor(backColor);
    _stateGL->clearColorDepthBuffer();

    // Render gradient or textured background from active camera
    if (!_skybox && !_camera->background().isUniform())
        _camera->background().render(_scrW, _scrH);

    // Change state (only when changed)
    _stateGL->multiSample(_doMultiSampling);
    _stateGL->depthTest(_doDepthTest);

    //////////////////////////////
    // 3. Set Projection & View //
    //////////////////////////////

    // Set projection and viewport
    if (_camera->projection() > P_monoOrthographic)
        _camera->setProjection(this, ET_left);
    else
        _camera->setProjection(this, ET_center);

    // Set view center eye or left eye
    if (_camera->projection() > P_monoOrthographic)
        _camera->setView(this, ET_left);
    else
        _camera->setView(this, ET_center);

    ////////////////////////
    // 4. Frustum Culling //
    ////////////////////////

    _camera->setFrustumPlanes();
    _blendNodes.clear();
    _visibleNodes.clear();
    if (s->root3D())
        s->root3D()->cull3DRec(this);

    _cullTimeMS = s->timeMilliSec() - startMS;

    ////////////////////
    // 5. Draw skybox //
    ////////////////////

    if (_skybox)
        _skybox->drawAroundCamera(this);

    ////////////////////////////////////
    // 5. Draw Opaque & Blended Nodes //
    ////////////////////////////////////

    startMS = s->timeMilliSec();

    draw3DGLAll();

    // For stereo draw for right eye
    if (_camera->projection() > P_monoOrthographic)
    {
        _camera->setProjection(this, ET_right);
        _camera->setView(this, ET_right);
        draw3DGLAll();
    }

    // Enable all color channels again
    _stateGL->colorMask(1, 1, 1, 1);

    _draw3DTimeMS = s->timeMilliSec() - startMS;

    postDraw();

    GET_GL_ERROR; // Check if any OGL errors occurred
    return camUpdated;
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::draw3DGLAll renders the opaque nodes before blended nodes and
the blended nodes have to be drawn from back to front.
During the cull traversal all nodes with alpha materials are flagged and 
added the to the vector _alphaNodes. The _visibleNodes vector contains all
nodes because a node with alpha meshes still can have nodes with opaque
material. To avoid double drawing the SLNode::drawMeshes draws in the blended
pass only the alpha meshes and in the opaque pass only the opaque meshes.
*/
void SLSceneView::draw3DGLAll()
{
    // 1) Draw first the opaque shapes and all helper lines (normals and AABBs)
    draw3DGLNodes(_visibleNodes, false, false);
    draw3DGLLines(_visibleNodes);
    draw3DGLLines(_blendNodes);

    // 2) Draw blended nodes sorted back to front
    draw3DGLNodes(_blendNodes, true, true);

    // 3) Draw helper
    draw3DGLLinesOverlay(_visibleNodes);
    draw3DGLLinesOverlay(_blendNodes);

    // 4) Draw visualization lines of animation curves
    SLApplication::scene->animManager().drawVisuals(this);

    // 5) Turn blending off again for correct anaglyph stereo modes
    _stateGL->blend(false);
    _stateGL->depthMask(true);
    _stateGL->depthTest(true);
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::draw3DGLNodes draws the nodes meshes from the passed node vector
directly with their world transform after the view transform.
*/
void SLSceneView::draw3DGLNodes(SLVNode& nodes,
                                SLbool   alphaBlended,
                                SLbool   depthSorted)
{
    if (nodes.size() == 0) return;

    // For blended nodes we activate OpenGL blending and stop depth buffer updates
    _stateGL->blend(alphaBlended);
    _stateGL->depthMask(!alphaBlended);

    // Important and expensive step for blended nodes with alpha meshes
    // Depth sort with lambda function by their view distance
    if (depthSorted)
    {
        std::sort(nodes.begin(), nodes.end(), [](SLNode* a, SLNode* b) {
            if (!a) return false;
            if (!b) return true;
            return a->aabb()->sqrViewDist() > b->aabb()->sqrViewDist();
        });
    }

    // draw the shapes directly with their wm transform
    for (auto node : nodes)
    {
        // Set the view transform
        _stateGL->modelViewMatrix.setMatrix(_stateGL->viewMatrix);

        // Apply world transform
        _stateGL->modelViewMatrix.multiply(node->updateAndGetWM().m());

        // Finally the nodes meshes
        node->drawMeshes(this);
    }

    GET_GL_ERROR; // Check if any OGL errors occurred
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::draw3DGLLines draws the AABB from the passed node vector directly
with their world coordinates after the view transform. The lines must be drawn
without blending.
Colors:
Red   : AABB of nodes with meshes
Pink  : AABB of nodes without meshes (only child nodes)
Yellow: AABB of selected node 
*/
void SLSceneView::draw3DGLLines(SLVNode& nodes)
{
    if (nodes.size() == 0) return;

    _stateGL->blend(false);
    _stateGL->depthMask(true);

    // Set the view transform
    _stateGL->modelViewMatrix.setMatrix(_stateGL->viewMatrix);

    // draw the opaque shapes directly w. their wm transform
    for (auto node : nodes)
    {
        if (node != _camera)
        {
            // Draw first AABB of the shapes but not the camera
            if ((drawBit(SL_DB_BBOX) || node->drawBit(SL_DB_BBOX)) &&
                !node->drawBit(SL_DB_SELECTED))
            {
                if (node->numMeshes() > 0)
                    node->aabb()->drawWS(SLCol3f(1, 0, 0));
                else
                    node->aabb()->drawWS(SLCol3f(1, 0, 1));
            }

            // Draw AABB for selected shapes
            if (node->drawBit(SL_DB_SELECTED))
            {
                SLScene* s = SLApplication::scene;
                if (node == s->selectedNode() || !s->selectedRect().isEmpty())
                    node->aabb()->drawWS(SLCol3f(1, 1, 0));
                else
                {
                    // delete selection bits from previous rectangle selection
                    if (node != s->selectedNode() && s->selectedRect().isEmpty())
                        node->drawBits()->off(SL_DB_SELECTED);
                }
            }
        }
    }

    GET_GL_ERROR; // Check if any OGL errors occurred
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::draw3DGLLinesOverlay draws the nodes axis and skeleton joints
as overlayed
*/
void SLSceneView::draw3DGLLinesOverlay(SLVNode& nodes)
{

    // draw the opaque shapes directly w. their wm transform
    for (auto node : nodes)
    {
        if (node != _camera)
        {
            if (drawBit(SL_DB_AXIS) || node->drawBit(SL_DB_AXIS) ||
                drawBit(SL_DB_SKELETON) || node->drawBit(SL_DB_SKELETON) ||
                node->drawBit(SL_DB_SELECTED))
            {
                // Set the view transform
                _stateGL->modelViewMatrix.setMatrix(_stateGL->viewMatrix);
                _stateGL->blend(false);     // Turn off blending for overlay
                _stateGL->depthMask(true);  // Freeze depth buffer for blending
                _stateGL->depthTest(false); // Turn of depth test for overlay

                // Draw axis
                if (drawBit(SL_DB_AXIS) ||
                    node->drawBit(SL_DB_AXIS) ||
                    node->drawBit(SL_DB_SELECTED))
                    node->aabb()->drawAxisWS();

                // Draw skeleton
                if (drawBit(SL_DB_SKELETON) ||
                    node->drawBit(SL_DB_SKELETON))
                {
                    // Draw axis of the skeleton joints and its parent bones
                    const SLSkeleton* skeleton = node->skeleton();
                    if (skeleton)
                    {
                        for (auto joint : skeleton->joints())
                        {
                            // Get the node wm & apply the joints wm
                            SLMat4f wm = node->updateAndGetWM();
                            wm *= joint->updateAndGetWM();

                            // Get parent node wm & apply the parent joint wm
                            SLMat4f parentWM;
                            if (joint->parent())
                            {
                                parentWM = node->parent()->updateAndGetWM();
                                parentWM *= joint->parent()->updateAndGetWM();
                                joint->aabb()->updateBoneWS(parentWM, false, wm);
                            }
                            else
                                joint->aabb()->updateBoneWS(parentWM, true, wm);

                            joint->aabb()->drawBoneWS();
                        }
                    }
                }
            }
        }
    }

    GET_GL_ERROR; // Check if any OGL errors occurred
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::draw2DGL draws all 2D stuff in ortho projection. So far no
update is done to the 2D scenegraph.
*/
void SLSceneView::draw2DGL()
{
    SLScene* s       = SLApplication::scene;
    SLfloat  startMS = s->timeMilliSec();

    SLfloat w2 = (SLfloat)_scrWdiv2;
    SLfloat h2 = (SLfloat)_scrHdiv2;

    // Set orthographic projection with 0,0,0 in the screen center
    if (_camera && _camera->projection() != P_stereoSideBySideD)
    {
        // 1. Set Projection & View
        _stateGL->projectionMatrix.ortho(-w2, w2, -h2, h2, 1.0f, -1.0f);
        _stateGL->viewport(0, 0, _scrW, _scrH);

        // 2. Pseudo 2D Frustum Culling
        _visibleNodes2D.clear();
        if (s->root2D())
            s->root2D()->cull2DRec(this);

        // 3. Draw all 2D nodes opaque
        draw2DGLNodes();

        // Draw selection rectangle
        /* The selection rectangle is defined in SLScene::selectRect and gets set and
        drawn in SLCamera::onMouseDown and SLCamera::onMouseMove. If the selectRect is
        not empty the SLScene::selectedNode is null. All vertices that are within the
        selectRect are listed in SLMesh::IS32. The selection evaluation is done during
        drawing in SLMesh::draw and is only valid for the current frame.
        All nodes that have selected vertice have their drawbit SL_DB_SELECTED set. */

        if (!s->selectedRect().isEmpty())
        {
            _stateGL->pushModelViewMatrix();
            _stateGL->modelViewMatrix.identity();
            _stateGL->modelViewMatrix.translate(-w2, h2, 1.0f);
            _stateGL->depthMask(false); // Freeze depth buffer for blending
            _stateGL->depthTest(false); // Disable depth testing
            //_stateGL->blendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR); // inverts background
            _stateGL->blend(true); // Enable blending
            //_stateGL->polygonLine(false);       // Only filled polygons

            s->selectedRect().drawGL(SLCol4f::WHITE);

            _stateGL->blend(false); // turn off blending
            //_stateGL->blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // std. transparency
            _stateGL->depthMask(true); // enable depth buffer writing
            _stateGL->depthTest(true); // enable depth testing
            _stateGL->popModelViewMatrix();
        }

        // 4. Draw ImGui UI
        if (_gui.build)
        {
            ImGui::Render();
            _gui.onPaint(ImGui::GetDrawData());
        }
    }

    _draw2DTimeMS = s->timeMilliSec() - startMS;
    return;
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::draw2DGLNodes draws 2D nodes from root2D in ortho projection.
*/
void SLSceneView::draw2DGLNodes()
{
    SLfloat depth = 1.0f;                         // Render depth between -1 & 1
    SLfloat cs    = SL_min(_scrW, _scrH) * 0.01f; // center size

    _stateGL->pushModelViewMatrix();
    _stateGL->modelViewMatrix.identity();
    _stateGL->depthMask(false);   // Freeze depth buffer for blending
    _stateGL->depthTest(false);   // Disable depth testing
    _stateGL->blend(true);        // Enable blending
    _stateGL->polygonLine(false); // Only filled polygons

    // Draw all 2D nodes blended (mostly text font textures)
    // draw the shapes directly with their wm transform
    for (auto node : _visibleNodes2D)
    {
        // Apply world transform
        _stateGL->modelViewMatrix.multiply(node->updateAndGetWM().m());

        // Finally the nodes meshes
        node->drawMeshes(this);
    }

    // Draw rotation helpers during camera animations
    if ((_mouseDownL || _mouseDownM) && _touchDowns == 0)
    {
        if (_camera->camAnim() == CA_turntableYUp ||
            _camera->camAnim() == CA_turntableZUp)
        {
            _stateGL->pushModelViewMatrix();
            _stateGL->modelViewMatrix.translate(0, 0, depth);

            SLVVec3f centerRombusPoints = {{-cs, 0, 0},
                                           {0, -cs, 0},
                                           {cs, 0, 0},
                                           {0, cs, 0}};
            _vaoTouch.clearAttribs();
            _vaoTouch.generateVertexPos(&centerRombusPoints);
            SLCol4f yelloAlpha(1.0f, 1.0f, 0.0f, 0.5f);

            _vaoTouch.drawArrayAsColored(PT_lineLoop, yelloAlpha);

            _stateGL->popModelViewMatrix();
        }
        else if (_camera->camAnim() == CA_trackball)
        {
            _stateGL->pushModelViewMatrix();
            _stateGL->modelViewMatrix.translate(0, 0, depth);

            // radius = half width or height
            SLfloat r = (SLfloat)(_scrW < _scrH
                                    ? _scrW / 2
                                    : _scrH / 2) *
                        _camera->trackballSize();

            SLVVec3f rombusAndCirclePoints; // = {{-cs,0,0},{0,-cs,0},{cs,0,0},{0,cs,0}};

            // Add points for circle over window
            SLint   circlePoints = 60;
            SLfloat deltaPhi     = SL_2PI / (SLfloat)circlePoints;
            for (SLint i = 0; i < circlePoints; ++i)
            {
                SLVec2f c;
                c.fromPolar(r, i * deltaPhi);
                rombusAndCirclePoints.push_back(SLVec3f(c.x, c.y, 0));
            }
            _vaoTouch.clearAttribs();
            _vaoTouch.generateVertexPos(&rombusAndCirclePoints);
            SLCol4f yelloAlpha(1.0f, 1.0f, 0.0f, 0.5f);

            _vaoTouch.drawArrayAsColored(PT_lineLoop, yelloAlpha);

            _stateGL->popModelViewMatrix();
        }
    }

    _stateGL->popModelViewMatrix();

    _stateGL->blend(false);    // turn off blending
    _stateGL->depthMask(true); // enable depth buffer writing
    _stateGL->depthTest(true); // enable depth testing
    GET_GL_ERROR;              // check if any OGL errors occurred
}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/*! 
SLSceneView::onMouseDown gets called whenever a mouse button gets pressed and
dispatches the event to the currently attached event handler object.
*/
SLbool SLSceneView::onMouseDown(SLMouseButton button,
                                SLint         x,
                                SLint         y,
                                SLKey         mod)
{
    SLScene* s = SLApplication::scene;

    // Pass the event to imgui
    _gui.onMouseDown(button, x, y);

#ifdef SL_GLES
    // Touch devices on iOS or Android have no mouse move event when the
    // finger isn't touching the screen. Therefore imgui can not detect hovering
    // over an imgui window. Without this extra frame you would have to touch
    // the display twice to open e.g. a menu.
    _gui.renderExtraFrame(s, this, x, y);
#endif

    if (ImGui::GetIO().WantCaptureMouse)
        return true;

    _mouseDownL = (button == MB_left);
    _mouseDownR = (button == MB_right);
    _mouseDownM = (button == MB_middle);
    _mouseMod   = mod;

    SLbool result = false;
    if (_camera && s->root3D())
    {
        result = _camera->onMouseDown(button, x, y, mod);
        for (auto eh : s->eventHandlers())
        {
            if (eh->onMouseDown(button, x, y, mod))
                result = true;
        }
    }

    // Grab image during calibration if calibration stream is running
    if (SLApplication::activeCalib->state() == CS_calibrateStream)
        SLApplication::activeCalib->state(CS_calibrateGrab);

    return result;
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onMouseUp gets called whenever a mouse button gets released.
*/
SLbool SLSceneView::onMouseUp(SLMouseButton button,
                              SLint         x,
                              SLint         y,
                              SLKey         mod)
{
    SLScene* s  = SLApplication::scene;
    _touchDowns = 0;

    // Continue with ray tracing
    if (_raytracer.state() == rtMoveGL)
    {
        _renderType = RT_rt;
        _raytracer.state(rtReady);
    }

    // Continue with path tracing
    if (_pathtracer.state() == rtMoveGL)
    {
        _renderType = RT_pt;
        _pathtracer.state(rtReady);
    }

    // Pass the event to imgui
    ImGui::GetIO().MousePos = ImVec2((SLfloat)x, (SLfloat)y);
    _gui.onMouseUp(button, x, y);

    _mouseDownL = false;
    _mouseDownR = false;
    _mouseDownM = false;

    if (_camera && s->root3D())
    {
        SLbool result = false;
        result        = _camera->onMouseUp(button, x, y, mod);
        for (auto eh : s->eventHandlers())
        {
            if (eh->onMouseUp(button, x, y, mod))
                result = true;
        }
        return result;
    }

    return false;
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onMouseMove gets called whenever the mouse is moved.
*/
SLbool SLSceneView::onMouseMove(SLint x, SLint y)
{
    SLScene* s = SLApplication::scene;

    // Pass the event to imgui
    _gui.onMouseMove(x, y);
    if (ImGui::GetIO().WantCaptureMouse)
        return true;

    if (!s->root3D()) return false;

    _touchDowns   = 0;
    SLbool result = false;

    if (_mouseDownL || _mouseDownR || _mouseDownM)
    {
        SLMouseButton btn = _mouseDownL
                              ? MB_left
                              : _mouseDownR ? MB_right : MB_middle;

        // Handle move in ray tracing
        if (_renderType == RT_rt && !_raytracer.doContinuous())
        {
            if (_raytracer.state() == rtFinished)
                _raytracer.state(rtMoveGL);
            else
            {
                _raytracer.doContinuous(false);
            }
            _renderType = RT_gl;
        }

        // Handle move in path tracing
        if (_renderType == RT_pt)
        {
            if (_pathtracer.state() == rtFinished)
                _pathtracer.state(rtMoveGL);
            _renderType = RT_gl;
        }

        result = _camera->onMouseMove(btn, x, y, _mouseMod);

        for (auto eh : s->eventHandlers())
        {
            if (eh->onMouseMove(btn, x, y, _mouseMod))
                result = true;
        }
    }
    return result;
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onMouseWheel gets called whenever the mouse wheel is turned.
The parameter wheelPos is an increasing or decreeing counter number.
*/
SLbool SLSceneView::onMouseWheelPos(SLint wheelPos, SLKey mod)
{
    SLScene* s = SLApplication::scene;
    if (!s->root3D()) return false;

    static SLint lastMouseWheelPos = 0;
    SLint        delta             = wheelPos - lastMouseWheelPos;
    lastMouseWheelPos              = wheelPos;
    return onMouseWheel(delta, mod);
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onMouseWheel gets called whenever the mouse wheel is turned.
The parameter delta is positive/negative depending on the wheel direction
*/
SLbool SLSceneView::onMouseWheel(SLint delta, SLKey mod)
{
    SLScene* s = SLApplication::scene;
    if (!s->root3D()) return false;

    // Pass the event to imgui
    if (ImGui::GetIO().WantCaptureMouse)
    {
        _gui.onMouseWheel((SLfloat)delta);
        return true;
    }

    // Handle mouse wheel in RT mode
    if (_renderType == RT_rt && !_raytracer.doContinuous() &&
        _raytracer.state() == rtFinished)
        _raytracer.state(rtReady);
    SLbool result = false;

    // update active camera
    result = _camera->onMouseWheel(delta, mod);

    for (auto eh : s->eventHandlers())
    {
        if (eh->onMouseWheel(delta, mod))
            result = true;
    }
    return result;
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onDoubleClick gets called when a mouse double click or finger 
double tab occurs.
*/
SLbool SLSceneView::onDoubleClick(SLMouseButton button,
                                  SLint         x,
                                  SLint         y,
                                  SLKey         mod)
{
    SLScene* s = SLApplication::scene;
    if (!s->root3D()) return false;

    SLbool result = false;

    // Do object picking with ray cast
    if (button == MB_left)
    {
        _mouseDownR = false;

        SLRay pickRay(this);
        if (_camera)
        {
            _camera->eyeToPixelRay((SLfloat)x, (SLfloat)y, &pickRay);
            s->root3D()->hitRec(&pickRay);
            if (pickRay.hitNode)
                cout << "NODE HIT: " << pickRay.hitNode->name() << endl;
        }

        if (pickRay.length < FLT_MAX)
        {
            s->selectedRect().setZero();
            s->selectNodeMesh(pickRay.hitNode, pickRay.hitMesh);
            if (onSelectedNodeMesh)
                onSelectedNodeMesh(s->selectedNode(), s->selectedMesh());
            result = true;
        }
    }
    else
    {
        result = _camera->onDoubleClick(button, x, y, mod);
        for (auto eh : s->eventHandlers())
        {
            if (eh->onDoubleClick(button, x, y, mod))
                result = true;
        }
    }
    return result;
}
//-----------------------------------------------------------------------------
/*! SLSceneView::onLongTouch gets called when the mouse or touch is down for
more than 500ms and has not moved.
*/
SLbool SLSceneView::onLongTouch(SLint x, SLint y)
{
    //SL_LOG("onLongTouch(%d, %d)\n", x, y);
    return true;
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onTouch2Down gets called whenever two fingers touch a handheld
screen.
*/
SLbool SLSceneView::onTouch2Down(SLint x1, SLint y1, SLint x2, SLint y2)
{
    SLScene* s = SLApplication::scene;
    if (!s->root3D()) return false;

    _touch[0].set(x1, y1);
    _touch[1].set(x2, y2);
    _touchDowns = 2;

    SLbool result = false;
    result        = _camera->onTouch2Down(x1, y1, x2, y2);
    for (auto eh : s->eventHandlers())
    {
        if (eh->onTouch2Down(x1, y1, x2, y2))
            result = true;
    }
    return result;
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onTouch2Move gets called whenever two fingers touch a handheld
screen.
*/
SLbool SLSceneView::onTouch2Move(SLint x1, SLint y1, SLint x2, SLint y2)
{
    SLScene* s = SLApplication::scene;
    if (!s->root3D()) return false;

    _touch[0].set(x1, y1);
    _touch[1].set(x2, y2);

    SLbool result = false;
    if (_touchDowns == 2)
    {
        result = _camera->onTouch2Move(x1, y1, x2, y2);
        for (auto eh : s->eventHandlers())
        {
            if (eh->onTouch2Move(x1, y1, x2, y2))
                result = true;
        }
    }
    return result;
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onTouch2Up gets called whenever two fingers touch a handheld
screen.
*/
SLbool SLSceneView::onTouch2Up(SLint x1, SLint y1, SLint x2, SLint y2)
{
    SLScene* s = SLApplication::scene;
    if (!s->root3D()) return false;

    _touch[0].set(x1, y1);
    _touch[1].set(x2, y2);
    _touchDowns   = 0;
    SLbool result = false;

    result = _camera->onTouch2Up(x1, y1, x2, y2);
    for (auto eh : s->eventHandlers())
    {
        if (eh->onTouch2Up(x1, y1, x2, y2))
            result = true;
    }
    return result;
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onKeyPress gets get called whenever a key is pressed. Before 
passing the command to the eventhandlers the main key commands are handled by
forwarding them to onCommand. 
*/
SLbool SLSceneView::onKeyPress(SLKey key, SLKey mod)
{
    SLScene* s = SLApplication::scene;
    if (!s->root3D()) return false;

    // Pass the event to imgui
    if (ImGui::GetIO().WantCaptureKeyboard)
    {
        _gui.onKeyPress(key, mod);
        return true;
    }

    // clang-format off
    // We have to coordinate these shortcuts in SLDemoGui::buildMenuBar
    if (key=='M') {doMultiSampling(!doMultiSampling()); return true;}
    if (key=='I') {doWaitOnIdle(!doWaitOnIdle()); return true;}
    if (key=='F') {doFrustumCulling(!doFrustumCulling()); return true;}
    if (key=='T') {doDepthTest(!doDepthTest()); return true;}
    if (key=='O') {s->stopAnimations(!s->stopAnimations()); return true;}

    if (key=='G') {renderType(RT_gl); return true;}
    if (key=='R') {startRaytracing(5);}

    if (key=='P') {drawBits()->toggle(SL_DB_WIREMESH); return true;}
    if (key=='N') {drawBits()->toggle(SL_DB_NORMALS); return true;}
    if (key=='B') {drawBits()->toggle(SL_DB_BBOX); return true;}
    if (key=='V') {drawBits()->toggle(SL_DB_VOXELS); return true;}
    if (key=='X') {drawBits()->toggle(SL_DB_AXIS); return true;}
    if (key=='C') {drawBits()->toggle(SL_DB_CULLOFF); return true;}
    if (key=='K') {drawBits()->toggle(SL_DB_SKELETON); return true;}

    if (key=='5')
    {   if (_camera->projection() == P_monoPerspective)
             _camera->projection(P_monoOrthographic);
        else _camera->projection(P_monoPerspective);
        if (_renderType == RT_rt && !_raytracer.doContinuous() &&
            _raytracer.state() == rtFinished)
            _raytracer.state(rtReady);
    }

    if (key==K_tab) {switchToNextCameraInScene(); return true;};

    if (key==K_esc && mod==K_ctrl)
    {   if(_renderType == RT_rt)
        {  _stopRT = true;
            return false;
        }
        else if(_renderType == RT_pt)
        {  _stopPT = true;
            return false;
        }
        else return true; // end the program
    }
    // clang-format on

    SLbool result = false;
    if (key || mod)
    {
        // 1) pass it to the camera
        result = _camera->onKeyPress(key, mod);

        // 2) pass it to any other eventhandler
        for (auto eh : s->eventHandlers())
        {
            if (eh->onKeyPress(key, mod))
                result = true;
        }
    }
    return result;
}
//-----------------------------------------------------------------------------
/*! 
SLSceneView::onKeyRelease get called whenever a key is released.
*/
SLbool SLSceneView::onKeyRelease(SLKey key, SLKey mod)
{
    SLScene* s = SLApplication::scene;

    // Pass the event to imgui
    if (ImGui::GetIO().WantCaptureKeyboard)
    {
        _gui.onKeyRelease(key, mod);
        return true;
    }

    if (!s->root3D()) return false;

    SLbool result = false;

    if (key || mod)
    {
        // 1) pass it to the camera
        result = _camera->onKeyRelease(key, mod);

        // 2) pass it to any other eventhandler
        for (auto eh : s->eventHandlers())
        {
            if (eh->onKeyRelease(key, mod))
                result = true;
        }
    }
    return result;
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::onCharInput get called whenever a new charcter comes in
*/
SLbool SLSceneView::onCharInput(SLuint c)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
    {
        _gui.onCharInput(c);
        return true;
    }
    return false;
}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/*!
Returns the window title with name & FPS
*/
SLstring SLSceneView::windowTitle()
{
    SLScene* s = SLApplication::scene;
    SLchar   title[255];

    if (_renderType == RT_rt)
    {
        if (_raytracer.doContinuous())
        {
            sprintf(title,
                    "%s (fps: %4.1f, Threads: %d)",
                    s->name().c_str(),
                    s->fps(),
                    _raytracer.numThreads());
        }
        else
        {
            sprintf(title,
                    "%s (%d%%, Threads: %d)",
                    s->name().c_str(),
                    _raytracer.pcRendered(),
                    _raytracer.numThreads());
        }
    }
    else if (_renderType == RT_pt)
    {
        sprintf(title,
                "%s (%d%%, Threads: %d)",
                s->name().c_str(),
                _pathtracer.pcRendered(),
                _pathtracer.numThreads());
    }
    else
    {
        SLuint nr = (uint)_visibleNodes.size();
        if (s->fps() > 5)
            sprintf(title,
                    "%s (fps: %4.0f, %u nodes of %u rendered)",
                    s->name().c_str(),
                    s->fps(),
                    nr,
                    _stats3D.numNodes);
        else
            sprintf(title,
                    "%s (fps: %4.1f, %u nodes of %u rendered)",
                    s->name().c_str(),
                    s->fps(),
                    nr,
                    _stats3D.numNodes);
    }
    return SLstring(title);
}
//-----------------------------------------------------------------------------
/*!
Starts the ray tracing & sets the RT menu
*/
void SLSceneView::startRaytracing(SLint maxDepth)
{
    _renderType = RT_rt;
    _stopRT     = false;
    _raytracer.maxDepth(maxDepth);
    _raytracer.aaSamples(_doMultiSampling && SLApplication::dpi < 200 ? 3 : 1);
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::updateAndRT3D starts the raytracing or refreshes the current RT
image during rendering. The function returns true if an animation was done 
prior to the rendering start.
*/
SLbool SLSceneView::draw3DRT()
{
    SLbool updated = false;

    // if the raytracer not yet got started
    if (_raytracer.state() == rtReady)
    {
        SLScene* s = SLApplication::scene;

        // Update transforms and aabbs
        // @Todo: causes multithreading bug in RT
        //s->root3D()->needUpdate();

        // Do software skinning on all changed skeletons
        for (auto mesh : s->meshes())
            mesh->updateAccelStruct();

        // Start raytracing
        if (_raytracer.doDistributed())
            _raytracer.renderDistrib(this);
        else
            _raytracer.renderClassic(this);
    }

    // Refresh the render image during RT
    _raytracer.renderImage();

    // React on the stop flag (e.g. ESC)
    if (_stopRT)
    {
        _renderType = RT_gl;
        updated     = true;
    }

    return updated;
}
//-----------------------------------------------------------------------------
/*!
Starts the pathtracing
*/
void SLSceneView::startPathtracing(SLint maxDepth, SLint samples)
{
    _renderType = RT_pt;
    _stopPT     = false;
    _pathtracer.maxDepth(maxDepth);
    _pathtracer.aaSamples(samples);
}
//-----------------------------------------------------------------------------
/*!
SLSceneView::updateAndRT3D starts the raytracing or refreshes the current RT
image during rendering. The function returns true if an animation was done 
prior to the rendering start.
*/
SLbool SLSceneView::draw3DPT()
{
    SLbool updated = false;

    // if the pathtracer not yet got started
    if (_pathtracer.state() == rtReady)
    {
        SLScene* s = SLApplication::scene;

        // Update transforms and AABBs
        // @Todo: causes multithreading bug in RT
        //s->root3D()->needUpdate();

        // Do software skinning on all changed skeletons
        for (auto mesh : s->meshes())
            mesh->updateAccelStruct();

        // Start raytracing
        _pathtracer.render(this);
    }

    // Refresh the render image during PT
    _pathtracer.renderImage();

    // React on the stop flag (e.g. ESC)
    if (_stopPT)
    {
        _renderType = RT_gl;
        updated     = true;
    }

    return updated;
}
//-----------------------------------------------------------------------------
