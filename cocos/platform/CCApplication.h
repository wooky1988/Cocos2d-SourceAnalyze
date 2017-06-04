/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
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

应用启动时，调用Appdelegate类，其实内部调用了父类CCApplication中相应的方法
因为要实现跨平台的功能逻辑，所以在CCApplication中会以不同的平台去判定调用
哪一个平台下的CCApplication的子类，已完成对应平台下关于路径，绘制等相应平台
才有的功能，所以在CCApplication中有如下的宏定义判断以区分不同的平台。
****************************************************************************/

#ifndef __PLATFORM_CCAPPLICATION_H__
#define __PLATFORM_CCAPPLICATION_H__
/// @cond DO_NOT_SHOW

#include "platform/CCPlatformConfig.h"

#if CC_TARGET_PLATFORM == CC_PLATFORM_MAC
#include "platform/mac/CCApplication-mac.h"
#elif CC_TARGET_PLATFORM == CC_PLATFORM_IOS
#include "platform/ios/CCApplication-ios.h"
#elif CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
#include "platform/android/CCApplication-android.h"
#elif CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
#include "platform/win32/CCApplication-win32.h"
#elif CC_TARGET_PLATFORM == CC_PLATFORM_WP8
#include "platform/winrt/CCApplication.h"
#elif CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
#include "platform/linux/CCApplication-linux.h"
#endif

/// @endcond
#endif /* __PLATFORM_CCAPPLICATION_H__*/
