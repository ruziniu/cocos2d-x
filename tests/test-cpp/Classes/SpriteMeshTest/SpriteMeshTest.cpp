/****************************************************************************
 Copyright (c) 2012 cocos2d-x.org
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

#include "SpriteMeshTest.h"
#include "../testResource.h"
#include "cocos2d.h"

#include "3d/CCSprite3D.h"

static std::function<Layer*()> createFunctions[] = {

    CL(SpriteMeshTest1),
};

static int sceneIdx=-1;
#define MAX_LAYER (sizeof(createFunctions) / sizeof(createFunctions[0]))

static Layer* nextTest()
{
    sceneIdx++;
    sceneIdx = sceneIdx % MAX_LAYER;

    auto layer = (createFunctions[sceneIdx])();
    return layer;
}

static Layer* prevTest()
{
    sceneIdx--;
    int total = MAX_LAYER;
    if( sceneIdx < 0 )
        sceneIdx += total;

    auto layer = (createFunctions[sceneIdx])();
    return layer;
}

static Layer* restartTest()
{
    auto layer = (createFunctions[sceneIdx])();
    return layer;
}

void SpriteMeshTestScene::runThisTest()
{
    auto layer = nextTest();
    addChild(layer);

    Director::getInstance()->replaceScene(this);
}

//------------------------------------------------------------------
//
// SpriteMeshTestDemo
//
//------------------------------------------------------------------

SpriteMeshTestDemo::SpriteMeshTestDemo(void)
: BaseTest()
{
}

SpriteMeshTestDemo::~SpriteMeshTestDemo(void)
{
}

std::string SpriteMeshTestDemo::title() const
{
    return "No title";
}

std::string SpriteMeshTestDemo::subtitle() const
{
    return "";
}

void SpriteMeshTestDemo::onEnter()
{
    BaseTest::onEnter();
}

void SpriteMeshTestDemo::restartCallback(Object* sender)
{
    auto s = new SpriteMeshTestScene();
    s->addChild(restartTest());

    Director::getInstance()->replaceScene(s);
    s->release();
}

void SpriteMeshTestDemo::nextCallback(Object* sender)
{
    auto s = new SpriteMeshTestScene();
    s->addChild( nextTest() );
    Director::getInstance()->replaceScene(s);
    s->release();
}

void SpriteMeshTestDemo::backCallback(Object* sender)
{
    auto s = new SpriteMeshTestScene();
    s->addChild( prevTest() );
    Director::getInstance()->replaceScene(s);
    s->release();
} 


//------------------------------------------------------------------
//
// SpriteMeshTest1
//
//------------------------------------------------------------------

bool SpriteMeshTest1::init()
{
    Size s = Director::getInstance()->getWinSize();
    Point m = Point(s.width/2, s.height/2);

//    auto objNode = Sprite3D::create("models/fighter.obj", "models/fighter.png");
    auto sprite3d = Sprite3D::create("models/Scania4.obj", "models/car00.png");

    float objScale = 400.0;    // scale up a lot
    m = Point(m.x, m.y * .5); // move the truck down a little

    sprite3d->setPosition(m);
    sprite3d->setVertexZ(0);
    sprite3d->setScale(objScale);

//    sprite3d->setRotation(45);

    this->addChild(sprite3d);

    return true;
}

std::string SpriteMeshTest1::title() const
{
    return "Sprite Mesh";
}

std::string SpriteMeshTest1::subtitle() const
{
    return "Testing Truck";
}

