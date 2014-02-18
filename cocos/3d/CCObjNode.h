//
//  SkyboxNode.h
//  testar1
//
//  Created by Pasi Kettunen on 12.12.2012.
//
//


/*
 *
 * SkyboxNode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SkyboxNode is distributed WITHOUT ANY WARRANTY; See the
 * GNU General Public License for more details.
 *
 * Note: The 'cocos2d for iPhone' license also applies if used in conjunction
 * with the Cocos2D framework.
 */

#ifndef __CCOBJNODE_H_
#define __CCOBJNODE_H_

#include <vector>

#include "CCNode.h"
#include "Interfaces.h"
#include "renderer/CCCustomCommand.h"

NS_CC_BEGIN

class Texture2D;

struct Drawable {
    GLuint VertexBuffer;
    GLuint IndexBuffer;
    size_t IndexCount;
};

class ObjNode : public Node
{
public:
    static ObjNode* create();

    void initializeModel();
    void setModel(ISurface *model);
    GLuint buildProgram(bool textured);
    void buildBuffers();
    void draw();
    void onDraw();
    void setTexture(Texture2D* texture);
    void updateBlendFunc();
    void setTextureName(const std::string& textureName);
    void removeTexture();


protected:
    ObjNode();
    virtual ~ObjNode();
    bool init();

    // the current rotation offset
    float xRot, yRot, zRot;
    ISurface *_model;
    
    Drawable _drawable;
    
    std::vector<GLfloat> vertices;
    std::vector<GLushort> indices;
    GLuint _program;

    BlendFunc _blendFunc;
    Texture2D *_texture;
    CustomCommand _customCommand;
};

NS_CC_END

#endif // __CCOBJNODE_H_
