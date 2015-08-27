##############################################################################
#  File:      SLProjectDeploy.pro
#  Purpose:   QMake project definition for SLProject deployment
#  Author:    Marcus Hudritsch
#  Date:      February 2014
#  Copyright: Marcus Hudritsch, Switzerland
#             THIS SOFTWARE IS PROVIDED FOR EDUCATIONAL PURPOSE ONLY AND
#             WITHOUT ANY WARRANTIES WHETHER EXPRESSED OR IMPLIED.
##############################################################################

textures.files = \
  ../_data/images/textures/brick0512_C.png \
  ../_data/images/textures/bricks1_0256_C.jpg \
  ../_data/images/textures/bricks1_0512_C.jpg \
  ../_data/images/textures/brickwall0512_C.jpg \
  ../_data/images/textures/brickwall0512_G.jpg \
  ../_data/images/textures/brickwall0512_H.jpg \
  ../_data/images/textures/brickwall0512_N.jpg \
  ../_data/images/textures/Checkerboard0512_C.png \
  ../_data/images/textures/Chess0256_C.bmp \
  ../_data/images/textures/CompileError.png \
  ../_data/images/textures/cursor.tga \
  ../_data/images/textures/earth1024_C.jpg \
  ../_data/images/textures/earth1024_G.jpg \
  ../_data/images/textures/earth1024_H.jpg \
  ../_data/images/textures/earth1024_N.jpg \
  ../_data/images/textures/earthCloud1024_A.jpg \
  ../_data/images/textures/earthCloud1024_C.jpg \
  ../_data/images/textures/earthNight1024_C.jpg \
  ../_data/images/textures/grass0512_C.jpg \
  ../_data/images/textures/gray_0256_C.jpg \
  ../_data/images/textures/MuttenzerBox+X0512_C.png \
  ../_data/images/textures/MuttenzerBox+Y0512_C.png \
  ../_data/images/textures/MuttenzerBox+Z0512_C.png \
  ../_data/images/textures/MuttenzerBox-X0512_C.png \
  ../_data/images/textures/MuttenzerBox-Y0512_C.png \
  ../_data/images/textures/MuttenzerBox-Z0512_C.png \
  ../_data/images/textures/Pool+X0512_C.png \
  ../_data/images/textures/Pool+Y0512_C.png \
  ../_data/images/textures/Pool+Z0512_C.png \
  ../_data/images/textures/Pool-X0512_C.png \
  ../_data/images/textures/Pool-Y0512_C.png \
  ../_data/images/textures/Pool-Z0512_C.png \
  ../_data/images/textures/Testmap_0512_C.png \
  ../_data/images/textures/tile1_0256_C.jpg \
  ../_data/images/textures/tree1_1024_C.png \
  ../_data/images/textures/VisionExample.png \
  ../_data/images/textures/VisionTest.png \
  ../_data/images/textures/Wave_radial10_256C.jpg \
  ../_data/images/textures/wood0_0256_C.jpg \
  ../_data/images/textures/wood0_0512_C.jpg \
  ../_data/images/textures/wood2_0256_C.jpg \
  ../_data/images/textures/wood2_0512_C.jpg \
  ../_data/images/textures/LiveVideoError.png \

shaders.files = \
  ../_data/shaders/BumpNormal.frag \
  ../_data/shaders/BumpNormal.vert \
  ../_data/shaders/BumpNormalEarth.frag \
  ../_data/shaders/BumpNormalParallax.frag \
  ../_data/shaders/Color.frag \
  ../_data/shaders/ColorAttribute.vert \
  ../_data/shaders/ColorUniform.vert \
  ../_data/shaders/Diffuse.frag \
  ../_data/shaders/Diffuse.vert \
  ../_data/shaders/Earth.frag \
  ../_data/shaders/ErrorTex.frag \
  ../_data/shaders/ErrorTex.vert \
  ../_data/shaders/FontTex.frag \
  ../_data/shaders/FontTex.vert \
  ../_data/shaders/PerPixBlinn.frag \
  ../_data/shaders/PerPixBlinn.vert \
  ../_data/shaders/PerPixBlinnSkinned.vert \
  ../_data/shaders/PerPixBlinnTex.frag \
  ../_data/shaders/PerPixBlinnTex.vert \
  ../_data/shaders/PerPixBlinnTexSkinned.vert \
  ../_data/shaders/PerVrtBlinn.frag \
  ../_data/shaders/PerVrtBlinn.vert \
  ../_data/shaders/PerVrtBlinnSkinned.vert \
  ../_data/shaders/PerVrtBlinnTex.frag \
  ../_data/shaders/PerVrtBlinnTex.vert \
  ../_data/shaders/PerVrtBlinnTexSkinned.vert \
  ../_data/shaders/Reflect.frag \
  ../_data/shaders/Reflect.vert \
  ../_data/shaders/RefractReflect.frag \
  ../_data/shaders/RefractReflect.vert \
  ../_data/shaders/RefractReflectDisp.frag \
  ../_data/shaders/RefractReflectDisp.vert \
  ../_data/shaders/SceneOculus.frag \
  ../_data/shaders/SceneOculus.vert \
  ../_data/shaders/ShadowMapping.frag \
  ../_data/shaders/ShadowMapping.vert \
  ../_data/shaders/StereoOculus.frag \
  ../_data/shaders/StereoOculus.vert \
  ../_data/shaders/StereoOculusDistortionMesh.frag \
  ../_data/shaders/StereoOculusDistortionMesh.vert \
  ../_data/shaders/TextureOnly.frag \
  ../_data/shaders/TextureOnly.vert \
  ../_data/shaders/TextureOnly3D.frag \
  ../_data/shaders/TextureOnly3D.vert \
  ../_data/shaders/Wave.frag \
  ../_data/shaders/Wave.vert \
  ../_data/shaders/WaveRefractReflect.vert \

models_3DS_Halloween.files = \
  ../_data/models/3DS/Halloween/GUTS.BMP \
  ../_data/models/3DS/Halloween/JACKTOP.BMP \
  ../_data/models/3DS/Halloween/JACKTO_B.BMP \
  ../_data/models/3DS/Halloween/JACK_B.BMP \
  ../_data/models/3DS/Halloween/JACK_C.BMP \
  ../_data/models/3DS/Halloween/STEM.BMP \
  ../_data/models/3DS/Halloween/STEM_B.BMP \
  ../_data/models/3DS/Halloween/jackolan.3ds \
  ../_data/models/3DS/Halloween/kerze.3DS \
  ../_data/models/3DS/Halloween/stem.3DS \

models_DAE_AstroBoy.files = \
  ../_data/models/DAE/AstroBoy/AstroBoy.dae \
  ../_data/models/DAE/AstroBoy/boy_10.jpg \

models_DAE_SkinnedCube.files = \
  ../_data/models/DAE/SkinnedCube/skinnedcube.dae \
  ../_data/models/DAE/SkinnedCube/skinnedcube2.dae \
  ../_data/models/DAE/SkinnedCube/skinnedcube4.dae \
  ../_data/models/DAE/SkinnedCube/skinnedcube5.dae \

models_FBX_Duck.files = \
  ../_data/models/FBX/Duck/duck.fbx \
  ../_data/models/FBX/Duck/duckCM.png \

models_FBX_Axes.files = \
  ../_data/models/FBX/Axes/axes_blender.fbx \

models_OBJ_Christoffelturm.files = \
  ../_data/models/OBJ/Christoffelturm/christoffelturm.obj \
  ../_data/models/OBJ/Christoffelturm/christoffelturm.mtl \
  ../_data/models/OBJ/Christoffelturm/texture1.jpg \
  ../_data/models/OBJ/Christoffelturm/texture2.jpg \
  ../_data/models/OBJ/Christoffelturm/shadow.png \


# Copies the given files to the destination directory
defineTest(copyToDestdir) {
    files = $$1
    for(FILE, files) {
        DDIR = $$DESTDIR
        win32:FILE ~= s,/,\\,g # Replace slashes in paths with backslashes for Windows
        win32:DDIR ~= s,/,\\,g
        QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$FILE) $$quote($$DDIR) $$escape_expand(\\n\\t)
    }
    export(QMAKE_POST_LINK)
}

# OpenCV
win32 {
    copyToDestdir($$PWD\_lib\prebuilt\OpenCV\x64\vc12\bin\opencv_core300.dll)
    copyToDestdir($$PWD\_lib\prebuilt\OpenCV\x64\vc12\bin\opencv_imgproc300.dll)
    copyToDestdir($$PWD\_lib\prebuilt\OpenCV\x64\vc12\bin\opencv_imgcodecs300.dll)
    copyToDestdir($$PWD\_lib\prebuilt\OpenCV\x64\vc12\bin\opencv_video300.dll)
    copyToDestdir($$PWD\_lib\prebuilt\OpenCV\x64\vc12\bin\opencv_videoio300.dll)
}
macx {
    cvlibs.files += \
        ../_lib/prebuilt/OpenCV/macx/libopencv_core.3.0.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_core.3.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_core.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_imgproc.3.0.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_imgproc.3.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_imgproc.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_video.3.0.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_video.3.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_video.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_videoio.3.0.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_videoio.3.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_videoio.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_imgcodecs.3.0.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_imgcodecs.3.0.dylib \
        ../_lib/prebuilt/OpenCV/macx/libopencv_imgcodecs.dylib
}
unix:!macx:!android {
    # linux only
}

# Deployment
macx: {
    textures.path = Contents/_data/images/textures
    shaders.path = Contents/_data/shaders
    models_3DS_Halloween.path = Contents/_data/models/3DS/Halloween
    models_DAE_AstroBoy.path = Contents/_data/models/DAE/AstroBoy
    models_DAE_SkinnedCube.path = Contents/_data/models/DAE/SkinnedCube
    models_FBX_Duck.path = Contents/_data/models/FBX/Duck
    models_FBX_Axes.path = Contents/_data/models/FBX/Axes
    models_OBJ_Christoffelturm.path = Contents/_data/models/OBJ/Christoffelturm
    cvlibs.path = Contents/Frameworks

    QMAKE_BUNDLE_DATA += textures
    QMAKE_BUNDLE_DATA += shaders
    QMAKE_BUNDLE_DATA += models_3DS_Halloween
    QMAKE_BUNDLE_DATA += models_DAE_AstroBoy
    QMAKE_BUNDLE_DATA += models_DAE_SkinnedCube
    QMAKE_BUNDLE_DATA += models_FBX_Duck
    QMAKE_BUNDLE_DATA += models_FBX_Axes
    QMAKE_BUNDLE_DATA += models_OBJ_Christoffelturm
    QMAKE_BUNDLE_DATA += cvlibs

    macx {ICON = ../lib-SLProject/SLProject-Icon.icns}

    #run macdeployqt
    QMAKE_POST_LINK += macdeployqt ../_bin-$$CONFIGURATION-$$PLATFORM/$${TARGET}.app/
}

message(-----------------------------------------)
message(Target: $$TARGET)
message(Config: $$CONFIGURATION)
message(Platform: $$PLATFORM)