/****************************************************************************
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2009      Valentin Milea
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2011      Zynga Inc.
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

#include "CCProtectedNode.h"

NS_CC_BEGIN

bool nodeComparisonLess(Node* n1, Node* n2)
{
    return( n1->getLocalZOrder() < n2->getLocalZOrder() ||
           ( n1->getLocalZOrder() == n2->getLocalZOrder() && n1->getOrderOfArrival() < n2->getOrderOfArrival() )
           );
}

// XXX: Yes, nodes might have a sort problem once every 15 days if the game runs at 60 FPS and each frame sprites are reordered.
static int s_globalOrderOfArrival = 1;

ProtectedNode::ProtectedNode()
{
}

ProtectedNode::~ProtectedNode()
{
    CCLOGINFO( "deallocing ProtectedNode: %p - tag: %i", this, _tag );
}

Node * Node::create(void)
{
	Node * ret = new Node();
    if (ret && ret->init())
    {
        ret->autorelease();
    }
    else
    {
        CC_SAFE_DELETE(ret);
    }
	return ret;
}

void Node::cleanup()
{
    // actions
    this->stopAllActions();
    this->unscheduleAllSelectors();
    
#if CC_ENABLE_SCRIPT_BINDING
    if ( _scriptType != kScriptTypeNone)
    {
        int action = kNodeOnCleanup;
        BasicScriptData data(this,(void*)&action);
        ScriptEvent scriptEvent(kNodeEvent,(void*)&data);
        ScriptEngineManager::getInstance()->getScriptEngine()->sendEvent(&scriptEvent);
    }
#endif // #if CC_ENABLE_SCRIPT_BINDING
    
    // timers
    for( const auto &child: _children)
        child->cleanup();
}


std::string Node::getDescription() const
{
    return StringUtils::format("<Node | Tag = %d", _tag);
}

// lazy allocs
void Node::childrenAlloc(void)
{
    _children.reserve(4);
}

Node* Node::getChildByTag(int tag)
{
    CCASSERT( tag != Node::INVALID_TAG, "Invalid tag");

    for (auto& child : _children)
    {
        if(child && child->_tag == tag)
            return child;
    }
    return nullptr;
}

/* "add" logic MUST only be on this method
* If a class want's to extend the 'addChild' behavior it only needs
* to override this method
*/
void ProtectedNode::addChild(Node *child, int zOrder, int tag)
{    
    CCASSERT( child != nullptr, "Argument must be non-nil");
    CCASSERT( child->_parent == nullptr, "child already added. It can't be added again");

    if (_children.empty())
    {
        this->childrenAlloc();
    }

    this->insertChild(child, zOrder);
    
#if CC_USE_PHYSICS
    if (child->getPhysicsBody() != nullptr)
    {
        child->getPhysicsBody()->setPosition(this->convertToWorldSpace(child->getPosition()));
    }
    
    for (Node* node = this->getParent(); node != nullptr; node = node->getParent())
    {
        if (dynamic_cast<Scene*>(node) != nullptr)
        {
            (dynamic_cast<Scene*>(node))->addChildToPhysicsWorld(child);
            break;
        }
    }
#endif

    child->_tag = tag;

    child->setParent(this);
    child->setOrderOfArrival(s_globalOrderOfArrival++);

    if( _running )
    {
        child->onEnter();
        // prevent onEnterTransitionDidFinish to be called twice when a node is added in onEnter
        if (_isTransitionFinished) {
            child->onEnterTransitionDidFinish();
        }
    }
    
    if (_cascadeColorEnabled)
    {
        updateCascadeColor();
    }
    
    if (_cascadeOpacityEnabled)
    {
        updateCascadeOpacity();
    }
}

void ProtectedNode::removeFromParent()
{
    this->removeFromParentAndCleanup(true);
}

void ProtectedNode::removeFromParentAndCleanup(bool cleanup)
{
    if (_parent != nullptr)
    {
        _parent->removeChild(this,cleanup);
    } 
}

/* "remove" logic MUST only be on this method
* If a class want's to extend the 'removeChild' behavior it only needs
* to override this method
*/
void ProtectedNode::removeChild(Node* child, bool cleanup /* = true */)
{
    // explicit nil handling
    if (_children.empty())
    {
        return;
    }

    ssize_t index = _children.getIndex(child);
    if( index != CC_INVALID_INDEX )
        this->detachChild( child, index, cleanup );
}

void ProtectedNode::removeAllChildrenWithCleanup(bool cleanup)
{
    // not using detachChild improves speed here
    for (auto& child : _children)
    {
        // IMPORTANT:
        //  -1st do onExit
        //  -2nd cleanup
        if(_running)
        {
            child->onExitTransitionDidStart();
            child->onExit();
        }

#if CC_USE_PHYSICS
        if (child->_physicsBody != nullptr)
        {
            child->_physicsBody->removeFromWorld();
        }
#endif

        if (cleanup)
        {
            child->cleanup();
        }
        // set parent nil at the end
        child->setParent(nullptr);
    }
    
    _children.clear();
}

// helper used by reorderChild & add
void Node::insertChild(Node* child, int z)
{
    _reorderChildDirty = true;
    _children.pushBack(child);
    child->_setLocalZOrder(z);
}

void ProtectedNode::sortAllProtectedChildren()
{
    if( _reorderChildDirty ) {
        std::sort( std::begin(_children), std::end(_children), nodeComparisonLess );
        _reorderChildDirty = false;
    }
}

void Node::draw(Renderer* renderer, const kmMat4 &transform, bool transformUpdated)
{
}

void Node::visit(Renderer* renderer, const kmMat4 &parentTransform, bool parentTransformUpdated)
{
    // quick return if not visible. children won't be drawn.
    if (!_visible)
    {
        return;
    }

    bool dirty = _transformUpdated || parentTransformUpdated;
    if(dirty)
        _modelViewTransform = this->transform(parentTransform);
    _transformUpdated = false;


    // IMPORTANT:
    // To ease the migration to v3.0, we still support the kmGL stack,
    // but it is deprecated and your code should not rely on it
    kmGLPushMatrix();
    kmGLLoadMatrix(&_modelViewTransform);

    int i = 0;      // used by _children
    int j = 0;      // used by _protectedChildren

    sortAllChildren();
    sortAllProtectedChildren();

    //
    // draw children and protectedChildren zOrder < 0
    //
    for( ; i < _children.size(); i++ )
    {
        auto node = _children.at(i);

        if ( node && node->_localZOrder < 0 )
            node->visit(renderer, _modelViewTransform, dirty);
        else
            break;
    }

    for( ; j < _protectedChildren.size(); j++ )
    {
        auto node = _protectedChildren.at(j);

        if ( node && node->_localZOrder < 0 )
            node->visit(renderer, _modelViewTransform, dirty);
        else
            break;
    }

    //
    // draw self
    //
    this->draw(renderer, _modelViewTransform, dirty);

    //
    // draw children and protectedChildren zOrder >= 0
    //
    for(auto it=_protectedChildren.cbegin()+j; it != _protectedChildren.cend(); ++it)
        (*it)->visit(renderer, _modelViewTransform, dirty);

    for(auto it=_children.cbegin()+i; it != _children.cend(); ++it)
        (*it)->visit(renderer, _modelViewTransform, dirty);

    // reset for next frame
    _orderOfArrival = 0;
 
    kmGLPopMatrix();
}

kmMat4 Node::transform(const kmMat4& parentTransform)
{
    kmMat4 ret = this->getNodeToParentTransform();
    kmMat4Multiply(&ret, &parentTransform, &ret);

    return ret;
}


#if CC_ENABLE_SCRIPT_BINDING

static bool sendNodeEventToJS(Node* node, int action)
{
    auto scriptEngine = ScriptEngineManager::getInstance()->getScriptEngine();

    if (scriptEngine->isCalledFromScript())
    {
        scriptEngine->setCalledFromScript(false);
    }
    else
    {
        BasicScriptData data(node,(void*)&action);
        ScriptEvent scriptEvent(kNodeEvent,(void*)&data);
        if (scriptEngine->sendEvent(&scriptEvent))
            return true;
    }
    
    return false;
}

static void sendNodeEventToLua(Node* node, int action)
{
    auto scriptEngine = ScriptEngineManager::getInstance()->getScriptEngine();
    
    BasicScriptData data(node,(void*)&action);
    ScriptEvent scriptEvent(kNodeEvent,(void*)&data);
    
    scriptEngine->sendEvent(&scriptEvent);
}

#endif

void Node::onEnter()
{
#if CC_ENABLE_SCRIPT_BINDING
    if (_scriptType == kScriptTypeJavascript)
    {
        if (sendNodeEventToJS(this, kNodeOnEnter))
            return;
    }
#endif
    
    _isTransitionFinished = false;
    
    for( const auto &child: _children)
        child->onEnter();
    
    this->resume();
    
    _running = true;
    
#if CC_ENABLE_SCRIPT_BINDING
    if (_scriptType == kScriptTypeLua)
    {
        sendNodeEventToLua(this, kNodeOnEnter);
    }
#endif
}

void Node::onEnterTransitionDidFinish()
{
#if CC_ENABLE_SCRIPT_BINDING
    if (_scriptType == kScriptTypeJavascript)
    {
        if (sendNodeEventToJS(this, kNodeOnEnterTransitionDidFinish))
            return;
    }
#endif

    _isTransitionFinished = true;
    for( const auto &child: _children)
        child->onEnterTransitionDidFinish();
    
#if CC_ENABLE_SCRIPT_BINDING
    if (_scriptType == kScriptTypeLua)
    {
        sendNodeEventToLua(this, kNodeOnEnterTransitionDidFinish);
    }
#endif
}

void Node::onExitTransitionDidStart()
{
#if CC_ENABLE_SCRIPT_BINDING
    if (_scriptType == kScriptTypeJavascript)
    {
        if (sendNodeEventToJS(this, kNodeOnExitTransitionDidStart))
            return;
    }
#endif
    
    for( const auto &child: _children)
        child->onExitTransitionDidStart();
    
#if CC_ENABLE_SCRIPT_BINDING
    if (_scriptType == kScriptTypeLua)
    {
        sendNodeEventToLua(this, kNodeOnExitTransitionDidStart);
    }
#endif
}

void Node::onExit()
{
#if CC_ENABLE_SCRIPT_BINDING
    if (_scriptType == kScriptTypeJavascript)
    {
        if (sendNodeEventToJS(this, kNodeOnExit))
            return;
    }
#endif
    
    this->pause();
    
    _running = false;
    
    for( const auto &child: _children)
        child->onExit();
    
#if CC_ENABLE_SCRIPT_BINDING
    if (_scriptType == kScriptTypeLua)
    {
        sendNodeEventToLua(this, kNodeOnExit);
    }
#endif
}

void Node::setEventDispatcher(EventDispatcher* dispatcher)
{
    if (dispatcher != _eventDispatcher)
    {
        _eventDispatcher->removeEventListenersForTarget(this);
        CC_SAFE_RETAIN(dispatcher);
        CC_SAFE_RELEASE(_eventDispatcher);
        _eventDispatcher = dispatcher;
    }
}

void Node::setActionManager(ActionManager* actionManager)
{
    if( actionManager != _actionManager ) {
        this->stopAllActions();
        CC_SAFE_RETAIN(actionManager);
        CC_SAFE_RELEASE(_actionManager);
        _actionManager = actionManager;
    }
}

Action * Node::runAction(Action* action)
{
    CCASSERT( action != nullptr, "Argument must be non-nil");
    _actionManager->addAction(action, this, !_running);
    return action;
}

void Node::stopAllActions()
{
    _actionManager->removeAllActionsFromTarget(this);
}

void Node::stopAction(Action* action)
{
    _actionManager->removeAction(action);
}

void Node::stopActionByTag(int tag)
{
    CCASSERT( tag != Action::INVALID_TAG, "Invalid tag");
    _actionManager->removeActionByTag(tag, this);
}

Action * Node::getActionByTag(int tag)
{
    CCASSERT( tag != Action::INVALID_TAG, "Invalid tag");
    return _actionManager->getActionByTag(tag, this);
}

ssize_t Node::getNumberOfRunningActions() const
{
    return _actionManager->getNumberOfRunningActionsInTarget(this);
}

// Node - Callbacks

void Node::setScheduler(Scheduler* scheduler)
{
    if( scheduler != _scheduler ) {
        this->unscheduleAllSelectors();
        CC_SAFE_RETAIN(scheduler);
        CC_SAFE_RELEASE(_scheduler);
        _scheduler = scheduler;
    }
}

bool Node::isScheduled(SEL_SCHEDULE selector)
{
    return _scheduler->isScheduled(selector, this);
}

void Node::scheduleUpdate()
{
    scheduleUpdateWithPriority(0);
}

void Node::scheduleUpdateWithPriority(int priority)
{
    _scheduler->scheduleUpdate(this, priority, !_running);
}

void Node::scheduleUpdateWithPriorityLua(int nHandler, int priority)
{
    unscheduleUpdate();
    
#if CC_ENABLE_SCRIPT_BINDING
    _updateScriptHandler = nHandler;
#endif
    
    _scheduler->scheduleUpdate(this, priority, !_running);
}

void Node::unscheduleUpdate()
{
    _scheduler->unscheduleUpdate(this);
    
#if CC_ENABLE_SCRIPT_BINDING
    if (_updateScriptHandler)
    {
        ScriptEngineManager::getInstance()->getScriptEngine()->removeScriptHandler(_updateScriptHandler);
        _updateScriptHandler = 0;
    }
#endif
}

void Node::schedule(SEL_SCHEDULE selector)
{
    this->schedule(selector, 0.0f, kRepeatForever, 0.0f);
}

void Node::schedule(SEL_SCHEDULE selector, float interval)
{
    this->schedule(selector, interval, kRepeatForever, 0.0f);
}

void Node::schedule(SEL_SCHEDULE selector, float interval, unsigned int repeat, float delay)
{
    CCASSERT( selector, "Argument must be non-nil");
    CCASSERT( interval >=0, "Argument must be positive");

    _scheduler->schedule(selector, this, interval , repeat, delay, !_running);
}

void Node::scheduleOnce(SEL_SCHEDULE selector, float delay)
{
    this->schedule(selector, 0.0f, 0, delay);
}

void Node::unschedule(SEL_SCHEDULE selector)
{
    // explicit null handling
    if (selector == nullptr)
        return;
    
    _scheduler->unschedule(selector, this);
}

void Node::unscheduleAllSelectors()
{
    _scheduler->unscheduleAllForTarget(this);
}

void Node::resume()
{
    _scheduler->resumeTarget(this);
    _actionManager->resumeTarget(this);
    _eventDispatcher->resumeEventListenersForTarget(this);
}

void Node::pause()
{
    _scheduler->pauseTarget(this);
    _actionManager->pauseTarget(this);
    _eventDispatcher->pauseEventListenersForTarget(this);
}

void Node::resumeSchedulerAndActions()
{
    resume();
}

void Node::pauseSchedulerAndActions()
{
    pause();
}

// override me
void Node::update(float fDelta)
{
#if CC_ENABLE_SCRIPT_BINDING
    if (0 != _updateScriptHandler)
    {
        //only lua use
        SchedulerScriptData data(_updateScriptHandler,fDelta);
        ScriptEvent event(kScheduleEvent,&data);
        ScriptEngineManager::getInstance()->getScriptEngine()->sendEvent(&event);
    }
#endif
    
    if (_componentContainer && !_componentContainer->isEmpty())
    {
        _componentContainer->visit(fDelta);
    }
}

AffineTransform Node::getNodeToParentAffineTransform() const
{
    AffineTransform ret;
    kmMat4 ret4 = getNodeToParentTransform();
    GLToCGAffine(ret4.mat, &ret);

    return ret;
}

const kmMat4& Node::getNodeToParentTransform() const
{
    if (_transformDirty)
    {
        // Translate values
        float x = _position.x;
        float y = _position.y;
        float z = _positionZ;

        if (_ignoreAnchorPointForPosition)
        {
            x += _anchorPointInPoints.x;
            y += _anchorPointInPoints.y;
        }

        // Rotation values
		// Change rotation code to handle X and Y
		// If we skew with the exact same value for both x and y then we're simply just rotating
        float cx = 1, sx = 0, cy = 1, sy = 0;
        if (_rotationZ_X || _rotationZ_Y)
        {
            float radiansX = -CC_DEGREES_TO_RADIANS(_rotationZ_X);
            float radiansY = -CC_DEGREES_TO_RADIANS(_rotationZ_Y);
            cx = cosf(radiansX);
            sx = sinf(radiansX);
            cy = cosf(radiansY);
            sy = sinf(radiansY);
        }

        bool needsSkewMatrix = ( _skewX || _skewY );


        // optimization:
        // inline anchor point calculation if skew is not needed
        // Adjusted transform calculation for rotational skew
        if (! needsSkewMatrix && !_anchorPointInPoints.equals(Point::ZERO))
        {
            x += cy * -_anchorPointInPoints.x * _scaleX + -sx * -_anchorPointInPoints.y * _scaleY;
            y += sy * -_anchorPointInPoints.x * _scaleX +  cx * -_anchorPointInPoints.y * _scaleY;
        }


        // Build Transform Matrix
        // Adjusted transform calculation for rotational skew
        kmScalar mat[] = {
                        cy * _scaleX,   sy * _scaleX,   0,          0,
                        -sx * _scaleY,  cx * _scaleY,   0,          0,
                        0,              0,              _scaleZ,    0,
                        x,              y,              z,          1 };
        
        kmMat4Fill(&_transform, mat);

        // XXX
        // FIX ME: Expensive operation.
        // FIX ME: It should be done together with the rotationZ
        if(_rotationY) {
            kmMat4 rotY;
            kmMat4RotationY(&rotY,CC_DEGREES_TO_RADIANS(_rotationY));
            kmMat4Multiply(&_transform, &_transform, &rotY);
        }
        if(_rotationX) {
            kmMat4 rotX;
            kmMat4RotationX(&rotX,CC_DEGREES_TO_RADIANS(_rotationX));
            kmMat4Multiply(&_transform, &_transform, &rotX);
        }

        // XXX: Try to inline skew
        // If skew is needed, apply skew and then anchor point
        if (needsSkewMatrix)
        {
            kmMat4 skewMatrix = { 1, (float)tanf(CC_DEGREES_TO_RADIANS(_skewY)), 0, 0,
                                  (float)tanf(CC_DEGREES_TO_RADIANS(_skewX)), 1, 0, 0,
                                  0,  0,  1, 0,
                                  0,  0,  0, 1};

            kmMat4Multiply(&_transform, &_transform, &skewMatrix);

            // adjust anchor point
            if (!_anchorPointInPoints.equals(Point::ZERO))
            {
                // XXX: Argh, kmMat needs a "translate" method.
                // XXX: Although this is faster than multiplying a vec4 * mat4
                _transform.mat[12] += _transform.mat[0] * -_anchorPointInPoints.x + _transform.mat[4] * -_anchorPointInPoints.y;
                _transform.mat[13] += _transform.mat[1] * -_anchorPointInPoints.x + _transform.mat[5] * -_anchorPointInPoints.y;
            }
        }

        if (_useAdditionalTransform)
        {
            kmMat4Multiply(&_transform, &_transform, &_additionalTransform);
        }

        _transformDirty = false;
    }

    return _transform;
}

void Node::setNodeToParentTransform(const kmMat4& transform)
{
    _transform = transform;
    _transformDirty = false;
    _transformUpdated = true;
}

void Node::setAdditionalTransform(const AffineTransform& additionalTransform)
{
    kmMat4 tmp;
    CGAffineToGL(additionalTransform, tmp.mat);
    setAdditionalTransform(&tmp);
}

void Node::setAdditionalTransform(kmMat4* additionalTransform)
{
    if(additionalTransform == nullptr) {
        _useAdditionalTransform = false;
    } else {
        _additionalTransform = *additionalTransform;
        _useAdditionalTransform = true;
    }
    _transformUpdated = _transformDirty = _inverseDirty = true;
}


AffineTransform Node::getParentToNodeAffineTransform() const
{
    AffineTransform ret;
    kmMat4 ret4 = getParentToNodeTransform();

    GLToCGAffine(ret4.mat,&ret);
    return ret;
}

const kmMat4& Node::getParentToNodeTransform() const
{
    if ( _inverseDirty ) {
        kmMat4Inverse(&_inverse, &_transform);
        _inverseDirty = false;
    }

    return _inverse;
}


AffineTransform Node::getNodeToWorldAffineTransform() const
{
    AffineTransform t = this->getNodeToParentAffineTransform();

    for (Node *p = _parent; p != nullptr; p = p->getParent())
        t = AffineTransformConcat(t, p->getNodeToParentAffineTransform());

    return t;
}

kmMat4 Node::getNodeToWorldTransform() const
{
    kmMat4 t = this->getNodeToParentTransform();

    for (Node *p = _parent; p != nullptr; p = p->getParent())
        kmMat4Multiply(&t, &p->getNodeToParentTransform(), &t);

    return t;
}

AffineTransform Node::getWorldToNodeAffineTransform() const
{
    return AffineTransformInvert(this->getNodeToWorldAffineTransform());
}

kmMat4 Node::getWorldToNodeTransform() const
{
    kmMat4 tmp, tmp2;

    tmp2 = this->getNodeToWorldTransform();
    kmMat4Inverse(&tmp, &tmp2);
    return tmp;
}


Point Node::convertToNodeSpace(const Point& worldPoint) const
{
    kmMat4 tmp = getWorldToNodeTransform();
    kmVec3 vec3 = {worldPoint.x, worldPoint.y, 0};
    kmVec3 ret;
    kmVec3Transform(&ret, &vec3, &tmp);
    return Point(ret.x, ret.y);
}

Point Node::convertToWorldSpace(const Point& nodePoint) const
{
    kmMat4 tmp = getNodeToWorldTransform();
    kmVec3 vec3 = {nodePoint.x, nodePoint.y, 0};
    kmVec3 ret;
    kmVec3Transform(&ret, &vec3, &tmp);
    return Point(ret.x, ret.y);

}

Point Node::convertToNodeSpaceAR(const Point& worldPoint) const
{
    Point nodePoint = convertToNodeSpace(worldPoint);
    return nodePoint - _anchorPointInPoints;
}

Point Node::convertToWorldSpaceAR(const Point& nodePoint) const
{
    Point pt = nodePoint + _anchorPointInPoints;
    return convertToWorldSpace(pt);
}

Point Node::convertToWindowSpace(const Point& nodePoint) const
{
    Point worldPoint = this->convertToWorldSpace(nodePoint);
    return Director::getInstance()->convertToUI(worldPoint);
}

// convenience methods which take a Touch instead of Point
Point Node::convertTouchToNodeSpace(Touch *touch) const
{
    Point point = touch->getLocation();
    return this->convertToNodeSpace(point);
}
Point Node::convertTouchToNodeSpaceAR(Touch *touch) const
{
    Point point = touch->getLocation();
    return this->convertToNodeSpaceAR(point);
}

void Node::updateTransform()
{
    // Recursively iterate over children
    for( const auto &child: _children)
        child->updateTransform();
}

Component* Node::getComponent(const std::string& pName)
{
    if( _componentContainer )
        return _componentContainer->get(pName);
    return nullptr;
}

bool Node::addComponent(Component *pComponent)
{
    // lazy alloc
    if( !_componentContainer )
        _componentContainer = new ComponentContainer(this);
    return _componentContainer->add(pComponent);
}

bool Node::removeComponent(const std::string& pName)
{
    if( _componentContainer )
        return _componentContainer->remove(pName);
    return false;
}

void Node::removeAllComponents()
{
    if( _componentContainer )
        _componentContainer->removeAll();
}

#if CC_USE_PHYSICS
void Node::setPhysicsBody(PhysicsBody* body)
{
    if (body != nullptr)
    {
        body->_node = this;
        body->retain();
        
        // physics rotation based on body position, but node rotation based on node anthor point
        // it cann't support both of them, so I clear the anthor point to default.
        if (!getAnchorPoint().equals(Point::ANCHOR_MIDDLE))
        {
            CCLOG("Node warning: setPhysicsBody sets anchor point to Point::ANCHOR_MIDDLE.");
            setAnchorPoint(Point::ANCHOR_MIDDLE);
        }
    }
    
    if (_physicsBody != nullptr)
    {
        PhysicsWorld* world = _physicsBody->getWorld();
        _physicsBody->removeFromWorld();
        _physicsBody->_node = nullptr;
        _physicsBody->release();
        
        if (world != nullptr && body != nullptr)
        {
            world->addBody(body);
        }
    }
    
    _physicsBody = body;
    if (body != nullptr)
    {
        Node* parent = getParent();
        Point pos = parent != nullptr ? parent->convertToWorldSpace(getPosition()) : getPosition();
        _physicsBody->setPosition(pos);
        _physicsBody->setRotation(getRotation());
    }
}

PhysicsBody* Node::getPhysicsBody() const
{
    return _physicsBody;
}
#endif //CC_USE_PHYSICS

GLubyte Node::getOpacity(void) const
{
	return _realOpacity;
}

GLubyte Node::getDisplayedOpacity(void) const
{
	return _displayedOpacity;
}

void Node::setOpacity(GLubyte opacity)
{
    _displayedOpacity = _realOpacity = opacity;
    
    updateCascadeOpacity();
}

void Node::updateDisplayedOpacity(GLubyte parentOpacity)
{
	_displayedOpacity = _realOpacity * parentOpacity/255.0;
    updateColor();
    
    if (_cascadeOpacityEnabled)
    {
        for(auto child : _children){
            child->updateDisplayedOpacity(_displayedOpacity);
        }
    }
}

bool Node::isCascadeOpacityEnabled(void) const
{
    return _cascadeOpacityEnabled;
}

void Node::setCascadeOpacityEnabled(bool cascadeOpacityEnabled)
{
    if (_cascadeOpacityEnabled == cascadeOpacityEnabled)
    {
        return;
    }
    
    _cascadeOpacityEnabled = cascadeOpacityEnabled;
    
    if (cascadeOpacityEnabled)
    {
        updateCascadeOpacity();
    }
    else
    {
        disableCascadeOpacity();
    }
}

void Node::updateCascadeOpacity()
{
    GLubyte parentOpacity = 255;
    
    if (_parent != nullptr && _parent->isCascadeOpacityEnabled())
    {
        parentOpacity = _parent->getDisplayedOpacity();
    }
    
    updateDisplayedOpacity(parentOpacity);
}

void Node::disableCascadeOpacity()
{
    _displayedOpacity = _realOpacity;
    
    for(auto child : _children){
        child->updateDisplayedOpacity(255);
    }
}

const Color3B& Node::getColor(void) const
{
	return _realColor;
}

const Color3B& Node::getDisplayedColor() const
{
	return _displayedColor;
}

void Node::setColor(const Color3B& color)
{
	_displayedColor = _realColor = color;
	
	updateCascadeColor();
}

void Node::updateDisplayedColor(const Color3B& parentColor)
{
	_displayedColor.r = _realColor.r * parentColor.r/255.0;
	_displayedColor.g = _realColor.g * parentColor.g/255.0;
	_displayedColor.b = _realColor.b * parentColor.b/255.0;
    updateColor();
    
    if (_cascadeColorEnabled)
    {
        for(const auto &child : _children){
            child->updateDisplayedColor(_displayedColor);
        }
    }
}

bool Node::isCascadeColorEnabled(void) const
{
    return _cascadeColorEnabled;
}

void Node::setCascadeColorEnabled(bool cascadeColorEnabled)
{
    if (_cascadeColorEnabled == cascadeColorEnabled)
    {
        return;
    }
    
    _cascadeColorEnabled = cascadeColorEnabled;
    
    if (_cascadeColorEnabled)
    {
        updateCascadeColor();
    }
    else
    {
        disableCascadeColor();
    }
}

void Node::updateCascadeColor()
{
	Color3B parentColor = Color3B::WHITE;
    if (_parent && _parent->isCascadeColorEnabled())
    {
        parentColor = _parent->getDisplayedColor();
    }
    
    updateDisplayedColor(parentColor);
}

void Node::disableCascadeColor()
{
    for(auto child : _children){
        child->updateDisplayedColor(Color3B::WHITE);
    }
}

__NodeRGBA::__NodeRGBA()
{
    CCLOG("NodeRGBA deprecated.");
}

NS_CC_END
