/****************************************************************************
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


#ifndef __CCEVENT_H__
#define __CCEVENT_H__

#include "base/CCRef.h"
#include "platform/CCPlatformMacros.h"

/**
 * @addtogroup base
 * @{
 */

NS_CC_BEGIN

class Node;

/** @class Event
 * @brief Base class of all kinds of events.
   触发的事件源类型
   一个事件类型用一个Event的子类描述，它也是事件分发到订阅者时事件源传递给订阅者的参数，
   里面包含了一些处理该事件相关的信息，例如：EventAcceleration就包含了x,y,z3个方向的加速度数据。
 */
class CC_DLL Event : public Ref
{
public:
    /** Type Event type.*/
    enum class Type
    {
        TOUCH,           // 触摸事件
        KEYBOARD,        // 键盘事件
        ACCELERATION,    // 加速器事件
        MOUSE,           // 鼠标事件
        FOCUS,           // 焦点事件
        GAME_CONTROLLER, // 自定义事件
        CUSTOM
    };
    
CC_CONSTRUCTOR_ACCESS:
    /** Constructor */
    Event(Type type);
public:
    /** Destructor.
     */
    virtual ~Event();

    /** Gets the event type.
     *
     * @return The event type.
     */
	inline Type getType() const { return _type; };
    
    /** Stops propagation for current event.
     */
    inline void stopPropagation() { _isStopped = true; };
    
    /** Checks whether the event has been stopped.
     *
     isStopped定义该event是否已经停止，当一个event发生停止时，
     与其相关的Listener都要停止callback的调用；
     currentTarget是与该Event相关联的node。
     
     * @return True if the event has been stopped.
     */
    inline bool isStopped() const { return _isStopped; };
    
    /** Gets current target of the event.
     * @return The target with which the event associates.
     * @note It onlys be available when the event listener is associated with node.
     *        It returns 0 when the listener is associated with fixed priority.
     */
    inline Node* getCurrentTarget() { return _currentTarget; };
    
protected:
    /** Sets current target */
    inline void setCurrentTarget(Node* target) { _currentTarget = target; };
    
	Type _type;     ///< Event type
    
    bool _isStopped;       ///< whether the event has been stopped.
    Node* _currentTarget;  ///< Current target
    
    friend class EventDispatcher;
};

NS_CC_END

// end of base group
/// @}

#endif // __CCEVENT_H__
