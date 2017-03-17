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
#include "base/CCEventDispatcher.h"
#include <algorithm>

#include "base/CCEventCustom.h"
#include "base/CCEventListenerTouch.h"
#include "base/CCEventListenerAcceleration.h"
#include "base/CCEventListenerMouse.h"
#include "base/CCEventListenerKeyboard.h"
#include "base/CCEventListenerCustom.h"
#include "base/CCEventListenerFocus.h"
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
#include "base/CCEventListenerController.h"
#endif
#include "2d/CCScene.h"
#include "base/CCDirector.h"
#include "base/CCEventType.h"


#define DUMP_LISTENER_ITEM_PRIORITY_INFO 0

namespace
{

class DispatchGuard
{
public:
    DispatchGuard(int& count):
            _count(count)
    {
        ++_count;
    }

    ~DispatchGuard()
    {
        --_count;
    }

private:
    int& _count;
};

}

NS_CC_BEGIN

static EventListener::ListenerID __getListenerID(Event* event)
{
    EventListener::ListenerID ret;
    switch (event->getType())
    {
        case Event::Type::ACCELERATION:
            ret = EventListenerAcceleration::LISTENER_ID;
            break;
        case Event::Type::CUSTOM:
            {
                auto customEvent = static_cast<EventCustom*>(event);
                ret = customEvent->getEventName();
            }
            break;
        case Event::Type::KEYBOARD:
            ret = EventListenerKeyboard::LISTENER_ID;
            break;
        case Event::Type::MOUSE:
            ret = EventListenerMouse::LISTENER_ID;
            break;
        case Event::Type::FOCUS:
            ret = EventListenerFocus::LISTENER_ID;
            break;
        case Event::Type::TOUCH:
            // Touch listener is very special, it contains two kinds of listeners, EventListenerTouchOneByOne and EventListenerTouchAllAtOnce.
            // return UNKNOWN instead.
            CCASSERT(false, "Don't call this method if the event is for touch.");
            break;
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
        case Event::Type::GAME_CONTROLLER:
            ret = EventListenerController::LISTENER_ID;
            break;
#endif
        default:
            CCASSERT(false, "Invalid type!");
            break;
    }
    
    return ret;
}

EventDispatcher::EventListenerVector::EventListenerVector() :
 _fixedListeners(nullptr),
 _sceneGraphListeners(nullptr),
 _gt0Index(0)
{
}

EventDispatcher::EventListenerVector::~EventListenerVector()
{
    CC_SAFE_DELETE(_sceneGraphListeners);
    CC_SAFE_DELETE(_fixedListeners);
}

size_t EventDispatcher::EventListenerVector::size() const
{
    //内部listener集合的大小是两个listener集合大小的总和。
    size_t ret = 0;
    if (_sceneGraphListeners)
        ret += _sceneGraphListeners->size();
    if (_fixedListeners)
        ret += _fixedListeners->size();
    
    return ret;
}

bool EventDispatcher::EventListenerVector::empty() const
{
    return (_sceneGraphListeners == nullptr || _sceneGraphListeners->empty())
        && (_fixedListeners == nullptr || _fixedListeners->empty());
}

void EventDispatcher::EventListenerVector::push_back(EventListener* listener)
{
#if CC_NODE_DEBUG_VERIFY_EVENT_LISTENERS
    CCASSERT(_sceneGraphListeners == nullptr ||
             std::count(_sceneGraphListeners->begin(), _sceneGraphListeners->end(), listener) == 0,
             "Listener should not be added twice!");
        
    CCASSERT(_fixedListeners == nullptr ||
             std::count(_fixedListeners->begin(), _fixedListeners->end(), listener) == 0,
             "Listener should not be added twice!");
#endif
    //查看listener的priority，如果为0，加入sceneGraphList，否则加入fixedList
    if (listener->getFixedPriority() == 0)
    {
        if (_sceneGraphListeners == nullptr)
        {
            _sceneGraphListeners = new std::vector<EventListener*>();
            _sceneGraphListeners->reserve(100);
        }
        
        _sceneGraphListeners->push_back(listener);
    }
    else
    {
        if (_fixedListeners == nullptr)
        {
            _fixedListeners = new std::vector<EventListener*>();
            _fixedListeners->reserve(100);
        }
        
        _fixedListeners->push_back(listener);
    }
}

void EventDispatcher::EventListenerVector::clearSceneGraphListeners()
{
    if (_sceneGraphListeners)
    {
        _sceneGraphListeners->clear();
        delete _sceneGraphListeners;
        _sceneGraphListeners = nullptr;
    }
}

void EventDispatcher::EventListenerVector::clearFixedListeners()
{
    if (_fixedListeners)
    {
        _fixedListeners->clear();
        delete _fixedListeners;
        _fixedListeners = nullptr;
    }
}

void EventDispatcher::EventListenerVector::clear()
{
    clearSceneGraphListeners();
    clearFixedListeners();
}


EventDispatcher::EventDispatcher()
: _inDispatch(0)
, _isEnabled(false)
, _nodePriorityIndex(0)
{
    _toAddedListeners.reserve(50);
    
    // fixed #4129: Mark the following listener IDs for internal use.
    // Therefore, internal listeners would not be cleaned when removeAllEventListeners is invoked.
    _internalCustomListenerIDs.insert(EVENT_COME_TO_FOREGROUND);
    _internalCustomListenerIDs.insert(EVENT_COME_TO_BACKGROUND);
    _internalCustomListenerIDs.insert(EVENT_RENDERER_RECREATED);
}

EventDispatcher::~EventDispatcher()
{
    // Clear internal custom listener IDs from set,
    // so removeAllEventListeners would clean internal custom listeners.
    _internalCustomListenerIDs.clear();
    removeAllEventListeners();
}

void EventDispatcher::visitTarget(Node* node, bool isRootNode)
{    
    int i = 0;
    auto& children = node->getChildren();
    
    auto childrenCount = children.size();
    
    if(childrenCount > 0)
    {
        Node* child = nullptr;
        // visit children zOrder < 0
        for( ; i < childrenCount; i++ )
        {
            child = children.at(i);
            
            if ( child && child->getLocalZOrder() < 0 )
                visitTarget(child, false);
            else
                break;
        }
        
        if (_nodeListenersMap.find(node) != _nodeListenersMap.end())
        {
            _globalZOrderNodeMap[node->getGlobalZOrder()].push_back(node);
        }
        
        for( ; i < childrenCount; i++ )
        {
            child = children.at(i);
            if (child)
                visitTarget(child, false);
        }
    }
    else
    {
        if (_nodeListenersMap.find(node) != _nodeListenersMap.end())
        {
            _globalZOrderNodeMap[node->getGlobalZOrder()].push_back(node);
        }
    }
    
    if (isRootNode)
    {
        std::vector<float> globalZOrders;
        globalZOrders.reserve(_globalZOrderNodeMap.size());
        
        for (const auto& e : _globalZOrderNodeMap)
        {
            globalZOrders.push_back(e.first);
        }
        
        std::sort(globalZOrders.begin(), globalZOrders.end(), [](const float a, const float b){
            return a < b;
        });
        
        for (const auto& globalZ : globalZOrders)
        {
            for (const auto& n : _globalZOrderNodeMap[globalZ])
            {
                _nodePriorityMap[n] = ++_nodePriorityIndex;
            }
        }
        
        _globalZOrderNodeMap.clear();
    }
}

void EventDispatcher::pauseEventListenersForTarget(Node* target, bool recursive/* = false */)
{
    auto listenerIter = _nodeListenersMap.find(target);
    if (listenerIter != _nodeListenersMap.end())
    {
        auto listeners = listenerIter->second;
        for (auto& l : *listeners)
        {
            l->setPaused(true);
        }
    }

    for (auto& listener : _toAddedListeners)
    {
        if (listener->getAssociatedNode() == target)
        {
            listener->setPaused(true);
        }
    }
    
    if (recursive)
    {
        const auto& children = target->getChildren();
        for (const auto& child : children)
        {
            pauseEventListenersForTarget(child, true);
        }
    }
}

void EventDispatcher::resumeEventListenersForTarget(Node* target, bool recursive/* = false */)
{
    auto listenerIter = _nodeListenersMap.find(target);
    if (listenerIter != _nodeListenersMap.end())
    {
        auto listeners = listenerIter->second;
        for (auto& l : *listeners)
        {
            //恢复Node的运行状态 
            l->setPaused(false);
        }
    }

    // toAdd List中也要进行恢复  
    for (auto& listener : _toAddedListeners)
    {
        if (listener->getAssociatedNode() == target)
        {
            listener->setPaused(false);
        }
    }
    //将该Node 与 node的child 都放到dirtyNode中，来记录与event相关的node
    setDirtyForNode(target);
    
    if (recursive)
    {
        const auto& children = target->getChildren();
        for (const auto& child : children)
        {
            resumeEventListenersForTarget(child, true);
        }
    }
}

void EventDispatcher::removeEventListenersForTarget(Node* target, bool recursive/* = false */)
{
    // Ensure the node is removed from these immediately also.
    // Don't want any dangling pointers or the possibility of dealing with deleted objects..
    _nodePriorityMap.erase(target);
    _dirtyNodes.erase(target);

    auto listenerIter = _nodeListenersMap.find(target);
    if (listenerIter != _nodeListenersMap.end())
    {
        auto listeners = listenerIter->second;
        auto listenersCopy = *listeners;
        for (auto& l : listenersCopy)
        {
            removeEventListener(l);
        }
    }
    
    // Bug fix: ensure there are no references to the node in the list of listeners to be added.
    // If we find any listeners associated with the destroyed node in this list then remove them.
    // This is to catch the scenario where the node gets destroyed before it's listener
    // is added into the event dispatcher fully. This could happen if a node registers a listener
    // and gets destroyed while we are dispatching an event (touch etc.)
    for (auto iter = _toAddedListeners.begin(); iter != _toAddedListeners.end(); )
    {
        EventListener * listener = *iter;
            
        if (listener->getAssociatedNode() == target)
        {
            listener->setAssociatedNode(nullptr);   // Ensure no dangling ptr to the target node.
            listener->setRegistered(false);
            listener->release();
            iter = _toAddedListeners.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    
    if (recursive)
    {
        const auto& children = target->getChildren();
        for (const auto& child : children)
        {
            removeEventListenersForTarget(child, true);
        }
    }
}

void EventDispatcher::associateNodeAndEventListener(Node* node, EventListener* listener)
{
    std::vector<EventListener*>* listeners = nullptr;
    auto found = _nodeListenersMap.find(node);
    if (found != _nodeListenersMap.end())
    {
        listeners = found->second;
    }
    else
    {
        listeners = new std::vector<EventListener*>();
        _nodeListenersMap.insert(std::make_pair(node, listeners));
    }
    
    listeners->push_back(listener);
}

void EventDispatcher::dissociateNodeAndEventListener(Node* node, EventListener* listener)
{
    std::vector<EventListener*>* listeners = nullptr;
    auto found = _nodeListenersMap.find(node);
    if (found != _nodeListenersMap.end())
    {
        listeners = found->second;
        auto iter = std::find(listeners->begin(), listeners->end(), listener);
        if (iter != listeners->end())
        {
            listeners->erase(iter);
        }
        
        if (listeners->empty())
        {
            _nodeListenersMap.erase(found);
            delete listeners;
        }
    }
}

void EventDispatcher::addEventListener(EventListener* listener)
{
    //如果当前Dispatcher正在进行事件Dispatch，则放到toAddList中。
    if (_inDispatch == 0)
    {
        forceAddEventListener(listener);
    }
    else
    {
        _toAddedListeners.push_back(listener);
    }

    listener->retain();
}

void EventDispatcher::forceAddEventListener(EventListener* listener)
{
    EventListenerVector* listeners = nullptr;
    EventListener::ListenerID listenerID = listener->getListenerID();
    //2017-03-17
    //找到该类eventlistener的vector，此处的vector是EventVector,还未进行优先级排序的vector。 
    auto itr = _listenerMap.find(listenerID);
    //如果没有找到，则需要向map中添加一个pair
    if (itr == _listenerMap.end())
    {
        
        listeners = new (std::nothrow) EventListenerVector();
        _listenerMap.insert(std::make_pair(listenerID, listeners));
    }
    else
    {
        listeners = itr->second;
    }
    //将该类别listenerpush_back进去（这个函数调用的是EventVector的pushback方法） 
    listeners->push_back(listener);

     //如果优先级是0，则设置为graph。
    if (listener->getFixedPriority() == 0)
    {   
        //设置该listenerID的DirtyFlag  
        //(setDirty函数可以这样理解，每个ListenerID都有特定的dirtyFlag，
        //每次进行add操作后，都要更新该ID的flag)
        setDirty(listenerID, DirtyFlag::SCENE_GRAPH_PRIORITY);
        
        //如果是sceneGraph类的事件，则需要处理两个方面：  
        //1.将node 与event 关联  
        //2.如果该node是运行中的，则需要恢复其事件（因为默认的sceneGraph listener的状态时pause）  
        //增加该listener与node的关联  
        auto node = listener->getAssociatedNode();
        CCASSERT(node != nullptr, "Invalid scene graph priority!");
        
        associateNodeAndEventListener(node, listener);
        
        if (node->isRunning())
        {
            resumeEventListenersForTarget(node);
        }
    }
    else
    {
        setDirty(listenerID, DirtyFlag::FIXED_PRIORITY);
    }
}

void EventDispatcher::addEventListenerWithSceneGraphPriority(EventListener* listener, Node* node)
{
    CCASSERT(listener && node, "Invalid parameters.");
    CCASSERT(!listener->isRegistered(), "The listener has been registered.");
    
    if (!listener->checkAvailable())
        return;
    
    listener->setAssociatedNode(node);
    listener->setFixedPriority(0);
    listener->setRegistered(true);//标记该listener已经被注册进EventDispatcher
    
    addEventListener(listener);
}

#if CC_NODE_DEBUG_VERIFY_EVENT_LISTENERS && COCOS2D_DEBUG > 0

void EventDispatcher::debugCheckNodeHasNoEventListenersOnDestruction(Node* node)
{
    // Check the listeners map
    for (const auto & keyValuePair : _listenerMap)
    {
        const EventListenerVector * eventListenerVector = keyValuePair.second;
        
        if (eventListenerVector)
        {
            if (eventListenerVector->getSceneGraphPriorityListeners())
            {
                for (EventListener * listener : *eventListenerVector->getSceneGraphPriorityListeners())
                {
                    CCASSERT(!listener ||
                             listener->getAssociatedNode() != node,
                             "Node should have no event listeners registered for it upon destruction!");
                }
            }
        }
    }
    
    // Check the node listeners map
    for (const auto & keyValuePair : _nodeListenersMap)
    {
        CCASSERT(keyValuePair.first != node, "Node should have no event listeners registered for it upon destruction!");
        
        if (keyValuePair.second)
        {
            for (EventListener * listener : *keyValuePair.second)
            {
                CCASSERT(listener->getAssociatedNode() != node,
                         "Node should have no event listeners registered for it upon destruction!");
            }
        }
    }
    
    // Check the node priority map
    for (const auto & keyValuePair : _nodePriorityMap)
    {
        CCASSERT(keyValuePair.first != node,
                 "Node should have no event listeners registered for it upon destruction!");
    }
    
    // Check the to be added list
    for (EventListener * listener : _toAddedListeners)
    {
        CCASSERT(listener->getAssociatedNode() != node,
                 "Node should have no event listeners registered for it upon destruction!");
    }
    
    // Check the dirty nodes set
    for (Node * dirtyNode : _dirtyNodes)
    {
        CCASSERT(dirtyNode != node,
                 "Node should have no event listeners registered for it upon destruction!");
    }
}

#endif  // #if CC_NODE_DEBUG_VERIFY_EVENT_LISTENERS && COCOS2D_DEBUG > 0


void EventDispatcher::addEventListenerWithFixedPriority(EventListener* listener, int fixedPriority)
{
    CCASSERT(listener, "Invalid parameters.");
    //一个事件只能被注册一次
    CCASSERT(!listener->isRegistered(), "The listener has been registered.");
    //Fixed类型的事件优先级不能是0
    CCASSERT(fixedPriority != 0, "0 priority is forbidden for fixed priority since it's used for scene graph based priority.");
    
    //检测监听事件是否可用
    if (!listener->checkAvailable())
        return;
    //设定相关属性
    listener->setAssociatedNode(nullptr);
    listener->setFixedPriority(fixedPriority);
    listener->setRegistered(true);
    listener->setPaused(false);

    addEventListener(listener);
}

EventListenerCustom* EventDispatcher::addCustomEventListener(const std::string &eventName, const std::function<void(EventCustom*)>& callback)
{
    //自定义的事件，唯一标示使用string字符串的方式。
    EventListenerCustom *listener = EventListenerCustom::create(eventName, callback);
    //默认自定义事件的优先级是1
    addEventListenerWithFixedPriority(listener, 1);
    return listener;
}

void EventDispatcher::removeEventListener(EventListener* listener)
{
    if (listener == nullptr)
        return;

    bool isFound = false;
    
    auto removeListenerInVector = [&](std::vector<EventListener*>* listeners){
        if (listeners == nullptr)
            return;
        
        for (auto iter = listeners->begin(); iter != listeners->end(); ++iter)
        {
            auto l = *iter;
            if (l == listener)
            {
                CC_SAFE_RETAIN(l);
                //找到后的处理方法，标记状态位，并处理关联Node 
                l->setRegistered(false);
                if (l->getAssociatedNode() != nullptr)
                {
                    dissociateNodeAndEventListener(l->getAssociatedNode(), l);
                    l->setAssociatedNode(nullptr);  // nullptr out the node pointer so we don't have any dangling pointers to destroyed nodes.
                }
                 //当前没有在分发事件 
                 //则直接从listeners中移除该listener（因为标记了状态未，如果此时在分发事件，则会等结束后再移除）
                if (_inDispatch == 0)
                {
                    listeners->erase(iter);
                    CC_SAFE_RELEASE(l);
                }
                
                isFound = true;
                break;
            }
        }
    };
    //从listenersmap 中遍历所有，拿出所有的vector 
    for (auto iter = _listenerMap.begin(); iter != _listenerMap.end();)
    {
        auto listeners = iter->second;
        auto fixedPriorityListeners = listeners->getFixedPriorityListeners();
        auto sceneGraphPriorityListeners = listeners->getSceneGraphPriorityListeners();
        //从graphList中寻找。找到后需要更新该listenerID的dirty flag。  
        removeListenerInVector(sceneGraphPriorityListeners);
        if (isFound)
        {
            // fixed #4160: Dirty flag need to be updated after listeners were removed.
            setDirty(listener->getListenerID(), DirtyFlag::SCENE_GRAPH_PRIORITY);
        }
        else
        {
            removeListenerInVector(fixedPriorityListeners);
            if (isFound)
            {
                setDirty(listener->getListenerID(), DirtyFlag::FIXED_PRIORITY);
            }
        }
        
#if CC_NODE_DEBUG_VERIFY_EVENT_LISTENERS
        CCASSERT(_inDispatch != 0 ||
                 !sceneGraphPriorityListeners ||
                 std::count(sceneGraphPriorityListeners->begin(), sceneGraphPriorityListeners->end(), listener) == 0,
                 "Listener should be in no lists after this is done if we're not currently in dispatch mode.");
            
        CCASSERT(_inDispatch != 0 ||
                 !fixedPriorityListeners ||
                 std::count(fixedPriorityListeners->begin(), fixedPriorityListeners->end(), listener) == 0,
                 "Listener should be in no lists after this is done if we're not currently in dispatch mode.");
#endif
        //如果vector在删除后是空的，则需要移除该vector，
        //并且将相应的listenerID从_priorityDirtyFlagMap中移除。
        if (iter->second->empty())
        {
            _priorityDirtyFlagMap.erase(listener->getListenerID());
            auto list = iter->second;
            iter = _listenerMap.erase(iter);
            CC_SAFE_DELETE(list);
        }
        else
        {
            ++iter;
        }
        
        if (isFound)
            break;
    }

    if (isFound)
    {
        CC_SAFE_RELEASE(listener);
    }
    else
    {
        for(auto iter = _toAddedListeners.begin(); iter != _toAddedListeners.end(); ++iter)
        {
            if (*iter == listener)
            {
                listener->setRegistered(false);
                listener->release();
                _toAddedListeners.erase(iter);
                break;
            }
        }
    }
}

void EventDispatcher::setPriority(EventListener* listener, int fixedPriority)
{
    if (listener == nullptr)
        return;
    
    for (auto iter = _listenerMap.begin(); iter != _listenerMap.end(); ++iter)
    {
        auto fixedPriorityListeners = iter->second->getFixedPriorityListeners();
        if (fixedPriorityListeners)
        {
            auto found = std::find(fixedPriorityListeners->begin(), fixedPriorityListeners->end(), listener);
            if (found != fixedPriorityListeners->end())
            {
                CCASSERT(listener->getAssociatedNode() == nullptr, "Can't set fixed priority with scene graph based listener.");
                
                if (listener->getFixedPriority() != fixedPriority)
                {
                    listener->setFixedPriority(fixedPriority);
                    setDirty(listener->getListenerID(), DirtyFlag::FIXED_PRIORITY);
                }
                return;
            }
        }
    }
}

void EventDispatcher::dispatchEventToListeners(EventListenerVector* listeners, const std::function<bool(EventListener*)>& onEvent)
{
    bool shouldStopPropagation = false;
    auto fixedPriorityListeners = listeners->getFixedPriorityListeners();
    auto sceneGraphPriorityListeners = listeners->getSceneGraphPriorityListeners();
    
     //整体操作流程分为三个部分，处理优先级<0，=0,>0三个部分  
    ssize_t i = 0;
    // priority < 0
    if (fixedPriorityListeners)
    {
        CCASSERT(listeners->getGt0Index() <= static_cast<ssize_t>(fixedPriorityListeners->size()), "Out of range exception!");
        
        if (!fixedPriorityListeners->empty())
        {
            for (; i < listeners->getGt0Index(); ++i)
            {
                auto l = fixedPriorityListeners->at(i);
                // onEvent（l）的操作调用了event的callBack，并且会返回是否停止，如果停止后，则将shouldStopPropagation标记为true  
                //在其后面的listeners则不会响应到该事件（这里可以看出触摸事件是如何被吞噬的）
                if (l->isEnabled() && !l->isPaused() && l->isRegistered() && onEvent(l))
                {
                    shouldStopPropagation = true;
                    break;
                }
            }
        }
    }
    
    if (sceneGraphPriorityListeners)
    {
        if (!shouldStopPropagation)
        {
            // priority == 0, scene graph priority
            for (auto& l : *sceneGraphPriorityListeners)
            {
                if (l->isEnabled() && !l->isPaused() && l->isRegistered() && onEvent(l))
                {
                    shouldStopPropagation = true;
                    break;
                }
            }
        }
    }

    if (fixedPriorityListeners)
    {
        if (!shouldStopPropagation)
        {
            // priority > 0
            ssize_t size = fixedPriorityListeners->size();
            for (; i < size; ++i)
            {
                auto l = fixedPriorityListeners->at(i);
                // onEvent（l）的操作调用了event的callBack，并且会返回是否停止，如果停止后，则将shouldStopPropagation标记为true  
                //在其后面的listeners则不会响应到该事件（这里可以看出触摸事件是如何被吞噬的）
                if (l->isEnabled() && !l->isPaused() && l->isRegistered() && onEvent(l))
                {
                    shouldStopPropagation = true;
                    break;
                }
            }
        }
    }
}

void EventDispatcher::dispatchEvent(Event* event)
{
    if (!_isEnabled)
        return;
    //为dirtyNodesVector中的dirtyNode更新Scene Flag。 
    updateDirtyFlagForSceneGraph();
    
    
    DispatchGuard guard(_inDispatch);
    //特殊touch事件，转到特殊的touch事件处理 
    if (event->getType() == Event::Type::TOUCH)
    {
        dispatchTouchEvent(static_cast<EventTouch*>(event));
        return;
    }

    //根据事件的类型，获取事件的ID  
    auto listenerID = __getListenerID(event);
    
     //根据事件ID，将该类事件进行排序（先响应谁）
    sortEventListeners(listenerID);
    
    auto iter = _listenerMap.find(listenerID);
    if (iter != _listenerMap.end())
    {
        auto listeners = iter->second;
        //该类事件的lambda函数 
        auto onEvent = [&event](EventListener* listener) -> bool{
            //设置event的target
            event->setCurrentTarget(listener->getAssociatedNode());
            //调用响应函数
            listener->_onEvent(event);
            //返回是否已经停止
            return event->isStopped();
        };
        //将该类事件的listeners 和 该类事件的 lambda函数传给该函数  
        dispatchEventToListeners(listeners, onEvent);
    }
    //更新该事件相关的listener 
    updateListeners(event);
}

void EventDispatcher::dispatchCustomEvent(const std::string &eventName, void *optionalUserData)
{
    EventCustom ev(eventName);
    ev.setUserData(optionalUserData);
    dispatchEvent(&ev);
}

//结合以下的dispatchEventToListeners的源码分析，可以看出新版本的OneByOne touch机制是这样的：  
//1.listener根据Node的优先级排序后，依次响应。值得注意的是，新版本的优先级是根据Node的global Zorder来的，而不是2.x的触摸优先级。  
//2.当TouchEvent Began来了之后，所有的listener会依次影响Touch Began。然后再依次响应Touch Move...而不是一个listener响应完  
//began move end之后 轮到下一个listener响应的顺序。  
//3.吞噬操作只有发生在began return true后才可以发生 
void EventDispatcher::dispatchTouchEvent(EventTouch* event)
{
    //先将EventListeners排序
    sortEventListeners(EventListenerTouchOneByOne::LISTENER_ID);
    sortEventListeners(EventListenerTouchAllAtOnce::LISTENER_ID);
    
    auto oneByOneListeners = getListeners(EventListenerTouchOneByOne::LISTENER_ID);
    auto allAtOnceListeners = getListeners(EventListenerTouchAllAtOnce::LISTENER_ID);
    
    // If there aren't any touch listeners, return directly.
    if (nullptr == oneByOneListeners && nullptr == allAtOnceListeners)
        return;
    
    bool isNeedsMutableSet = (oneByOneListeners && allAtOnceListeners);
    
    const std::vector<Touch*>& originalTouches = event->getTouches();
    std::vector<Touch*> mutableTouches(originalTouches.size());
    std::copy(originalTouches.begin(), originalTouches.end(), mutableTouches.begin());

    //
    // process the target handlers 1st
    //
    if (oneByOneListeners)
    {
        auto mutableTouchesIter = mutableTouches.begin();
        auto touchesIter = originalTouches.begin();
        
        for (; touchesIter != originalTouches.end(); ++touchesIter)
        {
            bool isSwallowed = false;

            auto onTouchEvent = [&](EventListener* l) -> bool { // Return true to break
                EventListenerTouchOneByOne* listener = static_cast<EventListenerTouchOneByOne*>(l);
                
                // Skip if the listener was removed.
                if (!listener->_isRegistered)
                    return false;
             
                event->setCurrentTarget(listener->_node);
                
                bool isClaimed = false;
                std::vector<Touch*>::iterator removedIter;
                
                EventTouch::EventCode eventCode = event->getEventCode();
                
                if (eventCode == EventTouch::EventCode::BEGAN)
                {
                    if (listener->onTouchBegan)
                    {
                        isClaimed = listener->onTouchBegan(*touchesIter, event);
                        if (isClaimed && listener->_isRegistered)
                        {
                            listener->_claimedTouches.push_back(*touchesIter);
                        }
                    }
                }
                else if (listener->_claimedTouches.size() > 0
                         && ((removedIter = std::find(listener->_claimedTouches.begin(), listener->_claimedTouches.end(), *touchesIter)) != listener->_claimedTouches.end()))
                {
                    isClaimed = true;
                    
                    switch (eventCode)
                    {
                        case EventTouch::EventCode::MOVED:
                            if (listener->onTouchMoved)
                            {
                                listener->onTouchMoved(*touchesIter, event);
                            }
                            break;
                        case EventTouch::EventCode::ENDED:
                            if (listener->onTouchEnded)
                            {
                                listener->onTouchEnded(*touchesIter, event);
                            }
                            if (listener->_isRegistered)
                            {
                                listener->_claimedTouches.erase(removedIter);
                            }
                            break;
                        case EventTouch::EventCode::CANCELLED:
                            if (listener->onTouchCancelled)
                            {
                                listener->onTouchCancelled(*touchesIter, event);
                            }
                            if (listener->_isRegistered)
                            {
                                listener->_claimedTouches.erase(removedIter);
                            }
                            break;
                        default:
                            CCASSERT(false, "The eventcode is invalid.");
                            break;
                    }
                }
                
                // If the event was stopped, return directly.
                if (event->isStopped())
                {
                    updateListeners(event);
                    return true;
                }
                
                CCASSERT((*touchesIter)->getID() == (*mutableTouchesIter)->getID(), "");
                
                if (isClaimed && listener->_isRegistered && listener->_needSwallow)
                {
                    if (isNeedsMutableSet)
                    {
                        mutableTouchesIter = mutableTouches.erase(mutableTouchesIter);
                        isSwallowed = true;
                    }
                    return true;
                }
                
                return false;
            };
            
            //
            dispatchEventToListeners(oneByOneListeners, onTouchEvent);
            if (event->isStopped())
            {
                return;
            }
            
            if (!isSwallowed)
                ++mutableTouchesIter;
        }
    }
    
    //
    // process standard handlers 2nd
    //
    //值得注意的是被吞噬的touch也不会被AllAtOnce响应到
    if (allAtOnceListeners && mutableTouches.size() > 0)
    {
        
        auto onTouchesEvent = [&](EventListener* l) -> bool{
            EventListenerTouchAllAtOnce* listener = static_cast<EventListenerTouchAllAtOnce*>(l);
            // Skip if the listener was removed.
            if (!listener->_isRegistered)
                return false;
            
            event->setCurrentTarget(listener->_node);
            
            switch (event->getEventCode())
            {
                case EventTouch::EventCode::BEGAN:
                    if (listener->onTouchesBegan)
                    {
                        listener->onTouchesBegan(mutableTouches, event);
                    }
                    break;
                case EventTouch::EventCode::MOVED:
                    if (listener->onTouchesMoved)
                    {
                        listener->onTouchesMoved(mutableTouches, event);
                    }
                    break;
                case EventTouch::EventCode::ENDED:
                    if (listener->onTouchesEnded)
                    {
                        listener->onTouchesEnded(mutableTouches, event);
                    }
                    break;
                case EventTouch::EventCode::CANCELLED:
                    if (listener->onTouchesCancelled)
                    {
                        listener->onTouchesCancelled(mutableTouches, event);
                    }
                    break;
                default:
                    CCASSERT(false, "The eventcode is invalid.");
                    break;
            }
            
            // If the event was stopped, return directly.
            if (event->isStopped())
            {
                updateListeners(event);
                return true;
            }
            
            return false;
        };
        
        dispatchEventToListeners(allAtOnceListeners, onTouchesEvent);
        if (event->isStopped())
        {
            return;
        }
    }
    
    updateListeners(event);
}

void EventDispatcher::updateListeners(Event* event)
{
    CCASSERT(_inDispatch > 0, "If program goes here, there should be event in dispatch.");

    if (_inDispatch > 1)
        return;

    auto onUpdateListeners = [this](const EventListener::ListenerID& listenerID)
    {
        auto listenersIter = _listenerMap.find(listenerID);
        if (listenersIter == _listenerMap.end())
            return;

        auto listeners = listenersIter->second;
        
        auto fixedPriorityListeners = listeners->getFixedPriorityListeners();
        auto sceneGraphPriorityListeners = listeners->getSceneGraphPriorityListeners();
        
        if (sceneGraphPriorityListeners)
        {
            for (auto iter = sceneGraphPriorityListeners->begin(); iter != sceneGraphPriorityListeners->end();)
            {
                auto l = *iter;
                if (!l->isRegistered())
                {
                    iter = sceneGraphPriorityListeners->erase(iter);
                    l->release();
                }
                else
                {
                    ++iter;
                }
            }
        }
        
        if (fixedPriorityListeners)
        {
            for (auto iter = fixedPriorityListeners->begin(); iter != fixedPriorityListeners->end();)
            {
                auto l = *iter;
                if (!l->isRegistered())
                {
                    iter = fixedPriorityListeners->erase(iter);
                    l->release();
                }
                else
                {
                    ++iter;
                }
            }
        }
        
        if (sceneGraphPriorityListeners && sceneGraphPriorityListeners->empty())
        {
            listeners->clearSceneGraphListeners();
        }

        if (fixedPriorityListeners && fixedPriorityListeners->empty())
        {
            listeners->clearFixedListeners();
        }
    };

    if (event->getType() == Event::Type::TOUCH)
    {
        onUpdateListeners(EventListenerTouchOneByOne::LISTENER_ID);
        onUpdateListeners(EventListenerTouchAllAtOnce::LISTENER_ID);
    }
    else
    {
        onUpdateListeners(__getListenerID(event));
    }
    
    CCASSERT(_inDispatch == 1, "_inDispatch should be 1 here.");
    
    for (auto iter = _listenerMap.begin(); iter != _listenerMap.end();)
    {
        if (iter->second->empty())
        {
            _priorityDirtyFlagMap.erase(iter->first);
            delete iter->second;
            iter = _listenerMap.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    
    if (!_toAddedListeners.empty())
    {
        for (auto& listener : _toAddedListeners)
        {
            forceAddEventListener(listener);
        }
        _toAddedListeners.clear();
    }
}

void EventDispatcher::updateDirtyFlagForSceneGraph()
{
    if (!_dirtyNodes.empty())
    {
        for (auto& node : _dirtyNodes)
        {
            auto iter = _nodeListenersMap.find(node);
            if (iter != _nodeListenersMap.end())
            {
                for (auto& l : *iter->second)
                {
                    setDirty(l->getListenerID(), DirtyFlag::SCENE_GRAPH_PRIORITY);
                }
            }
        }
        
        _dirtyNodes.clear();
    }
}

void EventDispatcher::sortEventListeners(const EventListener::ListenerID& listenerID)
{
    DirtyFlag dirtyFlag = DirtyFlag::NONE;
    
    auto dirtyIter = _priorityDirtyFlagMap.find(listenerID);
    if (dirtyIter != _priorityDirtyFlagMap.end())
    {
        dirtyFlag = dirtyIter->second;
    }
    
    if (dirtyFlag != DirtyFlag::NONE)
    {
        // Clear the dirty flag first, if `rootNode` is nullptr, then set its dirty flag of scene graph priority
        dirtyIter->second = DirtyFlag::NONE;

        if ((int)dirtyFlag & (int)DirtyFlag::FIXED_PRIORITY)
        {
            sortEventListenersOfFixedPriority(listenerID);
        }
        
        if ((int)dirtyFlag & (int)DirtyFlag::SCENE_GRAPH_PRIORITY)
        {
            auto rootNode = Director::getInstance()->getRunningScene();
            if (rootNode)
            {
                sortEventListenersOfSceneGraphPriority(listenerID, rootNode);
            }
            else
            {
                dirtyIter->second = DirtyFlag::SCENE_GRAPH_PRIORITY;
            }
        }
    }
}

void EventDispatcher::sortEventListenersOfSceneGraphPriority(const EventListener::ListenerID& listenerID, Node* rootNode)
{
    auto listeners = getListeners(listenerID);
    
    if (listeners == nullptr)
        return;
    auto sceneGraphListeners = listeners->getSceneGraphPriorityListeners();
    
    if (sceneGraphListeners == nullptr)
        return;

    // Reset priority index
    _nodePriorityIndex = 0;
    _nodePriorityMap.clear();

    visitTarget(rootNode, true);
    
    // After sort: priority < 0, > 0
    std::sort(sceneGraphListeners->begin(), sceneGraphListeners->end(), [this](const EventListener* l1, const EventListener* l2) {
        return _nodePriorityMap[l1->getAssociatedNode()] > _nodePriorityMap[l2->getAssociatedNode()];
    });
    
#if DUMP_LISTENER_ITEM_PRIORITY_INFO
    log("-----------------------------------");
    for (auto& l : *sceneGraphListeners)
    {
        log("listener priority: node ([%s]%p), priority (%d)", typeid(*l->_node).name(), l->_node, _nodePriorityMap[l->_node]);
    }
#endif
}

void EventDispatcher::sortEventListenersOfFixedPriority(const EventListener::ListenerID& listenerID)
{
    auto listeners = getListeners(listenerID);

    if (listeners == nullptr)
        return;
    
    auto fixedListeners = listeners->getFixedPriorityListeners();
    if (fixedListeners == nullptr)
        return;
    
    // After sort: priority < 0, > 0
    std::sort(fixedListeners->begin(), fixedListeners->end(), [](const EventListener* l1, const EventListener* l2) {
        return l1->getFixedPriority() < l2->getFixedPriority();
    });
    
    // FIXME: Should use binary search
    int index = 0;
    for (auto& listener : *fixedListeners)
    {
        if (listener->getFixedPriority() >= 0)
            break;
        ++index;
    }
    
    listeners->setGt0Index(index);
    
#if DUMP_LISTENER_ITEM_PRIORITY_INFO
    log("-----------------------------------");
    for (auto& l : *fixedListeners)
    {
        log("listener priority: node (%p), fixed (%d)", l->_node, l->_fixedPriority);
    }    
#endif
    
}

EventDispatcher::EventListenerVector* EventDispatcher::getListeners(const EventListener::ListenerID& listenerID)
{
    auto iter = _listenerMap.find(listenerID);
    if (iter != _listenerMap.end())
    {
        return iter->second;
    }
    
    return nullptr;
}

void EventDispatcher::removeEventListenersForListenerID(const EventListener::ListenerID& listenerID)
{
    auto listenerItemIter = _listenerMap.find(listenerID);
    if (listenerItemIter != _listenerMap.end())
    {
        auto listeners = listenerItemIter->second;
        auto fixedPriorityListeners = listeners->getFixedPriorityListeners();
        auto sceneGraphPriorityListeners = listeners->getSceneGraphPriorityListeners();
        
        auto removeAllListenersInVector = [&](std::vector<EventListener*>* listenerVector){
            if (listenerVector == nullptr)
                return;
            
            for (auto iter = listenerVector->begin(); iter != listenerVector->end();)
            {
                auto l = *iter;
                l->setRegistered(false);
                if (l->getAssociatedNode() != nullptr)
                {
                    dissociateNodeAndEventListener(l->getAssociatedNode(), l);
                    l->setAssociatedNode(nullptr);  // nullptr out the node pointer so we don't have any dangling pointers to destroyed nodes.
                }
                
                if (_inDispatch == 0)
                {
                    iter = listenerVector->erase(iter);
                    CC_SAFE_RELEASE(l);
                }
                else
                {
                    ++iter;
                }
            }
        };
        
        removeAllListenersInVector(sceneGraphPriorityListeners);
        removeAllListenersInVector(fixedPriorityListeners);
        
        // Remove the dirty flag according the 'listenerID'.
        // No need to check whether the dispatcher is dispatching event.
        _priorityDirtyFlagMap.erase(listenerID);
        
        if (!_inDispatch)
        {
            listeners->clear();
            delete listeners;
            _listenerMap.erase(listenerItemIter);
        }
    }
    
    for (auto iter = _toAddedListeners.begin(); iter != _toAddedListeners.end();)
    {
        if ((*iter)->getListenerID() == listenerID)
        {
            (*iter)->setRegistered(false);
            (*iter)->release();
            iter = _toAddedListeners.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

void EventDispatcher::removeEventListenersForType(EventListener::Type listenerType)
{
    if (listenerType == EventListener::Type::TOUCH_ONE_BY_ONE)
    {
        removeEventListenersForListenerID(EventListenerTouchOneByOne::LISTENER_ID);
    }
    else if (listenerType == EventListener::Type::TOUCH_ALL_AT_ONCE)
    {
        removeEventListenersForListenerID(EventListenerTouchAllAtOnce::LISTENER_ID);
    }
    else if (listenerType == EventListener::Type::MOUSE)
    {
        removeEventListenersForListenerID(EventListenerMouse::LISTENER_ID);
    }
    else if (listenerType == EventListener::Type::ACCELERATION)
    {
        removeEventListenersForListenerID(EventListenerAcceleration::LISTENER_ID);
    }
    else if (listenerType == EventListener::Type::KEYBOARD)
    {
        removeEventListenersForListenerID(EventListenerKeyboard::LISTENER_ID);
    }
    else
    {
        CCASSERT(false, "Invalid listener type!");
    }
}

void EventDispatcher::removeCustomEventListeners(const std::string& customEventName)
{
    removeEventListenersForListenerID(customEventName);
}

void EventDispatcher::removeAllEventListeners()
{
    bool cleanMap = true;
    std::vector<EventListener::ListenerID> types(_listenerMap.size());
    
    for (const auto& e : _listenerMap)
    {
        if (_internalCustomListenerIDs.find(e.first) != _internalCustomListenerIDs.end())
        {
            cleanMap = false;
        }
        else
        {
            types.push_back(e.first);
        }
    }

    for (const auto& type : types)
    {
        removeEventListenersForListenerID(type);
    }
    
    if (!_inDispatch && cleanMap)
    {
        _listenerMap.clear();
    }
}

void EventDispatcher::setEnabled(bool isEnabled)
{
    _isEnabled = isEnabled;
}

bool EventDispatcher::isEnabled() const
{
    return _isEnabled;
}

void EventDispatcher::setDirtyForNode(Node* node)
{
    // Mark the node dirty only when there is an eventlistener associated with it. 
    //2017-03-17,所谓dirtyNode其实就是还没有和EventListener相关联的node
    if (_nodeListenersMap.find(node) != _nodeListenersMap.end())
    {
        _dirtyNodes.insert(node);
    }

    // Also set the dirty flag for node's children
    const auto& children = node->getChildren();
    for (const auto& child : children)
    {
        setDirtyForNode(child);
    }
}

void EventDispatcher::setDirty(const EventListener::ListenerID& listenerID, DirtyFlag flag)
{    
    auto iter = _priorityDirtyFlagMap.find(listenerID);
    if (iter == _priorityDirtyFlagMap.end())
    {
        _priorityDirtyFlagMap.insert(std::make_pair(listenerID, flag));
    }
    else
    {
        int ret = (int)flag | (int)iter->second;
        iter->second = (DirtyFlag) ret;
    }
}

NS_CC_END
