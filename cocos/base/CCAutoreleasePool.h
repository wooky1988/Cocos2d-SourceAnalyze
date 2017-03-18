/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2013-2014 Chukong Technologies Inc.

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
#ifndef __AUTORELEASEPOOL_H__
#define __AUTORELEASEPOOL_H__

#include <vector>
#include <string>
#include "base/CCRef.h"

/**
 * @addtogroup base
 * @{
 */
NS_CC_BEGIN


/**
   用于管理autorelease对象的池。
   AutoreleasePool中存放着被显示调用autorealse的ref，
   并且在每一帧过后调用其clear函数，显示的调用存放在其中的ref的realse函数，然后清空自身。
 * A pool for managing autorlease objects.
 * @js NA
 */
class CC_DLL AutoreleasePool
{
public:
    /**
     * @warning Don't create an autorelease pool in heap, create it in stack.
     * @js NA
     * @lua NA
     */
    AutoreleasePool();
    
    /**
     * Create an autorelease pool with specific name. This name is useful for debugging.
     * @warning Don't create an autorelease pool in heap, create it in stack.
     * @js NA
     * @lua NA
     *
     * @param name The name of created autorelease pool.
     */
    AutoreleasePool(const std::string &name);
    
    /**
     * @js NA
     * @lua NA
     */
    ~AutoreleasePool();

    /**
     * Add a given object to this autorelease pool.
     *
     同一个对象可能会被多次添加到autorelease pool中。
     当这个池被损坏时，相应池里对象的release()方法会被调用，调用次数和它被添加的
     次数一致。
     * The same object may be added several times to an autorelease pool. When the
     * pool is destructed, the object's `Ref::release()` method will be called
     * the same times as it was added.
     *
     * @param object    The object to be added into the autorelease pool.
     * @js NA
     * @lua NA
     */
    void addObject(Ref *object);

    /**
     * Clear the autorelease pool.
     * 每次主循环都会调用的函数，对于释放池中的对象，都会调用Release
     * It will invoke each element's `release()` function.
     *
     * @js NA
     * @lua NA
     */
    void clear();
    
#if defined(COCOS2D_DEBUG) && (COCOS2D_DEBUG > 0)
    /**
     * Whether the autorelease pool is doing `clear` operation.
     *
     * @return True if autorelase pool is clearning, false if not.
     *
     * @js NA
     * @lua NA
     */
    bool isClearing() const { return _isClearing; };
#endif
    
    /**
     * Checks whether the autorelease pool contains the specified object.
     *
     判断指定对象是否存在于内存池中
     * @param object The object to be checked.
     * @return True if the autorelease pool contains the object, false if not
     * @js NA
     * @lua NA
     */
    bool contains(Ref* object) const;

    /**
     * Dump the objects that are put into the autorelease pool. It is used for debugging.
     *
     * The result will look like:
     * Object pointer address     object id     reference count
     *
     * @js NA
     * @lua NA
     */
    void dump();
    
private:
    /**
     管理对象池的底层数组
     * The underlying array of object managed by the pool.
     *
     虽然对象在被添加的时候，数组对这个对象做了一次retains的操作，
     * Although Array retains the object once when an object is added, proper
     * Ref::release() is called outside the array to make sure that the pool
     * does not affect the managed object's reference count. So an object can
     * be destructed properly by calling Ref::release() even if the object
     * is in the pool.
     */
    std::vector<Ref*> _managedObjectArray;
    std::string _name;
    
#if defined(COCOS2D_DEBUG) && (COCOS2D_DEBUG > 0)
    /**
     *  The flag for checking whether the pool is doing `clear` operation.
     */
    bool _isClearing;
#endif
};

// end of base group
/** @} */

/**
 * @cond
 PoolManager管理着所有的AutoReleasePool，可能有一个或者多个AutoReleasePool.
 */
class CC_DLL PoolManager
{
public:

    CC_DEPRECATED_ATTRIBUTE static PoolManager* sharedPoolManager() { return getInstance(); }
    static PoolManager* getInstance();
    
    CC_DEPRECATED_ATTRIBUTE static void purgePoolManager() { destroyInstance(); }
    static void destroyInstance();
    
    /**
     * Get current auto release pool, there is at least one auto release pool that created by engine.
     * You can create your own auto release pool at demand, which will be put into auto releae pool stack.
       获取当前的对象的管理池，在引擎中至少有一个内存管理池，如果需要的话你也可以创建自己的内存管理池
       创建的管理池，将会被放在release pool的堆栈中。
     */
    AutoreleasePool *getCurrentPool() const;

    //判断AutoreleasePool是否有obj
    bool isObjectInPools(Ref* obj) const;


    friend class AutoreleasePool;
    
private:
    PoolManager();
    ~PoolManager();
    
    void push(AutoreleasePool *pool);
    void pop();
    
    static PoolManager* s_singleInstance;
    
    std::vector<AutoreleasePool*> _releasePoolStack;
};
/**
 * @endcond
 */

NS_CC_END

#endif //__AUTORELEASEPOOL_H__
