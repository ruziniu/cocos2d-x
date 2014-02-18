#include "Interfaces.h"
#include "CCGL.h"

struct Face {
    ivec3 face;
    ivec3 texi;
};
class ObjSurface : public ISurface {
public:
    ObjSurface(const string& name);
    ~ObjSurface();
    int GetVertexCount(/*int &texInCount*/) const;
    int GetTexelCount() const;
    
    int GetLineIndexCount() const { return 0; }
    int GetTriangleIndexCount() const;
    void GenerateVertices(vector<float>& vertices, unsigned char flags) const;
    void GenerateLineIndices(vector<unsigned short>& indices) const {}
    void GenerateTriangleIndices(vector<unsigned short>& indices) const;
    //void readTexels(std::vector<vec2>& ) const;
    
    static GLuint BuildShader(const char* source, GLenum shaderType) ;
    static GLuint BuildProgram(const char* vertexShaderSource,
                                    const char* fragmentShaderSource);
private:
    void countVertexData() const;
    string m_name;
    //vector<ivec3> m_faces;
    vector<Face> m_faces;
    vector<vec2> m_texels;
    mutable int m_faceCount;
    mutable int m_vertexCount;
    mutable int m_texelCount;
    static const int MaxLineSize = 128;
};
