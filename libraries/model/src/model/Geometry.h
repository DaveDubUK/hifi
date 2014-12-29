//
//  Geometry.h
//  libraries/model/src/model
//
//  Created by Sam Gateau on 12/5/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_model_Geometry_h
#define hifi_model_Geometry_h

#include <glm/glm.hpp>

#include "AABox.h"

#include "gpu/Resource.h"
#include "gpu/Stream.h"

namespace model {
typedef gpu::BufferView::Index Index;
typedef gpu::BufferView BufferView;
typedef AABox Box;
typedef std::vector< Box > Boxes;

class Mesh {
public:
    const static Index PRIMITIVE_RESTART_INDEX = -1;

    typedef gpu::BufferView BufferView;
    typedef std::vector< BufferView > BufferViews;

    typedef gpu::Stream::Slot Slot;
    typedef gpu::Stream::Format VertexFormat;
    typedef std::map< Slot, BufferView > BufferViewMap;

    typedef glm::vec3 Vec3;

    Mesh();
    Mesh(const Mesh& mesh);
    Mesh& operator= (const Mesh& mesh) = default;
    virtual ~Mesh();

    // Vertex buffer
    void setVertexBuffer(const BufferView& buffer);
    const BufferView& getVertexBuffer() const { return _vertexBuffer; }
    uint getNumVertices() const { return _vertexBuffer.getNumElements(); }
    bool hasVertexData() const { return !_vertexBuffer._buffer.isNull(); }

    // Attribute Buffers
    int getNumAttributes() const { return _attributeBuffers.size(); }
    void addAttribute(Slot slot, const BufferView& buffer);

    // Stream format
    const VertexFormat& getVertexFormat() const { return _vertexFormat; }

    // Index Buffer
    void setIndexBuffer(const BufferView& buffer);
    const BufferView& getIndexBuffer() const { return _indexBuffer; }
    uint getNumIndices() const { return _indexBuffer.getNumElements(); }

    // Access vertex position value
    const Vec3& getPos3(Index index) const { return _vertexBuffer.get<Vec3>(index); }

    enum Topology {
        POINTS = 0,
        LINES,
        LINE_STRIP,
        TRIANGLES,
        TRIANGLE_STRIP,
        QUADS,
        QUAD_STRIP,

        NUM_TOPOLOGIES,
    };

    // Subpart of a mesh, describing the toplogy of the surface
    class Part {
    public:
        Index _startIndex;
        Index _numIndices;
        Index _baseVertex;
        Topology _topology;

        Part() :
            _startIndex(0),
            _numIndices(0),
            _baseVertex(0),
            _topology(TRIANGLES)
            {}
        Part(Index startIndex, Index numIndices, Index baseVertex, Topology topology) :
            _startIndex(startIndex),
            _numIndices(numIndices),
            _baseVertex(baseVertex),
            _topology(topology)
            {}
    };

    void setPartBuffer(const BufferView& buffer);
    const BufferView& getPartBuffer() const { return _partBuffer; }
    uint getNumParts() const { return _partBuffer.getNumElements(); }

    // evaluate the bounding box of A part
    const Box evalPartBound(int partNum) const;
    // evaluate the bounding boxes of the parts in the range [start, end[ and fill the bounds parameter
    // the returned box is the bounding box of ALL the evaluated part bounds.
    const Box evalPartBounds(int partStart, int partEnd, Boxes& bounds) const;


    // Generate a BufferStream on the mesh vertices and attributes
    const gpu::BufferStream makeBufferStream() const;

protected:

    VertexFormat _vertexFormat;

    BufferView _vertexBuffer;
    BufferViewMap _attributeBuffers;

    BufferView _indexBuffer;

    BufferView _partBuffer;

    void evalVertexFormat();

};
typedef QSharedPointer< Mesh > MeshPointer;


class Geometry {
public:

    Geometry();
    Geometry(const Geometry& geometry);
    ~Geometry();

    void setMesh(const MeshPointer& mesh);
    const MeshPointer& getMesh() const { return _mesh; }

protected:

    MeshPointer _mesh;
    BufferView _boxes;

};

};

#endif
