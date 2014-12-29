//
//  GeometryCache.h
//  interface/src/renderer
//
//  Created by Andrzej Kapolka on 6/21/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_GeometryCache_h
#define hifi_GeometryCache_h

// include this before QOpenGLBuffer, which includes an earlier version of OpenGL
#include <gpu/GPUConfig.h>

#include <QMap>
#include <QOpenGLBuffer>

#include <DependencyManager.h>
#include <ResourceCache.h>

#include <FBXReader.h>

#include <AnimationCache.h>

#include "gpu/Stream.h"

class NetworkGeometry;
class NetworkMesh;
class NetworkTexture;

/// Stores cached geometry.
class GeometryCache : public ResourceCache  {
    Q_OBJECT
    SINGLETON_DEPENDENCY(GeometryCache)

public:
    void renderHemisphere(int slices, int stacks);
    void renderSphere(float radius, int slices, int stacks, bool solid = true);
    void renderSquare(int xDivisions, int yDivisions);
    void renderHalfCylinder(int slices, int stacks);
    void renderCone(float base, float height, int slices, int stacks);
    void renderGrid(int xDivisions, int yDivisions);
    void renderSolidCube(float size);
    void renderWireCube(float size);

    /// Loads geometry from the specified URL.
    /// \param fallback a fallback URL to load if the desired one is unavailable
    /// \param delayLoad if true, don't load the geometry immediately; wait until load is first requested
    QSharedPointer<NetworkGeometry> getGeometry(const QUrl& url, const QUrl& fallback = QUrl(), bool delayLoad = false);

protected:

    virtual QSharedPointer<Resource> createResource(const QUrl& url,
        const QSharedPointer<Resource>& fallback, bool delayLoad, const void* extra);
        
private:
    GeometryCache();
    virtual ~GeometryCache();
    
    typedef QPair<int, int> IntPair;
    typedef QPair<GLuint, GLuint> VerticesIndices;
    
    QHash<IntPair, VerticesIndices> _hemisphereVBOs;
    QHash<IntPair, VerticesIndices> _sphereVBOs;
    QHash<IntPair, VerticesIndices> _squareVBOs;
    QHash<IntPair, VerticesIndices> _halfCylinderVBOs;
    QHash<IntPair, VerticesIndices> _coneVBOs;
    QHash<float, VerticesIndices> _wireCubeVBOs;
    QHash<float, VerticesIndices> _solidCubeVBOs;
    QHash<IntPair, QOpenGLBuffer> _gridBuffers;
    
    QHash<QUrl, QWeakPointer<NetworkGeometry> > _networkGeometry;
};

/// Geometry loaded from the network.
class NetworkGeometry : public Resource {
    Q_OBJECT

public:
    
    /// A hysteresis value indicating that we have no state memory.
    static const float NO_HYSTERESIS;
    
    NetworkGeometry(const QUrl& url, const QSharedPointer<NetworkGeometry>& fallback, bool delayLoad,
        const QVariantHash& mapping = QVariantHash(), const QUrl& textureBase = QUrl());

    /// Checks whether the geometry and its textures are loaded.
    bool isLoadedWithTextures() const;

    /// Returns a pointer to the geometry appropriate for the specified distance.
    /// \param hysteresis a hysteresis parameter that prevents rapid model switching
    QSharedPointer<NetworkGeometry> getLODOrFallback(float distance, float& hysteresis, bool delayLoad = false) const;

    const FBXGeometry& getFBXGeometry() const { return _geometry; }
    const QVector<NetworkMesh>& getMeshes() const { return _meshes; }

    QVector<int> getJointMappings(const AnimationPointer& animation);

    virtual void setLoadPriority(const QPointer<QObject>& owner, float priority);
    virtual void setLoadPriorities(const QHash<QPointer<QObject>, float>& priorities);
    virtual void clearLoadPriority(const QPointer<QObject>& owner);
    
    void setTextureWithNameToURL(const QString& name, const QUrl& url);
    QStringList getTextureNames() const;
        
protected:

    virtual void init();
    virtual void downloadFinished(QNetworkReply* reply);
    virtual void reinsert();
    
    Q_INVOKABLE void setGeometry(const FBXGeometry& geometry);
    
private slots:
    void replaceTexturesWithPendingChanges();
private:
    
    friend class GeometryCache;
    
    void setLODParent(const QWeakPointer<NetworkGeometry>& lodParent) { _lodParent = lodParent; }
    
    QVariantHash _mapping;
    QUrl _textureBase;
    QSharedPointer<NetworkGeometry> _fallback;
    
    QMap<float, QSharedPointer<NetworkGeometry> > _lods;
    FBXGeometry _geometry;
    QVector<NetworkMesh> _meshes;
    
    QWeakPointer<NetworkGeometry> _lodParent;
    
    QHash<QWeakPointer<Animation>, QVector<int> > _jointMappings;
    
    QHash<QString, QUrl> _pendingTextureChanges;
};

/// The state associated with a single mesh part.
class NetworkMeshPart {
public: 
    
    QString diffuseTextureName;
    QSharedPointer<NetworkTexture> diffuseTexture;
    QString normalTextureName;
    QSharedPointer<NetworkTexture> normalTexture;
    QString specularTextureName;
    QSharedPointer<NetworkTexture> specularTexture;
    QString emissiveTextureName;
    QSharedPointer<NetworkTexture> emissiveTexture;

    bool isTranslucent() const;
};

/// The state associated with a single mesh.
class NetworkMesh {
public:
    gpu::BufferPointer _indexBuffer;
    gpu::BufferPointer _vertexBuffer;

    gpu::BufferStreamPointer _vertexStream;

    gpu::Stream::FormatPointer _vertexFormat;
    
    QVector<NetworkMeshPart> parts;
    
    int getTranslucentPartCount(const FBXMesh& fbxMesh) const;
};

#endif // hifi_GeometryCache_h
