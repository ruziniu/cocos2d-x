//
//  SkyboxNode.m
//  testar1
//
//  Created by Pasi Kettunen on 12.12.2012.
//
//

#include "CCObjNode.h"
#include "CCObjSurface.h"
#include "CCTexture2D.h"
#include "CCGLProgram.h"
#include "CCDirector.h"
#include "CCTextureCache.h"
#include "renderer/CCRenderer.h"

#include "kazmath/kazmath.h"
#include "kazmath/GL/matrix.h"

NS_CC_BEGIN

#define USE_VBO

#define STRINGIFY(A)  #A
//#include "../Shaders/TexturedLighting.es2.vert.h"
#include "TexturedLighting1.es2.vert.h"
#include "TexturedLighting.es2.frag.h"
#include "ColorLighting1.es2.frag.h"

struct UniformHandles {
    GLuint Modelview;
    GLuint Projection;
    GLuint NormalMatrix;
    GLuint LightPosition;
    GLint AmbientMaterial;
    GLint SpecularMaterial;
    GLint DiffuseMaterial;
    GLint Shininess;
    GLint Sampler;
};

struct AttributeHandles {
    GLint Position;
    GLint Normal;
    GLint TextureCoord;
};
UniformHandles m_uniforms;
AttributeHandles m_attributes;

ObjNode* ObjNode::create()
{
    auto ret = new ObjNode;
    if( ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    return nullptr;
}

ObjNode::ObjNode()
: _texture(nullptr)
{
}

ObjNode::~ObjNode()
{
}

bool ObjNode::init()
{
    return true;
}

void ObjNode::initializeModel()
{
    if (_model) {

        unsigned char vertexFlags = VertexFlagsNormals | VertexFlagsTexCoords;
        _model->GenerateVertices(vertices, vertexFlags);
        
        int indexCount = _model->GetTriangleIndexCount();
        indices.resize(indexCount);
        _model->GenerateTriangleIndices(indices);
        _drawable.IndexCount = indexCount;

        delete _model;
        _model = NULL;
#ifdef USE_VBO
        this->buildBuffers();
#endif
        this->updateBlendFunc();
        _program = this->buildProgram( _texture->getName() != 0);
    }
}

void ObjNode::setModel(ISurface *model)
{
    _model = model;
    this->initializeModel();
}

GLuint ObjNode::buildProgram(bool textured)
{
    GLuint program;
    // Create the GLSL program.
    if (textured) {
        program = ObjSurface::BuildProgram(SimpleVertexShader1, SimpleFragmentShader);
    }
    else
        program = ObjSurface::BuildProgram(SimpleVertexShader1, ColorLighting1);
    //glUseProgram(_program);
    
    // Extract the handles to attributes and uniforms.
    m_attributes.Position = glGetAttribLocation(program, "Position");
    m_attributes.Normal = glGetAttribLocation(program, "Normal");
    
    m_uniforms.DiffuseMaterial = glGetUniformLocation(program, "DiffuseMaterial");
    if (textured) {
        m_attributes.TextureCoord = glGetAttribLocation(program, "TextureCoord");
        m_uniforms.Sampler = glGetUniformLocation(program, "Sampler");
    }
    else {
        m_attributes.TextureCoord = 0;
        m_uniforms.Sampler = 0;
    }
    m_uniforms.Projection = glGetUniformLocation(program, "Projection");
    m_uniforms.Modelview = glGetUniformLocation(program, "Modelview");
    m_uniforms.NormalMatrix = glGetUniformLocation(program, "NormalMatrix");
    m_uniforms.LightPosition = glGetUniformLocation(program, "LightPosition");
    m_uniforms.AmbientMaterial = glGetUniformLocation(program, "AmbientMaterial");
    m_uniforms.SpecularMaterial = glGetUniformLocation(program, "SpecularMaterial");
    m_uniforms.Shininess = glGetUniformLocation(program, "Shininess");
    return program;
}

#ifdef USE_VBO
void ObjNode::buildBuffers()
{
    GLuint vertexBuffer;
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(vertices[0]),
                 &vertices[0],
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // Create a new VBO for the indices
    size_t indexCount = indices.size();// model->GetTriangleIndexCount();
    GLuint indexBuffer;

    glGenBuffers(1, &indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indexCount * sizeof(GLushort),
                 &indices[0],
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    _drawable.VertexBuffer = vertexBuffer;
    _drawable.IndexBuffer = indexBuffer;
    _drawable.IndexCount = indexCount;
}

#endif
void ObjNode::draw()
{
    _customCommand.init(_globalZOrder);
    _customCommand.func = CC_CALLBACK_0(ObjNode::onDraw, this);
    Director::getInstance()->getRenderer()->addCommand(&_customCommand);
}

void ObjNode::onDraw()
{
    float _contentScale = _scaleX;

    //CC_NODE_DRAW_SETUP();

    GL::useProgram(_program);

    GL::blendFunc( _blendFunc.src, _blendFunc.dst );
    kmGLLoadIdentity();
    
	if (_texture->getName()) {
        GL::bindTexture2D(_texture->getName());
        glUniform1i(m_uniforms.Sampler, 0);
        GL::enableVertexAttribs( GL::VERTEX_ATTRIB_FLAG_POS_COLOR_TEX );
    }
    else {
        GL::enableVertexAttribs( GL::VERTEX_ATTRIB_FLAG_POSITION );
    }
    
    // Set up some default material parameters.
    glUniform3f(m_uniforms.AmbientMaterial, 0.1f, 0.1f, 0.1f);
    glUniform3f(m_uniforms.SpecularMaterial, 0.5, 0.5, 0.5);
    glUniform1f(m_uniforms.Shininess, 10);
    //glUniform1f(m_uniforms.Shininess, 0);
    
    // Set the diffuse color.
    vec3 color = vec3(1,1, 1.1) * .50;
    glUniform3f(m_uniforms.DiffuseMaterial, color.x, color.y, color.z);
    //glVertexAttrib3f(m_attributes.DiffuseMaterial, color.x, color.y, color.z);
    
    // Initialize various state.
    glEnableVertexAttribArray(m_attributes.Position);
    glEnableVertexAttribArray(m_attributes.Normal);
    if (_texture->getName())
        glEnableVertexAttribArray(m_attributes.TextureCoord);
    glEnable(GL_DEPTH_TEST);

    // Set the light position.
    vec3 lightPosition = vec3(-2.25, 0.25, 1);
    lightPosition.Normalize();
    vec4 lp = vec4(lightPosition.x, lightPosition.y, lightPosition.z, 0);
    glUniform3fv(m_uniforms.LightPosition, 1, lp.Pointer());

    // Set up transforms.
    Size size = Director::getInstance()->getWinSize();
    Point pos = Point(_position.x - size.width / 2, _position.y - size.height / 2);
    mat4 m_translation = mat4::Translate(pos.x * 8.0 / size.width, pos.y * 8.0 / size.width, _vertexZ);
    //mat4 m_scale = mat4::Scale(.5 + zRot * .07);
    mat4 m_scale = mat4::Scale(_contentScale);

    // Set the model-view transform.
    Quaternion rot = Quaternion::CreateFromAxisAngle(vec3(0, 1, 0), yRot * Pi / 180);
    Quaternion rot2 = Quaternion::CreateFromAxisAngle(vec3(0, 0, 1), zRot * Pi / 180);
    Quaternion rot3 = Quaternion::CreateFromAxisAngle(vec3(1, 0, 0), xRot * Pi / 180);
    //rot = rot.Rotated(rot);
    mat4 rotation = rot2.ToMatrix();
    rotation *= rot.ToMatrix();
    rotation *= rot3.ToMatrix();
    mat4 modelview = m_scale * rotation * m_translation;

    glUniformMatrix4fv(m_uniforms.Modelview, 1, 0, modelview.Pointer());
    
    // Set the normal matrix.
    // It's orthogonal, so its Inverse-Transpose is itself!
    mat3 normalMatrix = modelview.ToMat3();
    glUniformMatrix3fv(m_uniforms.NormalMatrix, 1, 0, normalMatrix.Pointer());
    
    // Set the projection transform.
    
    float h = 4.0f * size.height / size.width;
    float k = 1.0;
    h *= k;
    mat4 projectionMatrix = mat4::Frustum(-2 * k, 2 * k, -h / 2, h / 2, 4, 40);

    //projectionMatrix *= mat4::Scale(1.0);
    glUniformMatrix4fv(m_uniforms.Projection, 1, 0, projectionMatrix.Pointer());

#ifdef USE_VBO
    // Draw the surface using VBOs
    int stride = sizeof(vec3) + sizeof(vec3) + sizeof(vec2);
    const GLvoid* normalOffset = (const GLvoid*) sizeof(vec3);
    const GLvoid* texCoordOffset = (const GLvoid*) (2 * sizeof(vec3));
    GLint position = m_attributes.Position;
    GLint normal = m_attributes.Normal;
    GLint texCoord = m_attributes.TextureCoord;

    glBindBuffer(GL_ARRAY_BUFFER, _drawable.VertexBuffer);
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, stride, 0);
    glVertexAttribPointer(normal, 3, GL_FLOAT, GL_FALSE, stride, normalOffset);
    if (_texture->getName())
        glVertexAttribPointer(texCoord, 2, GL_FLOAT, GL_FALSE, stride, texCoordOffset);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _drawable.IndexBuffer);
    glDrawElements(GL_TRIANGLES, _drawable.IndexCount, GL_UNSIGNED_SHORT, 0);
    
    //glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

#else
    // Draw the surface without VBOs
    int stride = sizeof(vec3) + sizeof(vec3) + sizeof(vec2);
    const GLvoid* normals = (const GLvoid*) ((char*)&vertices[0] + sizeof(vec3));
    const GLvoid* texCoords = (const GLvoid*) ((char*)&vertices[0] + (2 * sizeof(vec3)));
    const GLvoid* Vertices = (const GLvoid*)&vertices[0];
    GLint position = m_attributes.Position;
    GLint normal = m_attributes.Normal;
    GLint texCoord = m_attributes.TextureCoord;
    
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, stride, Vertices);
    glVertexAttribPointer(normal, 3, GL_FLOAT, GL_FALSE, stride, normals);
    if ([texture_ name])
        glVertexAttribPointer(texCoord, 2, GL_FLOAT, GL_FALSE, stride, texCoords);
    
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, &indices[0]);
#endif
    
    /*glDisableVertexAttribArray(m_attributes.Position);
    glDisableVertexAttribArray(m_attributes.Normal);
    if ([texture_ name])
        glDisableVertexAttribArray(m_attributes.TextureCoord);*/

    glDisable(GL_DEPTH_TEST);
    CC_INCREMENT_GL_DRAWS(1);
    
}

void ObjNode::setTextureName(const std::string& textureName)
{
    auto cache = Director::getInstance()->getTextureCache();
    Texture2D *tex = cache->addImage(textureName);
	if( tex ) {
        this->setTexture(tex);
    }
}

void ObjNode::removeTexture()
{
	if( _texture ) {
        _texture->release();

        this->updateBlendFunc();
        _program = this->buildProgram(_texture->getName() != 0);
	}
}

#pragma mark ObjNode - CocosNodeTexture protocol

void ObjNode::updateBlendFunc()
{
	// it is possible to have an untextured sprite
	if( !_texture || ! _texture->hasPremultipliedAlpha() ) {
		_blendFunc.src = GL_SRC_ALPHA;
		_blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
	} else {
		_blendFunc.src = CC_BLEND_SRC;
		_blendFunc.dst = CC_BLEND_DST;
	}
}

void ObjNode::setTexture(Texture2D* texture)
{
	// accept texture==nil as argument
	CCASSERT( texture , "setTexture expects a Texture2D. Invalid argument");
    
	if( _texture != texture ) {
        if(_texture)
            _texture->release();

		_texture = texture;
        _texture->retain();
        
        this->updateBlendFunc();
        _program = this->buildProgram( _texture->getName() != 0);
	}
}

NS_CC_END
