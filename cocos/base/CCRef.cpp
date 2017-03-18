/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2013-2014 Chukong Technologies

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

#include "base/CCRef.h"
#include "base/CCAutoreleasePool.h"
#include "base/ccMacros.h"
#include "base/CCScriptSupport.h"

#if CC_REF_LEAK_DETECTION
#include <algorithm>    // std::find
#endif

NS_CC_BEGIN

#if CC_REF_LEAK_DETECTION
static void trackRef(Ref* ref);
static void untrackRef(Ref* ref);
#endif

//创建一个Ref对象时，即调用构造函数时默认的引用计数数值为1
Ref::Ref()
: _referenceCount(1) // when the Ref is created, the reference count of it is 1
{
#if CC_ENABLE_SCRIPT_BINDING
    static unsigned int uObjectCount = 0;
    _luaID = 0;
    _ID = ++uObjectCount;
    _scriptObject = nullptr;
#endif
    
#if CC_REF_LEAK_DETECTION
    trackRef(this);
#endif
}

Ref::~Ref()
{
#if CC_ENABLE_SCRIPT_BINDING
    // if the object is referenced by Lua engine, remove it
    if (_luaID)
    {
        ScriptEngineManager::getInstance()->getScriptEngine()->removeScriptObjectByObject(this);
    }
    else
    {
        ScriptEngineProtocol* pEngine = ScriptEngineManager::getInstance()->getScriptEngine();
        if (pEngine != nullptr && pEngine->getScriptType() == kScriptTypeJavascript)
        {
            pEngine->removeScriptObjectByObject(this);
        }
    }
#endif


#if CC_REF_LEAK_DETECTION
    if (_referenceCount != 0)
        untrackRef(this);
#endif
}
//调用retain方法时，默认计数数值加1。
void Ref::retain()
{
    CCASSERT(_referenceCount > 0, "reference count should be greater than 0");
    ++_referenceCount;
}
//调用release方法时，默认计数数值减1。
void Ref::release()
{
    CCASSERT(_referenceCount > 0, "reference count should be greater than 0");
    --_referenceCount;

    if (_referenceCount == 0)
    {
#if defined(COCOS2D_DEBUG) && (COCOS2D_DEBUG > 0)
        auto poolManager = PoolManager::getInstance();
        //如果对象没有被清除且对象依旧存在于对象池中，此时报个断言错误信息
        if (!poolManager->getCurrentPool()->isClearing() && poolManager->isObjectInPools(this))
        {
            // Trigger an assert if the reference count is 0 but the Ref is still in autorelease pool.
            // This happens when 'autorelease/release' were not used in pairs with 'new/retain'.
            //
            // Wrong usage (1):
            //
            // auto obj = Node::create();   // Ref = 1, but it's an autorelease Ref which means it was in the autorelease pool.
            // obj->autorelease();   // Wrong: If you wish to invoke autorelease several times, you should retain `obj` first.
            //
            // Wrong usage (2):
            //
            // auto obj = Node::create();
            // obj->release();   // Wrong: obj is an autorelease Ref, it will be released when clearing current pool.
            //
            // Correct usage (1):
            //
            // auto obj = Node::create();
            //                     |-   new Node();     // `new` is the pair of the `autorelease` of next line
            //                     |-   autorelease();  // The pair of `new Node`.
            //
            // obj->retain();
            // obj->autorelease();  // This `autorelease` is the pair of `retain` of previous line.
            //
            // Correct usage (2):
            //
            // auto obj = Node::create();
            // obj->retain();
            // obj->release();   // This `release` is the pair of `retain` of previous line.
            CCASSERT(false, "The reference shouldn't be 0 because it is still in autorelease pool.");
        }
#endif

#if CC_REF_LEAK_DETECTION
        untrackRef(this);
#endif
        //当引用计数变成0的时候，默认delete自己.
        delete this;
    }
}
//调用该方法将对象添加到内存对象的管理池中。
Ref* Ref::autorelease()
{
    PoolManager::getInstance()->getCurrentPool()->addObject(this);
    return this;
}

unsigned int Ref::getReferenceCount() const
{
    return _referenceCount;
}

#if CC_REF_LEAK_DETECTION

static std::list<Ref*> __refAllocationList;

void Ref::printLeaks()
{
    // Dump Ref object memory leaks
    if (__refAllocationList.empty())
    {
        log("[memory] All Ref objects successfully cleaned up (no leaks detected).\n");
    }
    else
    {
        log("[memory] WARNING: %d Ref objects still active in memory.\n", (int)__refAllocationList.size());

        for (const auto& ref : __refAllocationList)
        {
            CC_ASSERT(ref);
            const char* type = typeid(*ref).name();
            log("[memory] LEAK: Ref object '%s' still active with reference count %d.\n", (type ? type : ""), ref->getReferenceCount());
        }
    }
}

static void trackRef(Ref* ref)
{
    CCASSERT(ref, "Invalid parameter, ref should not be null!");

    // Create memory allocation record.
    __refAllocationList.push_back(ref);
}

static void untrackRef(Ref* ref)
{
    auto iter = std::find(__refAllocationList.begin(), __refAllocationList.end(), ref);
    if (iter == __refAllocationList.end())
    {
        log("[memory] CORRUPTION: Attempting to free (%s) with invalid ref tracking record.\n", typeid(*ref).name());
        return;
    }

    __refAllocationList.erase(iter);
}

#endif // #if CC_REF_LEAK_DETECTION


NS_CC_END
