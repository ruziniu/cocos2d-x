/****************************************************************************
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2011      Zynga Inc.
Copyright (c) 2013-2014 Chukong Technologies Inc.

Copyright (c) 2011 HKASoftware

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

/*
 Code based on HKTMXTiledMap by HKASoftware http://hkasoftware.com
 Adapted (and rewritten) for cocos2d-x needs
 
 Further info:
 http://www.cocos2d-iphone.org/forums/topic/hktmxtiledmap/

 */
#include "CCTMXLayer2.h"
#include "CCTMXXMLParser.h"
#include "CCTMXTiledMap.h"
#include "CCSprite.h"
#include "CCTextureCache.h"
#include "CCShaderCache.h"
#include "CCGLProgram.h"
#include "ccCArray.h"
#include "CCDirector.h"
#include "renderer/CCRenderer.h"

#include "kazmath/kazmath.h"
#include "kazmath/GL/matrix.h"

NS_CC_BEGIN

// TMXLayer2 - init & alloc & dealloc

TMXLayer2 * TMXLayer2::create(TMXTilesetInfo *tilesetInfo, TMXLayerInfo *layerInfo, TMXMapInfo *mapInfo)
{
    TMXLayer2 *ret = new TMXLayer2();
    if (ret->initWithTilesetInfo(tilesetInfo, layerInfo, mapInfo))
    {
        ret->autorelease();
        return ret;
    }
    return nullptr;
}
bool TMXLayer2::initWithTilesetInfo(TMXTilesetInfo *tilesetInfo, TMXLayerInfo *layerInfo, TMXMapInfo *mapInfo)
{    

    if( tilesetInfo )
    {
        _texture = Director::getInstance()->getTextureCache()->addImage(tilesetInfo->_sourceImage);
        _texture->retain();
    }

    // layerInfo
    _layerName = layerInfo->_name;
    _layerSize = layerInfo->_layerSize;
    _tiles = layerInfo->_tiles;
    setOpacity( layerInfo->_opacity );
    setProperties(layerInfo->getProperties());

    // tilesetInfo
    _tileSet = tilesetInfo;
    CC_SAFE_RETAIN(_tileSet);

    // mapInfo
    _mapTileSize = mapInfo->getTileSize();
    _layerOrientation = mapInfo->getOrientation();

    // offset (after layer orientation is set);
    Point offset = this->calculateLayerOffset(layerInfo->_offset);
    this->setPosition(CC_POINT_PIXELS_TO_POINTS(offset));

    this->setContentSize(CC_SIZE_PIXELS_TO_POINTS(Size(_layerSize.width * _mapTileSize.width, _layerSize.height * _mapTileSize.height)));

    // shader, and other stuff
    setShaderProgram(ShaderCache::getInstance()->getProgram(GLProgram::SHADER_NAME_POSITION_TEXTURE_COLOR));

    return true;
}

TMXLayer2::TMXLayer2()
:_layerName("")
,_layerSize(Size::ZERO)
,_mapTileSize(Size::ZERO)
,_tiles(nullptr)
,_tileSet(nullptr)
,_layerOrientation(TMXOrientationOrtho)
,_lastPosition(Point(-1000,-1000))
,_firstTileToDraw(100000)
,_lastTileToDraw(0)
{}

TMXLayer2::~TMXLayer2()
{
    CC_SAFE_RELEASE(_tileSet);
    CC_SAFE_RELEASE(_texture);
    CC_SAFE_DELETE_ARRAY(_tiles);
}

void TMXLayer2::draw()
{
    _customCommand.init(_globalZOrder);
    _customCommand.func = CC_CALLBACK_0(TMXLayer2::onDraw, this);
    Director::getInstance()->getRenderer()->addCommand(&_customCommand);
}

void TMXLayer2::onDraw()
{
    GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POSITION | GL::VERTEX_ATTRIB_FLAG_TEX_COORDS);
    GL::bindTexture2D( _texture->getName() );

    // vertices
    glBindBuffer(GL_ARRAY_BUFFER, _buffersVBO[0]);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    // tex coords
    glBindBuffer(GL_ARRAY_BUFFER, _buffersVBO[1]);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORDS, 2, GL_FLOAT, GL_FALSE, 0, NULL);


    Point trans = convertToWorldSpace(Point::ZERO);
    Point baseTile = Point( floor(-trans.x / (_mapTileSize.width)), floor(-trans.y / (_mapTileSize.height)));

    if( !baseTile.equals(_lastPosition) ) {

        Vertex2F *texcoords = (Vertex2F *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        updateTexCoords(baseTile, texcoords);
        _lastPosition = baseTile;
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    getShaderProgram()->use();

    // Draws in "world" coordinates, since the vertices are always displayed.
    _modelViewTransform.mat[12] += (baseTile.x * _mapTileSize.width);
    _modelViewTransform.mat[13] += (baseTile.y * _mapTileSize.height);
    getShaderProgram()->setUniformsForBuiltins(_modelViewTransform);

    glVertexAttrib4f(GLProgram::VERTEX_ATTRIB_COLOR, _displayedColor.r/255.0f, _displayedColor.g/255.0f, _displayedColor.b/255.0f, _displayedOpacity/255.0f);

    // indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffersVBO[2]);

    // draw:
    int toDraw = (_lastTileToDraw - _firstTileToDraw) + 1;

    if(toDraw > 0) {
        glDrawElements(GL_TRIANGLES, toDraw * 6, GL_UNSIGNED_SHORT, (GLvoid*)(_firstTileToDraw*6*sizeof(GLushort)));
        CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1,toDraw*6);
    }


    // cleanup
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

}

uint32_t TMXLayer2::getGID(int x, int y, cocos2d::Point baseTile) const
{
    int tileidx = -1;
    switch (_layerOrientation)
    {
        case TMXOrientationOrtho:
        {
            x += baseTile.x;
            y += baseTile.y;

            if( x < 0 || x >= _layerSize.width
               || y < 0 || y >= _layerSize.height)
                tileidx = -1;
            else
                tileidx = (_layerSize.height - 1 - y) * _layerSize.width + x;
            break;
        }

        case TMXOrientationIso:
        {
            x += baseTile.x;
            y -= baseTile.y*2;

            int xx = x + y/2;
            int yy = (y+1)/2 - x;

            if( xx < 0 || xx >= _layerSize.width
               || yy < 0 || yy >= _layerSize.height )
                tileidx = -1;
            else
                tileidx = xx + _layerSize.width * yy;
            break;
        }

        default:
        {
            log("unsupported orientation format");
            CCASSERT(false, "ouch");
            break;
        }
    }

    if(tileidx == -1)
        return -1;
    return _tiles[tileidx];
}

void TMXLayer2::updateTexCoords(const Point& baseTile, Vertex2F *texcoords)
{
    _firstTileToDraw = _screenTileCount;
    _lastTileToDraw = 0;

    Size texSize = _tileSet->_imageSize;
    for (int y=0; y < _screenGridSize.height; y++)
    {
        for (int x=0; x < _screenGridSize.width; x++)
        {
            uint32_t tileGID = getGID(x, y, baseTile);

            // vertices are sorted in a way, that top tiles are drawn before bottom tiles, so we need to invert 'y'
            int screenidx = (_screenGridSize.height - 1 - y) * _screenGridSize.width + x;
            // 4: quad,   2: x,y
            Vertex2F *texbase = texcoords + screenidx * 4;

            float left, right, top, bottom;

            // GID==0 empty tile
            if(tileGID==0)
            {
                left = bottom = 0;
                top = right = 0;
            }
            else
            {
                _firstTileToDraw = std::min(screenidx, _firstTileToDraw);
                _lastTileToDraw = std::max(screenidx, _lastTileToDraw);

                Rect tileTexture = _tileSet->getRectForGID(tileGID);

                left   = (tileTexture.origin.x / texSize.width);
                right  = left + (tileTexture.size.width / texSize.width);
                bottom = (tileTexture.origin.y / texSize.height);
                top    = bottom + (tileTexture.size.height / texSize.height);
            }

            if (tileGID & kTMXTileVerticalFlag)
                std::swap(top,bottom);

            if (tileGID & kTMXTileHorizontalFlag)
                std::swap(left,right);

            if (tileGID & kTMXTileDiagonalFlag)
            {
                texbase[0].x = left;
                texbase[0].y = top;
                texbase[1].x = left;
                texbase[1].y = bottom;
                texbase[2].x = right;
                texbase[2].y = top;
                texbase[3].x = right;
                texbase[3].y = bottom;
            }
            else
            {
                texbase[0].x = left;
                texbase[0].y = top;
                texbase[1].x = right;
                texbase[1].y = top;
                texbase[2].x = left;
                texbase[2].y = bottom;
                texbase[3].x = right;
                texbase[3].y = bottom;
            }
        }
    }
}

void TMXLayer2::setupIndices()
{
    GLushort *indices = (GLushort *)malloc( _screenTileCount * 6 * sizeof(GLushort) );

    for( int i=0; i < _screenTileCount; i++)
    {
        indices[i*6+0] = i*4+0;
        indices[i*6+1] = i*4+1;
        indices[i*6+2] = i*4+2;

        indices[i*6+3] = i*4+3;
        indices[i*6+4] = i*4+2;
        indices[i*6+5] = i*4+1;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffersVBO[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * _screenTileCount * 6, indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    free(indices);
}

void TMXLayer2::setupVertices()
{
    Vertex2F *vertices = (Vertex2F *)malloc( _screenTileCount * 4 * sizeof(Vertex2F) );

    int i=0;
    // top to bottom sorting to support overlapping
    for (int y=_screenGridSize.height-1; y >=0; y--)
    {
        for (int x=0; x < _screenGridSize.width; x++)
        {
            Vertex2F pos0, pos1;

            setVerticesForPos(x, y, &pos0, &pos1);

            // define the points of a quad here; we'll use the index buffer to make them triangles
            vertices[i+0].x = pos0.x;
            vertices[i+0].y = pos0.y;
            vertices[i+1].x = pos1.x;
            vertices[i+1].y = pos0.y;
            vertices[i+2].x = pos0.x;
            vertices[i+2].y = pos1.y;
            vertices[i+3].x = pos1.x;
            vertices[i+3].y = pos1.y;
            i += 4;
        }
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffersVBO[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(vertices[0]) * _screenTileCount * 4, vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    free(vertices);
}

void TMXLayer2::setVerticesForPos(int x, int y, Vertex2F *pos0, Vertex2F *pos1)
{
    Vertex2F tmp0, tmp1;
    switch (_layerOrientation)
    {
        case TMXOrientationOrtho:
            tmp0.x = _mapTileSize.width * x;
            tmp1.x = tmp0.x + _tileSet->_tileSize.width;
            tmp0.y = _mapTileSize.height * y;
            tmp1.y = tmp0.y + _tileSet->_tileSize.height;
            break;
        case TMXOrientationIso:
            tmp0.x = _mapTileSize.width * x - _mapTileSize.width/2 * (y%2);
            tmp1.x = tmp0.x + _tileSet->_tileSize.width;
            tmp0.y = _mapTileSize.height * (y-1) / 2;
            tmp1.y = tmp0.y + _tileSet->_tileSize.height;
            break;
        case TMXOrientationHex:
            break;
    }
    *pos0 = tmp0;
    *pos1 = tmp1;
}

void TMXLayer2::setupVBO()
{
    glGenBuffers(3, &_buffersVBO[0]);

    // Vertex
    setupVertices();

    // Tex Coords
    glBindBuffer(GL_ARRAY_BUFFER, _buffersVBO[1]);
    glBufferData(GL_ARRAY_BUFFER, _screenTileCount * 4 * sizeof(Vertex2F), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Indices
    setupIndices();

    CHECK_GL_ERROR_DEBUG();
}

// TMXLayer2 - setup Tiles
void TMXLayer2::setupTiles()
{    
    // Optimization: quick hack that sets the image size on the tileset
    _tileSet->_imageSize = _texture->getContentSizeInPixels();

    // By default all the tiles are aliased
    // pros: easier to render
    // cons: difficult to scale / rotate / etc.
    _texture->setAliasTexParameters();

    //CFByteOrder o = CFByteOrderGetCurrent();

    // Parse cocos2d properties
//    this->parseInternalProperties();

    Size screenSize = Director::getInstance()->getWinSize();

    switch (_layerOrientation)
    {
        case TMXOrientationOrtho:
            _screenGridSize.width = ceil(screenSize.width / _mapTileSize.width) + 1;
            _screenGridSize.height = ceil(screenSize.height / _mapTileSize.height) + 2;
            break;
        case TMXOrientationIso:
            _screenGridSize.width = ceil(screenSize.width / _mapTileSize.width) + 2;
            _screenGridSize.height = ceil(screenSize.height / (_mapTileSize.height/2)) + 4;
            break;
        case TMXOrientationHex:
            break;
    }

    _screenTileCount = _screenGridSize.width * _screenGridSize.height;

    setupVBO();
}

// removing / getting tiles
Sprite* TMXLayer2::getTileAt(const Point& tileCoordinate)
{
    return nullptr;
}

void TMXLayer2::removeTileAt(const Point& tileCoordinate)
{

}

// TMXLayer2 - Properties
Value TMXLayer2::getProperty(const std::string& propertyName) const
{
    if (_properties.find(propertyName) != _properties.end())
        return _properties.at(propertyName);
    
    return Value();
}

void TMXLayer2::parseInternalProperties()
{
    auto vertexz = getProperty("cc_vertexz");
    if (!vertexz.isNull())
    {
        log("cc_vertexz is not supported in TMXLayer2. Use TMXLayer instead");
    }
}


//CCTMXLayer2 - obtaining positions, offset
Point TMXLayer2::calculateLayerOffset(const Point& pos)
{
    Point ret = Point::ZERO;
    switch (_layerOrientation) 
    {
    case TMXOrientationOrtho:
        ret = Point( pos.x * _mapTileSize.width, -pos.y *_mapTileSize.height);
        break;
    case TMXOrientationIso:
        ret = Point((_mapTileSize.width /2) * (pos.x - pos.y),
                  (_mapTileSize.height /2 ) * (-pos.x - pos.y));
        break;
    case TMXOrientationHex:
        CCASSERT(pos.equals(Point::ZERO), "offset for hexagonal map not implemented yet");
        break;
    }
    return ret;    
}


std::string TMXLayer2::getDescription() const
{
    return StringUtils::format("<TMXLayer2 | tag = %d, size = %d,%d>", _tag, (int)_mapTileSize.width, (int)_mapTileSize.height);
}

NS_CC_END

