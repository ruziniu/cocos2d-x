#include "CCObjSurface.h"
#include <list>
#include <fstream>
#include <assert.h>
#include <iostream>
#include "CCGL.h"

using namespace std;

ObjSurface::ObjSurface(const string& name) :
    m_name(name),
    m_faceCount(0),
    m_vertexCount(0),
  m_texelCount(0)
{
    if (GetTexelCount() > 0) {
        m_texels.resize(GetTexelCount());
    }
    vector<vec2>::iterator texel = m_texels.begin();
    
    m_faces.resize(GetTriangleIndexCount() / 3);
    ifstream objFile(m_name.c_str());
    float dumy;
    vector<Face>::iterator face = m_faces.begin();
    while (objFile) {
        char c = objFile.get();
        if (c == 'f' && objFile.get() == ' ') {
            assert(face != m_faces.end() && "parse error");
            face->texi = ivec3(1,1,1);
            objFile >> face->face.x;
            if ((c = objFile.get()) == '/') {
                objFile >> face->texi.x;
                if ((c = objFile.get()) == '/')
                    objFile >> dumy;
            }
            
            objFile >> face->face.y;
            if ((c = objFile.get()) == '/') {
                objFile >> face->texi.y;
                if ((c = objFile.get()) == '/')
                    objFile >> dumy;
            }
            
            objFile >> face->face.z;
            if ((c = objFile.get()) == '/') {
                objFile >> face->texi.z;
                if ((c = objFile.get()) == '/') {
                    objFile >> dumy;
                }
            }
            if (c != '\n')
                objFile.ignore(MaxLineSize, '\n');
            
            face->face -= ivec3(1, 1, 1);
            face->texi -= ivec3(1, 1, 1);
            ++face;
        }
        else if (c == 'v') {
            if ((c = objFile.get()) == 't') {
                objFile >> texel->x >> texel->y;

                /*while (texel->x >= 1.000) {
                    texel->x -= 1.0;
                }
                while (texel->y >= 1.000) {
                    texel->y -= 1.0;
                }*/
//                texel->x = 1.0 - texel->x;
                //if (texel->x > 1.0 || texel->y > 1.0) {
                texel->y = 1.0 - texel->y;
                ++texel;
                //}
            }
            if (c != '\n')
                objFile.ignore(MaxLineSize, '\n');
        }
        else if (c != '\n')
            objFile.ignore(MaxLineSize, '\n');
    }
    assert(face == m_faces.end() && "parse error");
}

ObjSurface::~ObjSurface() {
    
}
void ObjSurface::countVertexData() const {
    ifstream objFile(m_name.c_str());
    m_faceCount = 0;
    while (objFile) {
        char c = objFile.get();
        if (c == 'v') {
            if ((c = objFile.get()) == ' ')
                m_vertexCount++;
            else if (c == 't')
                m_texelCount++;
        }
        else if (c == 'f') {
            if ((c = objFile.get()) == ' ')
                m_faceCount++;
        }
        objFile.ignore(MaxLineSize, '\n');
    }
}

int ObjSurface::GetVertexCount() const
{
    if (m_vertexCount != 0) {
        return m_vertexCount;
    }
    countVertexData();
    
    return m_vertexCount;
}

int ObjSurface::GetTexelCount() const {
    if (m_vertexCount != 0) {
        return m_texelCount;
    }
    countVertexData();
    
    return m_texelCount;
}
int ObjSurface::GetTriangleIndexCount() const
{
    if (m_faceCount != 0)
        return m_faceCount * 3;
    countVertexData();

    return m_faceCount * 3;
}

void ObjSurface::GenerateVertices(vector<float>& floats, unsigned char flags) const
{
    //assert(flags == VertexFlagsNormals && "Unsupported flags.");

    struct Vertex {
        vec3 Position;
        vec3 Normal;
        vec2 Texel;
    };

    // Read in the vertex positions and initialize lighting normals to (0, 0, 0).
    int texelCount = GetTexelCount();
    floats.resize(GetVertexCount() * 8);// 6);
    ifstream objFile(m_name.c_str());
    Vertex* vertex = (Vertex*) &floats[0];
    while (objFile) {
        char c = objFile.get();
        if (c == 'v' && objFile.get() == ' ') {
            vertex->Normal = vec3(0, 0, 0);
            vec3& position = (vertex)->Position;
            objFile >> position.x >> position.y >> position.z;
            vec2& texel = (vertex)->Texel;
            texel.x = position.x * .5 + .5;
            texel.y = position.y * -.5 + .5;
            vertex++;
        }
        objFile.ignore(MaxLineSize, '\n');
    }

    vertex = (Vertex*) &floats[0];
    for (size_t faceIndex = 0; faceIndex < m_faces.size(); ++faceIndex) {
        ivec3 face = m_faces[faceIndex].face;
        //Face face = m_faces[faceIndex];

        // Compute the facet normal.
        vec3 a = vertex[face.x].Position;
        vec3 b = vertex[face.y].Position;
        vec3 c = vertex[face.z].Position;
        vec3 facetNormal = (b - a).Cross(c - a);

        // Add the facet normal to the lighting normal of each adjoining vertex.
        vertex[face.x].Normal += facetNormal;
        vertex[face.y].Normal += facetNormal;
        vertex[face.z].Normal += facetNormal;
        
        if (texelCount) {
            ivec3 texi = m_faces[faceIndex].texi;
            vertex[face.x].Texel = m_texels[texi.x];
            vertex[face.y].Texel = m_texels[texi.y];
            vertex[face.z].Texel = m_texels[texi.z];
        }
    }

    // Normalize the normals.
    for (int v = 0; v < GetVertexCount(); ++v)
        vertex[v].Normal.Normalize();
}

void ObjSurface::GenerateTriangleIndices(vector<unsigned short>& indices) const
{
    indices.resize(GetTriangleIndexCount());
    vector<unsigned short>::iterator index = indices.begin();
    for (vector<Face>::const_iterator f = m_faces.begin(); f != m_faces.end(); ++f) {
        *index++ = f->face.x;
        *index++ = f->face.y;
        *index++ = f->face.z;
    }
}

GLuint ObjSurface::BuildShader(const char* source, GLenum shaderType) 
{
    GLuint shaderHandle = glCreateShader(shaderType);
    glShaderSource(shaderHandle, 1, &source, 0);
    glCompileShader(shaderHandle);
    
    GLint compileSuccess;
    glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compileSuccess);
    
    if (compileSuccess == GL_FALSE) {
        GLchar messages[256];
        glGetShaderInfoLog(shaderHandle, sizeof(messages), 0, &messages[0]);
        std::cout << messages;
        exit(1);
    }
    
    return shaderHandle;
}

GLuint ObjSurface::BuildProgram(const char* vertexShaderSource,
                    const char* fragmentShaderSource)
{
    GLuint vertexShader = BuildShader(vertexShaderSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = BuildShader(fragmentShaderSource, GL_FRAGMENT_SHADER);
    
    GLuint programHandle = glCreateProgram();
    glAttachShader(programHandle, vertexShader);
    glAttachShader(programHandle, fragmentShader);
    glLinkProgram(programHandle);
    
    GLint linkSuccess;
    glGetProgramiv(programHandle, GL_LINK_STATUS, &linkSuccess);
    if (linkSuccess == GL_FALSE) {
        GLchar messages[256];
        glGetProgramInfoLog(programHandle, sizeof(messages), 0, &messages[0]);
        std::cout << messages;
        exit(1);
    }
    
    return programHandle;
}

