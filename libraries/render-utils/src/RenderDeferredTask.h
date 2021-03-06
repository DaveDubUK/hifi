//
//  RenderDeferredTask.h
//  render-utils/src/
//
//  Created by Sam Gateau on 5/29/15.
//  Copyright 20154 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderDeferredTask_h
#define hifi_RenderDeferredTask_h

#include "render/DrawTask.h"

#include "gpu/Pipeline.h"

class PrepareDeferred {
public:
    void run(const render::SceneContextPointer& sceneContext, const render::RenderContextPointer& renderContext);

    typedef render::Job::Model<PrepareDeferred> JobModel;
};


class RenderDeferred {
public:
    void run(const render::SceneContextPointer& sceneContext, const render::RenderContextPointer& renderContext);

    typedef render::Job::Model<RenderDeferred> JobModel;
};

class ResolveDeferred {
public:
    void run(const render::SceneContextPointer& sceneContext, const render::RenderContextPointer& renderContext);

    typedef render::Job::Model<ResolveDeferred> JobModel;
};

class DrawOpaqueDeferred {
public:
    void run(const render::SceneContextPointer& sceneContext, const render::RenderContextPointer& renderContext, const render::ItemIDsBounds& inItems);

    typedef render::Job::ModelI<DrawOpaqueDeferred, render::ItemIDsBounds> JobModel;
};

class DrawTransparentDeferred {
public:
    void run(const render::SceneContextPointer& sceneContext, const render::RenderContextPointer& renderContext, const render::ItemIDsBounds& inItems);

    typedef render::Job::ModelI<DrawTransparentDeferred, render::ItemIDsBounds> JobModel;
};

class DrawOverlay3D {
    mutable gpu::PipelinePointer _opaquePipeline; //lazy evaluation hence mutable
public:
    const gpu::PipelinePointer& getOpaquePipeline() const;
    
    void run(const render::SceneContextPointer& sceneContext, const render::RenderContextPointer& renderContext);

    typedef render::Job::Model<DrawOverlay3D> JobModel;
};

class RenderDeferredTask : public render::Task {
public:

    RenderDeferredTask();
    ~RenderDeferredTask();

    render::Jobs _jobs;

    virtual void run(const render::SceneContextPointer& sceneContext, const render::RenderContextPointer& renderContext);

};


#endif // hifi_RenderDeferredTask_h
