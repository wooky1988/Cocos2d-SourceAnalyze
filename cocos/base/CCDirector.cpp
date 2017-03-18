/****************************************************************************
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2010-2013 cocos2d-x.org
Copyright (c) 2011      Zynga Inc.
Copyright (c) 2013-2015 Chukong Technologies Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

// cocos2d includes
#include "base/CCDirector.h"

// standard includes
#include <string>

#include "2d/CCDrawingPrimitives.h"
#include "2d/CCSpriteFrameCache.h"
#include "platform/CCFileUtils.h"

#include "2d/CCActionManager.h"
#include "2d/CCFontFNT.h"
#include "2d/CCFontAtlasCache.h"
#include "2d/CCAnimationCache.h"
#include "2d/CCTransition.h"
#include "2d/CCFontFreeType.h"
#include "2d/CCLabelAtlas.h"
#include "renderer/CCGLProgramCache.h"
#include "renderer/CCGLProgramStateCache.h"
#include "renderer/CCTextureCache.h"
#include "renderer/ccGLStateCache.h"
#include "renderer/CCRenderer.h"
#include "2d/CCCamera.h"
#include "base/CCUserDefault.h"
#include "base/ccFPSImages.h"
#include "base/CCScheduler.h"
#include "base/ccMacros.h"
#include "base/CCEventDispatcher.h"
#include "base/CCEventCustom.h"
#include "base/CCConsole.h"
#include "base/CCAutoreleasePool.h"
#include "base/CCConfiguration.h"
#include "base/CCAsyncTaskPool.h"
#include "platform/CCApplication.h"
//#include "platform/CCGLViewImpl.h"

#if CC_ENABLE_SCRIPT_BINDING
#include "CCScriptSupport.h"
#endif

#if CC_USE_PHYSICS
#include "physics/CCPhysicsWorld.h"
#endif

/**
 Position of the FPS
 
 Default: 0,0 (bottom-left corner)
 */
#ifndef CC_DIRECTOR_STATS_POSITION
#define CC_DIRECTOR_STATS_POSITION Director::getInstance()->getVisibleOrigin()
#endif // CC_DIRECTOR_STATS_POSITION

using namespace std;

NS_CC_BEGIN
// FIXME: it should be a Director ivar. Move it there once support for multiple directors is added

// singleton stuff
static DisplayLinkDirector *s_SharedDirector = nullptr;

#define kDefaultFPS        60  // 60 frames per second
extern const char* cocos2dVersion(void);

const char *Director::EVENT_PROJECTION_CHANGED = "director_projection_changed";
const char *Director::EVENT_AFTER_DRAW = "director_after_draw";
const char *Director::EVENT_AFTER_VISIT = "director_after_visit";
const char *Director::EVENT_AFTER_UPDATE = "director_after_update";

//获得的是DisplayLinkDirector实例
Director* Director::getInstance()
{
    if (!s_SharedDirector)
    {
        s_SharedDirector = new (std::nothrow) DisplayLinkDirector();
        CCASSERT(s_SharedDirector, "FATAL: Not enough memory");
        s_SharedDirector->init();
    }

    return s_SharedDirector;
}

Director::Director()
: _isStatusLabelUpdated(true)
{
}
//导演类初始化init方法
bool Director::init(void)
{
    setDefaultValues();

    // scenes
    _runningScene = nullptr;
    _nextScene = nullptr;

    _notificationNode = nullptr;

    _scenesStack.reserve(15);

    // FPS
    _accumDt = 0.0f;
    _frameRate = 0.0f;
    _FPSLabel = _drawnBatchesLabel = _drawnVerticesLabel = nullptr;
    _totalFrames = 0;
    _lastUpdate = new struct timeval;
    _secondsPerFrame = 1.0f;

    // paused ?
    _paused = false;

    // purge ?
    _purgeDirectorInNextLoop = false;
    
    // restart ?
    _restartDirectorInNextLoop = false;

    _winSizeInPoints = Size::ZERO;

    _openGLView = nullptr;

    _contentScaleFactor = 1.0f;

    _console = new (std::nothrow) Console;

    // scheduler
    _scheduler = new (std::nothrow) Scheduler();
    // action manager
    _actionManager = new (std::nothrow) ActionManager();
    _scheduler->scheduleUpdate(_actionManager, Scheduler::PRIORITY_SYSTEM, false);
    //所有事件的监听和事件分发都是靠EventDispatcher完成.
    _eventDispatcher = new (std::nothrow) EventDispatcher();
    _eventAfterDraw = new (std::nothrow) EventCustom(EVENT_AFTER_DRAW);
    _eventAfterDraw->setUserData(this);
    _eventAfterVisit = new (std::nothrow) EventCustom(EVENT_AFTER_VISIT);
    _eventAfterVisit->setUserData(this);
    _eventAfterUpdate = new (std::nothrow) EventCustom(EVENT_AFTER_UPDATE);
    _eventAfterUpdate->setUserData(this);
    _eventProjectionChanged = new (std::nothrow) EventCustom(EVENT_PROJECTION_CHANGED);
    _eventProjectionChanged->setUserData(this);


    //init TextureCache
    initTextureCache();
    //20161023:初始化Cocos2d-x整个引擎生命周期内会用到的与opengl变换矩阵相关的一些全局变量
    initMatrixStack();
    //初始化渲染类
    _renderer = new (std::nothrow) Renderer;

    return true;
}

Director::~Director(void)
{
    CCLOGINFO("deallocing Director: %p", this);

    CC_SAFE_RELEASE(_FPSLabel);
    CC_SAFE_RELEASE(_drawnVerticesLabel);
    CC_SAFE_RELEASE(_drawnBatchesLabel);

    CC_SAFE_RELEASE(_runningScene);
    CC_SAFE_RELEASE(_notificationNode);
    CC_SAFE_RELEASE(_scheduler);
    CC_SAFE_RELEASE(_actionManager);
    
    delete _eventAfterUpdate;
    delete _eventAfterDraw;
    delete _eventAfterVisit;
    delete _eventProjectionChanged;

    delete _renderer;

    delete _console;


    CC_SAFE_RELEASE(_eventDispatcher);
    
    // delete _lastUpdate
    CC_SAFE_DELETE(_lastUpdate);

    Configuration::destroyInstance();

    s_SharedDirector = nullptr;
}
//设置一些初始化数值。
void Director::setDefaultValues(void)
{
    Configuration *conf = Configuration::getInstance();

    // default FPS
    double fps = conf->getValue("cocos2d.x.fps", Value(kDefaultFPS)).asDouble();
    _oldAnimationInterval = _animationInterval = 1.0 / fps;

    // Display FPS
    _displayStats = conf->getValue("cocos2d.x.display_fps", Value(false)).asBool();

    // GL projection
    std::string projection = conf->getValue("cocos2d.x.gl.projection", Value("3d")).asString();
    //由于conf中没有配置“cocos2d.x.gl.projection”，
    //因此projection使用了 getCString传入的默认值："3d"
    //，_projection则被赋值为Projection::_3D
    if (projection == "3d")
        _projection = Projection::_3D;//这里默认设置成3D模式
    else if (projection == "2d")
        _projection = Projection::_2D;
    else if (projection == "custom")
        _projection = Projection::CUSTOM;
    else
        CCASSERT(false, "Invalid projection value");

    // Default pixel format for PNG images with alpha
    std::string pixel_format = conf->getValue("cocos2d.x.texture.pixel_format_for_png", Value("rgba8888")).asString();
    if (pixel_format == "rgba8888")
        Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);
    else if(pixel_format == "rgba4444")
        Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA4444);
    else if(pixel_format == "rgba5551")
        Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGB5A1);

    // PVR v2 has alpha premultiplied ?
    bool pvr_alpha_premultipled = conf->getValue("cocos2d.x.texture.pvrv2_has_alpha_premultiplied", Value(false)).asBool();
    Image::setPVRImagesHavePremultipliedAlpha(pvr_alpha_premultipled);
}
//20161023该方法用于初始化一些OpenGL的参数，建立好后续 OpenGL操作时所需要的各种数据结构。
void Director::setGLDefaultValues()
{
    // This method SHOULD be called only after openGLView_ was initialized
    CCASSERT(_openGLView, "opengl view should not be null");

    setAlphaBlending(true);
    setDepthTest(false);
    setProjection(_projection);//是实际上绘制参数设置的核心。
}

// Draw the Scene
void Director::drawScene()
{
    // calculate "global" dt
    //更新_deltaTime（上次调用该函数到这次调用的间隔时间）
    //更新_lastUpdate（记录上次调用的时间点）
    calculateDeltaTime();
    
    if (_openGLView)
    {
        _openGLView->pollEvents();
    }

    //tick before glClear: issue #533
    if (! _paused)
    {
        _scheduler->update(_deltaTime);//调度系统就在这里驱动的
        //自定义事件分发处理，每次刷新结束时触发一下事件的分发
        _eventDispatcher->dispatchEvent(_eventAfterUpdate);//TODO:update结束了我要发送一个消息

    }

    _renderer->clear();//开启颜色和深度缓冲区并开始绘制

    /* to avoid flickr, nextScene MUST be here: after tick and before draw.
     * FIXME: Which bug is this one. It seems that it can't be reproduced with v0.9
     */
    if (_nextScene)
    {
        setNextScene();
    }
    //向当前OpenGL变换矩阵栈Push元素
    //02，将栈顶的矩阵复制并压栈。
    pushMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
    
    if (_runningScene)
    {
#if CC_USE_PHYSICS
        auto physicsWorld = _runningScene->getPhysicsWorld();
        if (physicsWorld && physicsWorld->isAutoStep())
        {
            physicsWorld->update(_deltaTime, false);
        }
#endif
        //clear draw stats
        _renderer->clearDrawStats();
        
        //render the scene
        //从这里开起渲染的整个操作流程。
        //调用Scene的Visit方法和render的render()方法开启OpenGl的UI渲染。
        _runningScene->render(_renderer);
        
        _eventDispatcher->dispatchEvent(_eventAfterVisit);
    }

    // draw the notifications node
    // 绘制悬浮节点，如果叫global Node比较好理解，不随场景的切换而消失
    if (_notificationNode)
    {
        _notificationNode->visit(_renderer, Mat4::IDENTITY, 0);
    }

    if (_displayStats)
    {
        showStats(); //屏幕上显示帧率
    }
    _renderer->render();
    //TODO: 渲染结束了我要发一个消息
    _eventDispatcher->dispatchEvent(_eventAfterDraw);
    //从当前OpenGL变换矩阵栈Pop元素
    popMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);

    _totalFrames++;//计算从程序开始到现在共刷过多少帧

    // swap buffers
    if (_openGLView)
    {
        _openGLView->swapBuffers();
    }

    if (_displayStats)
    {
        calculateMPF();//计算_secondsPerFrame
    }
}
//更新_deltaTime（上次调用该函数到这次调用的间隔时间）
//更新_lastUpdate（记录上次调用的时间点）
void Director::calculateDeltaTime()
{
    struct timeval now;

    if (gettimeofday(&now, nullptr) != 0)
    {
        CCLOG("error in gettimeofday");
        _deltaTime = 0;
        return;
    }

    // new delta time. Re-fixed issue #1277
    if (_nextDeltaTimeZero)
    {
        _deltaTime = 0;
        _nextDeltaTimeZero = false;
    }
    else
    {
        _deltaTime = (now.tv_sec - _lastUpdate->tv_sec) + (now.tv_usec - _lastUpdate->tv_usec) / 1000000.0f;
        _deltaTime = MAX(0, _deltaTime);
    }

#if COCOS2D_DEBUG
    // If we are debugging our code, prevent big delta time
    if (_deltaTime > 0.2f)
    {
        _deltaTime = 1 / 60.0f;
    }
#endif

    *_lastUpdate = now;
}
float Director::getDeltaTime() const
{
    return _deltaTime;
}
void Director::setOpenGLView(GLView *openGLView)
{
    CCASSERT(openGLView, "opengl view should not be null");

    if (_openGLView != openGLView)
    {
        // Configuration. Gather GPU info
        Configuration *conf = Configuration::getInstance();
        conf->gatherGPUInfo();
        CCLOG("%s\n",conf->getInfo().c_str());

        if(_openGLView)
            _openGLView->release();
        _openGLView = openGLView;
        _openGLView->retain();

        // set size
        //由于尚未调用setDesignResolutionSize，因此_winSizeInPoints的值与FrameSize大小相同
        //在GLView的setFrame方法中将设计分辨率和屏幕分辨率初始化为一样的。
        _winSizeInPoints = _openGLView->getDesignResolutionSize();

        _isStatusLabelUpdated = true;

        if (_openGLView)
        {
            setGLDefaultValues();
        }

        _renderer->initGLView();

        CHECK_GL_ERROR_DEBUG();

        if (_eventDispatcher)
        {
            _eventDispatcher->setEnabled(true);
        }
    }
}

TextureCache* Director::getTextureCache() const
{
    return _textureCache;
}

void Director::initTextureCache()
{
#ifdef EMSCRIPTEN
    _textureCache = new (std::nothrow) TextureCacheEmscripten();
#else
    _textureCache = new (std::nothrow) TextureCache();
#endif // EMSCRIPTEN
}

void Director::destroyTextureCache()
{
    if (_textureCache)
    {
        _textureCache->waitForQuit();
        CC_SAFE_RELEASE_NULL(_textureCache);
    }
}
//20161023:设置视口(ViewPort)
void Director::setViewport()
{
    if (_openGLView)
    {
        _openGLView->setViewPortInPoints(0, 0, _winSizeInPoints.width, _winSizeInPoints.height);
    }
}

void Director::setNextDeltaTimeZero(bool nextDeltaTimeZero)
{
    _nextDeltaTimeZero = nextDeltaTimeZero;
}

//
// FIXME TODO
// Matrix code MUST NOT be part of the Director
// MUST BE moved outide.
// Why the Director must have this code ?
//声明了三个变换矩阵的栈
//每个矩阵先pop清空，然后再分别push一个单位矩阵。
//modelview_matrix_stack（模型视图矩阵栈）
//projection_matrix_stack（投影矩阵栈）以及texture_matrix_stack（纹理矩阵栈）。
void Director::initMatrixStack()
{
    while (!_modelViewMatrixStack.empty())
    {
        _modelViewMatrixStack.pop();
    }
    
    while (!_projectionMatrixStack.empty())
    {
        _projectionMatrixStack.pop();
    }
    
    while (!_textureMatrixStack.empty())
    {
        _textureMatrixStack.pop();
    }
    //初始化了单位矩阵。
    //将单位矩阵分别圧入不同的matrix stack。
    _modelViewMatrixStack.push(Mat4::IDENTITY);
    _projectionMatrixStack.push(Mat4::IDENTITY);
    _textureMatrixStack.push(Mat4::IDENTITY);
}

void Director::resetMatrixStack()
{
    initMatrixStack();
}

void Director::popMatrix(MATRIX_STACK_TYPE type)
{
    if(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW == type)
    {
        _modelViewMatrixStack.pop();
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION == type)
    {
        _projectionMatrixStack.pop();
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE == type)
    {
        _textureMatrixStack.pop();
    }
    else
    {
        CCASSERT(false, "unknow matrix stack type");
    }
}
//20161023：加载单位矩阵。所谓"单位矩阵"，指的是对脚线上元素都为1的矩阵。
//最后将不同的单位矩阵压入到对应的不同的栈中。cocos2d只用到前两个.
void Director::loadIdentityMatrix(MATRIX_STACK_TYPE type)
{
    if(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW == type)
    {     //模型视图矩阵栈
        _modelViewMatrixStack.top() = Mat4::IDENTITY;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION == type)
    {     //投影矩阵栈
        _projectionMatrixStack.top() = Mat4::IDENTITY;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE == type)
    {    //纹理矩阵栈
        _textureMatrixStack.top() = Mat4::IDENTITY;
    }
    else
    {
        CCASSERT(false, "unknow matrix stack type");
    }
}

void Director::loadMatrix(MATRIX_STACK_TYPE type, const Mat4& mat)
{
    if(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW == type)
    {
        _modelViewMatrixStack.top() = mat;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION == type)
    {
        _projectionMatrixStack.top() = mat;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE == type)
    {
        _textureMatrixStack.top() = mat;
    }
    else
    {
        CCASSERT(false, "unknow matrix stack type");
    }
}

void Director::multiplyMatrix(MATRIX_STACK_TYPE type, const Mat4& mat)
{
    if(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW == type)
    {
        _modelViewMatrixStack.top() *= mat;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION == type)
    {
        _projectionMatrixStack.top() *= mat;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE == type)
    {
        _textureMatrixStack.top() *= mat;
    }
    else
    {
        CCASSERT(false, "unknow matrix stack type");
    }
}

void Director::pushMatrix(MATRIX_STACK_TYPE type)
{
    if(type == MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW)
    {
        _modelViewMatrixStack.push(_modelViewMatrixStack.top());
    }
    else if(type == MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION)
    {
        _projectionMatrixStack.push(_projectionMatrixStack.top());
    }
    else if(type == MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE)
    {
        _textureMatrixStack.push(_textureMatrixStack.top());
    }
    else
    {
        CCASSERT(false, "unknow matrix stack type");
    }
}

const Mat4& Director::getMatrix(MATRIX_STACK_TYPE type)
{
    if(type == MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW)
    {
        return _modelViewMatrixStack.top();
    }
    else if(type == MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION)
    {
        return _projectionMatrixStack.top();
    }
    else if(type == MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE)
    {
        return _textureMatrixStack.top();
    }

    CCASSERT(false, "unknow matrix stack type, will return modelview matrix instead");
    return  _modelViewMatrixStack.top();
}

void Director::setProjection(Projection projection)
{
    Size size = _winSizeInPoints;

    setViewport();//设置视口(ViewPort)大小为winSize

    switch (projection)
    {
        case Projection::_2D:
        {
            loadIdentityMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION);
#if CC_TARGET_PLATFORM == CC_PLATFORM_WP8
            if(getOpenGLView() != nullptr)
            {
                multiplyMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION, getOpenGLView()->getOrientationMatrix());
            }
#endif
            Mat4 orthoMatrix;
            Mat4::createOrthographicOffCenter(0, size.width, 0, size.height, -1024, 1024, &orthoMatrix);
            multiplyMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION, orthoMatrix);
            loadIdentityMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
            break;
        }
            
        case Projection::_3D://设置投影变换矩阵。
        {
            float zeye = this->getZEye();

            Mat4 matrixPerspective, matrixLookup;
            //20161023:把投影变化的单位矩阵压入对应的栈中，设置成Top元素
            loadIdentityMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION);
            
#if CC_TARGET_PLATFORM == CC_PLATFORM_WP8
            //if needed, we need to add a rotation for Landscape orientations on Windows Phone 8 since it is always in Portrait Mode
            GLView* view = getOpenGLView();
            if(getOpenGLView() != nullptr)
            {
                multiplyMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION, getOpenGLView()->getOrientationMatrix());
            }
#endif
            // issue #1334
            //20161023:设置“投影变换”矩阵参数
            //创建透视投影矩阵，参数依次为：上下视角为60度，长宽比，近平面，远平面
            Mat4::createPerspective(60, (GLfloat)size.width/size.height, 10, zeye+size.height/2, &matrixPerspective);

            multiplyMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION, matrixPerspective);
            //20161023:设置“模型视图变换”矩阵参数
            //在LookAt创建视点是传入的视点的x,y,z的坐标其实就是
            //这三个值：size.width/2, size.height/2, zeye
            //可以看出eye在xy平面的投影恰好是以屏幕分辨率构成的矩形的中心。
            //centre坐标，表示的是视线方向，该方向矢量是由eye坐标centre坐标共同构成的由eye指向center。

            Vec3 eye(size.width/2, size.height/2, zeye), center(size.width/2, size.height/2, 0.0f), up(0.0f, 1.0f, 0.0f);
            Mat4::createLookAt(eye, center, up, &matrixLookup);
            multiplyMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION, matrixLookup);
            //20161023:把模型视图变化矩阵压入对应的栈中，设置成Top元素
            loadIdentityMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
            break;
        }

        case Projection::CUSTOM:
            // Projection Delegate is no longer needed
            // since the event "PROJECTION CHANGED" is emitted
            break;

        default:
            CCLOG("cocos2d: Director: unrecognized projection");
            break;
    }

    _projection = projection;
    GL::setProjectionMatrixDirty();

    _eventDispatcher->dispatchEvent(_eventProjectionChanged);
}

void Director::purgeCachedData(void)
{
    FontFNT::purgeCachedData();
    FontAtlasCache::purgeCachedData();

    if (s_SharedDirector->getOpenGLView())
    {
        SpriteFrameCache::getInstance()->removeUnusedSpriteFrames();
        _textureCache->removeUnusedTextures();

        // Note: some tests such as ActionsTest are leaking refcounted textures
        // There should be no test textures left in the cache
        log("%s\n", _textureCache->getCachedTextureInfo().c_str());
    }
    FileUtils::getInstance()->purgeCachedEntries();
}
//获取视点的位置。
float Director::getZEye(void) const
{
    return (_winSizeInPoints.height / 1.1566f);
}
//设置是否启动OpenGl的alpha通道 
void Director::setAlphaBlending(bool on)
{
    if (on)
    {
        GL::blendFunc(CC_BLEND_SRC, CC_BLEND_DST);
    }
    else
    {
        GL::blendFunc(GL_ONE, GL_ZERO);
    }

    CHECK_GL_ERROR_DEBUG();
}

void Director::setDepthTest(bool on)
{
    _renderer->setDepthTest(on);
}

void Director::setClearColor(const Color4F& clearColor)
{
    _renderer->setClearColor(clearColor);
}

static void GLToClipTransform(Mat4 *transformOut)
{
    if(nullptr == transformOut) return;
    
    Director* director = Director::getInstance();
    CCASSERT(nullptr != director, "Director is null when seting matrix stack");

    auto projection = director->getMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION);

#if CC_TARGET_PLATFORM == CC_PLATFORM_WP8
    //if needed, we need to undo the rotation for Landscape orientation in order to get the correct positions
    projection = Director::getInstance()->getOpenGLView()->getReverseOrientationMatrix() * projection;
#endif

    auto modelview = director->getMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
    *transformOut = projection * modelview;
}

Vec2 Director::convertToGL(const Vec2& uiPoint)
{
    Mat4 transform;
    GLToClipTransform(&transform);

    Mat4 transformInv = transform.getInversed();

    // Calculate z=0 using -> transform*[0, 0, 0, 1]/w
    float zClip = transform.m[14]/transform.m[15];

    Size glSize = _openGLView->getDesignResolutionSize();
    Vec4 clipCoord(2.0f*uiPoint.x/glSize.width - 1.0f, 1.0f - 2.0f*uiPoint.y/glSize.height, zClip, 1);

    Vec4 glCoord;
    //transformInv.transformPoint(clipCoord, &glCoord);
    transformInv.transformVector(clipCoord, &glCoord);
    float factor = 1.0/glCoord.w;
    return Vec2(glCoord.x * factor, glCoord.y * factor);
}

Vec2 Director::convertToUI(const Vec2& glPoint)
{
    Mat4 transform;
    GLToClipTransform(&transform);

    Vec4 clipCoord;
    // Need to calculate the zero depth from the transform.
    Vec4 glCoord(glPoint.x, glPoint.y, 0.0, 1);
    transform.transformVector(glCoord, &clipCoord);

	/*
	BUG-FIX #5506

	a = (Vx, Vy, Vz, 1)
	b = (a×M)T
	Out = 1 ⁄ bw(bx, by, bz)
	*/
	
	clipCoord.x = clipCoord.x / clipCoord.w;
	clipCoord.y = clipCoord.y / clipCoord.w;
	clipCoord.z = clipCoord.z / clipCoord.w;

    Size glSize = _openGLView->getDesignResolutionSize();
    float factor = 1.0/glCoord.w;
    return Vec2(glSize.width*(clipCoord.x*0.5 + 0.5) * factor, glSize.height*(-clipCoord.y*0.5 + 0.5) * factor);
}

const Size& Director::getWinSize(void) const
{
    return _winSizeInPoints;
}

Size Director::getWinSizeInPixels() const
{
    return Size(_winSizeInPoints.width * _contentScaleFactor, _winSizeInPoints.height * _contentScaleFactor);
}

Size Director::getVisibleSize() const
{
    if (_openGLView)
    {
        return _openGLView->getVisibleSize();
    }
    else
    {
        return Size::ZERO;
    }
}

Vec2 Director::getVisibleOrigin() const
{
    if (_openGLView)
    {
        return _openGLView->getVisibleOrigin();
    }
    else
    {
        return Vec2::ZERO;
    }
}

// scene management

void Director::runWithScene(Scene *scene)
{
    CCASSERT(scene != nullptr, "This command can only be used to start the Director. There is already a scene present.");
    CCASSERT(_runningScene == nullptr, "_runningScene should be null");

    pushScene(scene);//将当前场景压栈
    startAnimation();
}

void Director::replaceScene(Scene *scene)
{
    //CCASSERT(_runningScene, "Use runWithScene: instead to start the director");
    CCASSERT(scene != nullptr, "the scene should not be null");
    //第一次进入时，场景为空，调用runWithScene方法
    if (_runningScene == nullptr) {
        runWithScene(scene);
        return;
    }
    
    if (scene == _nextScene)
        return;
    
    if (_nextScene)
    {
        if (_nextScene->isRunning())
        {
            _nextScene->onExit();
        }
        _nextScene->cleanup();
        _nextScene = nullptr;
    }

    ssize_t index = _scenesStack.size();

    _sendCleanupToScene = true;
    _scenesStack.replace(index - 1, scene);

    _nextScene = scene;
}
//将当前场景压栈
void Director::pushScene(Scene *scene)
{
    CCASSERT(scene, "the scene should not null");

    _sendCleanupToScene = false;

    _scenesStack.pushBack(scene);
    _nextScene = scene;
}

void Director::popScene(void)
{
    CCASSERT(_runningScene != nullptr, "running scene should not null");

    _scenesStack.popBack();
    ssize_t c = _scenesStack.size();

    if (c == 0)
    {
        end();
    }
    else
    {
        _sendCleanupToScene = true;
        _nextScene = _scenesStack.at(c - 1);
    }
}

void Director::popToRootScene(void)
{
    popToSceneStackLevel(1);
}

void Director::popToSceneStackLevel(int level)
{
    CCASSERT(_runningScene != nullptr, "A running Scene is needed");
    ssize_t c = _scenesStack.size();

    // level 0? -> end
    if (level == 0)
    {
        end();
        return;
    }

    // current level or lower -> nothing
    if (level >= c)
        return;

    auto fisrtOnStackScene = _scenesStack.back();
    if (fisrtOnStackScene == _runningScene)
    {
        _scenesStack.popBack();
        --c;
    }

    // pop stack until reaching desired level
    while (c > level)
    {
        auto current = _scenesStack.back();

        if (current->isRunning())
        {
            current->onExit();
        }

        current->cleanup();
        _scenesStack.popBack();
        --c;
    }

    _nextScene = _scenesStack.back();

    // cleanup running scene
    _sendCleanupToScene = true;
}

void Director::end()
{
    _purgeDirectorInNextLoop = true;
}

void Director::restart()
{
    _restartDirectorInNextLoop = true;
}

void Director::reset()
{    
    if (_runningScene)
    {
        _runningScene->onExit();
        _runningScene->cleanup();
        _runningScene->release();
    }
    
    _runningScene = nullptr;
    _nextScene = nullptr;

    // cleanup scheduler
    getScheduler()->unscheduleAll();
    
    // Remove all events
    if (_eventDispatcher)
    {
        _eventDispatcher->removeAllEventListeners();
    }
    
    // remove all objects, but don't release it.
    // runWithScene might be executed after 'end'.
    _scenesStack.clear();
    
    stopAnimation();
    
    CC_SAFE_RELEASE_NULL(_FPSLabel);
    CC_SAFE_RELEASE_NULL(_drawnBatchesLabel);
    CC_SAFE_RELEASE_NULL(_drawnVerticesLabel);
    
    // purge bitmap cache
    FontFNT::purgeCachedData();
    
    FontFreeType::shutdownFreeType();
    
    // purge all managed caches
    
#if defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1)))
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif _MSC_VER >= 1400 //vs 2005 or higher
#pragma warning (push)
#pragma warning (disable: 4996)
#endif
//it will crash clang static analyzer so hide it if __clang_analyzer__ defined
#ifndef __clang_analyzer__
    DrawPrimitives::free();
#endif
#if defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1)))
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
#elif _MSC_VER >= 1400 //vs 2005 or higher
#pragma warning (pop)
#endif
    AnimationCache::destroyInstance();
    SpriteFrameCache::destroyInstance();
    GLProgramCache::destroyInstance();
    GLProgramStateCache::destroyInstance();
    FileUtils::destroyInstance();
    AsyncTaskPool::destoryInstance();
    
    // cocos2d-x specific data structures
    UserDefault::destroyInstance();
    
    GL::invalidateStateCache();
    
    destroyTextureCache();
}

void Director::purgeDirector()
{
    reset();

    CHECK_GL_ERROR_DEBUG();
    
    // OpenGL view
    if (_openGLView)
    {
        _openGLView->end();
        _openGLView = nullptr;
    }

    // delete Director
    release();
}

void Director::restartDirector()
{
    reset();
    
    // Texture cache need to be reinitialized
    initTextureCache();
    
    // Reschedule for action manager
    getScheduler()->scheduleUpdate(getActionManager(), Scheduler::PRIORITY_SYSTEM, false);
    
    // release the objects
    PoolManager::getInstance()->getCurrentPool()->clear();
    
    // Real restart in script level
#if CC_ENABLE_SCRIPT_BINDING
    ScriptEvent scriptEvent(kRestartGame, NULL);
    ScriptEngineManager::getInstance()->getScriptEngine()->sendEvent(&scriptEvent);
#endif
}

void Director::setNextScene()
{
    bool runningIsTransition = dynamic_cast<TransitionScene*>(_runningScene) != nullptr;
    bool newIsTransition = dynamic_cast<TransitionScene*>(_nextScene) != nullptr;

    // If it is not a transition, call onExit/cleanup
     if (! newIsTransition)
     {
         if (_runningScene)
         {
             _runningScene->onExitTransitionDidStart();
             _runningScene->onExit();
         }
 
         // issue #709. the root node (scene) should receive the cleanup message too
         // otherwise it might be leaked.
         if (_sendCleanupToScene && _runningScene)
         {
             _runningScene->cleanup();
         }
     }

    if (_runningScene)
    {
        _runningScene->release();
    }
    _runningScene = _nextScene;
    _nextScene->retain();
    _nextScene = nullptr;

    if ((! runningIsTransition) && _runningScene)
    {
        _runningScene->onEnter();
        _runningScene->onEnterTransitionDidFinish();
    }
}

void Director::pause()
{
    if (_paused)
    {
        return;
    }

    _oldAnimationInterval = _animationInterval;

    // when paused, don't consume CPU
    setAnimationInterval(1 / 4.0);
    _paused = true;
}

void Director::resume()
{
    if (! _paused)
    {
        return;
    }

    setAnimationInterval(_oldAnimationInterval);

    _paused = false;
    _deltaTime = 0;
    // fix issue #3509, skip one fps to avoid incorrect time calculation.
    setNextDeltaTimeZero(true);
}

// display the FPS using a LabelAtlas
// updates the FPS every frame
void Director::showStats()
{
    if (_isStatusLabelUpdated)
    {
        createStatsLabel();
        _isStatusLabelUpdated = false;
    }

    static unsigned long prevCalls = 0;
    static unsigned long prevVerts = 0;
    static float prevDeltaTime  = 0.016f; // 60FPS
    static const float FPS_FILTER = 0.10f;

    _accumDt += _deltaTime;
    
    if (_displayStats && _FPSLabel && _drawnBatchesLabel && _drawnVerticesLabel)
    {
        char buffer[30];

        float dt = _deltaTime * FPS_FILTER + (1-FPS_FILTER) * prevDeltaTime;
        prevDeltaTime = dt;
        _frameRate = 1/dt;

        // Probably we don't need this anymore since
        // the framerate is using a low-pass filter
        // to make the FPS stable
        if (_accumDt > CC_DIRECTOR_STATS_INTERVAL)
        {
            sprintf(buffer, "%.1f / %.3f", _frameRate, _secondsPerFrame);
            _FPSLabel->setString(buffer);
            _accumDt = 0;
        }

        auto currentCalls = (unsigned long)_renderer->getDrawnBatches();
        auto currentVerts = (unsigned long)_renderer->getDrawnVertices();
        if( currentCalls != prevCalls ) {
            sprintf(buffer, "GL calls:%6lu", currentCalls);
            _drawnBatchesLabel->setString(buffer);
            prevCalls = currentCalls;
        }

        if( currentVerts != prevVerts) {
            sprintf(buffer, "GL verts:%6lu", currentVerts);
            _drawnVerticesLabel->setString(buffer);
            prevVerts = currentVerts;
        }

        const Mat4& identity = Mat4::IDENTITY;
        _drawnVerticesLabel->visit(_renderer, identity, 0);
        _drawnBatchesLabel->visit(_renderer, identity, 0);
        _FPSLabel->visit(_renderer, identity, 0);
    }
}

void Director::calculateMPF()
{
    static float prevSecondsPerFrame = 0;
    static const float MPF_FILTER = 0.10f;

    struct timeval now;
    gettimeofday(&now, nullptr);
    
    _secondsPerFrame = (now.tv_sec - _lastUpdate->tv_sec) + (now.tv_usec - _lastUpdate->tv_usec) / 1000000.0f;

    _secondsPerFrame = _secondsPerFrame * MPF_FILTER + (1-MPF_FILTER) * prevSecondsPerFrame;
    prevSecondsPerFrame = _secondsPerFrame;
}

// returns the FPS image data pointer and len
void Director::getFPSImageData(unsigned char** datapointer, ssize_t* length)
{
    // FIXME: fixed me if it should be used 
    *datapointer = cc_fps_images_png;
    *length = cc_fps_images_len();
}

void Director::createStatsLabel()
{
    Texture2D *texture = nullptr;
    std::string fpsString = "00.0";
    std::string drawBatchString = "000";
    std::string drawVerticesString = "00000";
    if (_FPSLabel)
    {
        fpsString = _FPSLabel->getString();
        drawBatchString = _drawnBatchesLabel->getString();
        drawVerticesString = _drawnVerticesLabel->getString();
        
        CC_SAFE_RELEASE_NULL(_FPSLabel);
        CC_SAFE_RELEASE_NULL(_drawnBatchesLabel);
        CC_SAFE_RELEASE_NULL(_drawnVerticesLabel);
        _textureCache->removeTextureForKey("/cc_fps_images");
        FileUtils::getInstance()->purgeCachedEntries();
    }

    Texture2D::PixelFormat currentFormat = Texture2D::getDefaultAlphaPixelFormat();
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA4444);
    unsigned char *data = nullptr;
    ssize_t dataLength = 0;
    getFPSImageData(&data, &dataLength);

    Image* image = new (std::nothrow) Image();
    bool isOK = image->initWithImageData(data, dataLength);
    if (! isOK) {
        CCLOGERROR("%s", "Fails: init fps_images");
        return;
    }

    texture = _textureCache->addImage(image, "/cc_fps_images");
    CC_SAFE_RELEASE(image);

    /*
     We want to use an image which is stored in the file named ccFPSImage.c 
     for any design resolutions and all resource resolutions. 
     
     To achieve this, we need to ignore 'contentScaleFactor' in 'AtlasNode' and 'LabelAtlas'.
     So I added a new method called 'setIgnoreContentScaleFactor' for 'AtlasNode',
     this is not exposed to game developers, it's only used for displaying FPS now.
     */
    float scaleFactor = 1 / CC_CONTENT_SCALE_FACTOR();

    _FPSLabel = LabelAtlas::create();
    _FPSLabel->retain();
    _FPSLabel->setIgnoreContentScaleFactor(true);
    _FPSLabel->initWithString(fpsString, texture, 12, 32 , '.');
    _FPSLabel->setScale(scaleFactor);

    _drawnBatchesLabel = LabelAtlas::create();
    _drawnBatchesLabel->retain();
    _drawnBatchesLabel->setIgnoreContentScaleFactor(true);
    _drawnBatchesLabel->initWithString(drawBatchString, texture, 12, 32, '.');
    _drawnBatchesLabel->setScale(scaleFactor);

    _drawnVerticesLabel = LabelAtlas::create();
    _drawnVerticesLabel->retain();
    _drawnVerticesLabel->setIgnoreContentScaleFactor(true);
    _drawnVerticesLabel->initWithString(drawVerticesString, texture, 12, 32, '.');
    _drawnVerticesLabel->setScale(scaleFactor);


    Texture2D::setDefaultAlphaPixelFormat(currentFormat);

    const int height_spacing = 22 / CC_CONTENT_SCALE_FACTOR();
    _drawnVerticesLabel->setPosition(Vec2(0, height_spacing*2) + CC_DIRECTOR_STATS_POSITION);
    _drawnBatchesLabel->setPosition(Vec2(0, height_spacing*1) + CC_DIRECTOR_STATS_POSITION);
    _FPSLabel->setPosition(Vec2(0, height_spacing*0)+CC_DIRECTOR_STATS_POSITION);
}

void Director::setContentScaleFactor(float scaleFactor)
{
    if (scaleFactor != _contentScaleFactor)
    {
        _contentScaleFactor = scaleFactor;
        _isStatusLabelUpdated = true;
    }
}

void Director::setNotificationNode(Node *node)
{
    CC_SAFE_RELEASE(_notificationNode);
    _notificationNode = node;
    CC_SAFE_RETAIN(_notificationNode);
}

void Director::setScheduler(Scheduler* scheduler)
{
    if (_scheduler != scheduler)
    {
        CC_SAFE_RETAIN(scheduler);
        CC_SAFE_RELEASE(_scheduler);
        _scheduler = scheduler;
    }
}

void Director::setActionManager(ActionManager* actionManager)
{
    if (_actionManager != actionManager)
    {
        CC_SAFE_RETAIN(actionManager);
        CC_SAFE_RELEASE(_actionManager);
        _actionManager = actionManager;
    }    
}

void Director::setEventDispatcher(EventDispatcher* dispatcher)
{
    if (_eventDispatcher != dispatcher)
    {
        CC_SAFE_RETAIN(dispatcher);
        CC_SAFE_RELEASE(_eventDispatcher);
        _eventDispatcher = dispatcher;
    }
}

/***************************************************
* implementation of DisplayLinkDirector
**************************************************/

// should we implement 4 types of director ??
// I think DisplayLinkDirector is enough
// so we now only support DisplayLinkDirector
void DisplayLinkDirector::startAnimation()
{
    if (gettimeofday(_lastUpdate, nullptr) != 0)
    {
        CCLOG("cocos2d: DisplayLinkDirector: Error on gettimeofday");
    }

    _invalid = false;//设置为false以后在mainLoop中就开始刷新了

#ifndef WP8_SHADER_COMPILER
    Application::getInstance()->setAnimationInterval(_animationInterval);
#endif

    // fix issue #3509, skip one fps to avoid incorrect time calculation.
    setNextDeltaTimeZero(true);
}
/*
主循环中有最关键的两个函数drawScene和内存池清理，
既然我们知道了所有事件的源头来自 mainLoop()函数，
那么什么地方调用了它呢？ 这里不同的平台调用的方法不一样，
以iOS为例，在AppController.mm中有一行cocos2d::Application::getInstance()->run(); 
win32为例，在CCApplication-win32中有个while死循环方法里调用了导演类的mainloop方法。
*/
void DisplayLinkDirector::mainLoop()
{
    if (_purgeDirectorInNextLoop)//调用End方法时触发
    {
        _purgeDirectorInNextLoop = false;
        purgeDirector();
    }
    else if (_restartDirectorInNextLoop)
    {
        _restartDirectorInNextLoop = false;
        restartDirector();
    }
    else if (! _invalid)//调用StartAnimation时，设置为false，这里开启主线程绘制
    {
        drawScene();//OpenGL 主循环 绘制场景
     
        // release the objects        
        //在每一次循环中都会清理一次内存池,整个内存管理的动力所在。
        //调用clear()方法时，会调用对应池中对象的release()方法，完成引用计数的减1处理。
        //即池中每一个对象的 _referenceCount = _referenceCount - 1  
        PoolManager::getInstance()->getCurrentPool()->clear();
    }
}

void DisplayLinkDirector::stopAnimation()
{
    _invalid = true;
}

void DisplayLinkDirector::setAnimationInterval(double interval)
{
    _animationInterval = interval;
    if (! _invalid)
    {
        stopAnimation();
        startAnimation();
    }    
}

NS_CC_END

