//
//  RenderableTextEntityItem.cpp
//  interface/src
//
//  Created by Brad Hefta-Gaub on 8/6/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <glm/gtx/quaternion.hpp>

#include <gpu/GPUConfig.h>

#include <DeferredLightingEffect.h>
#include <GeometryCache.h>
#include <PerfStat.h>

#include "RenderableTextEntityItem.h"
#include "GLMHelpers.h"

EntityItemPointer RenderableTextEntityItem::factory(const EntityItemID& entityID, const EntityItemProperties& properties) {
    return EntityItemPointer(new RenderableTextEntityItem(entityID, properties));
}

void RenderableTextEntityItem::render(RenderArgs* args) {
    PerformanceTimer perfTimer("RenderableTextEntityItem::render");
    Q_ASSERT(getType() == EntityTypes::Text);
    
    static const float SLIGHTLY_BEHIND = -0.005f;
    glm::vec4 textColor = glm::vec4(toGlm(getTextColorX()), 1.0f);
    glm::vec4 backgroundColor = glm::vec4(toGlm(getBackgroundColorX()), 1.0f);
    glm::vec3 dimensions = getDimensions();
    
    Transform transformToTopLeft = getTransformToCenter();
    transformToTopLeft.postTranslate(glm::vec3(-0.5f, 0.5f, 0.0f)); // Go to the top left
    transformToTopLeft.setScale(1.0f); // Use a scale of one so that the text is not deformed
    
    // Batch render calls
    Q_ASSERT(args->_batch);
    gpu::Batch& batch = *args->_batch;
    batch.setModelTransform(transformToTopLeft);
    
    // Render background
    glm::vec3 minCorner = glm::vec3(0.0f, -dimensions.y, SLIGHTLY_BEHIND);
    glm::vec3 maxCorner = glm::vec3(dimensions.x, 0.0f, SLIGHTLY_BEHIND);
    DependencyManager::get<DeferredLightingEffect>()->renderQuad(batch, minCorner, maxCorner, backgroundColor);
    
    float scale = _lineHeight / _textRenderer->getFontSize();
    transformToTopLeft.setScale(scale); // Scale to have the correct line height
    batch.setModelTransform(transformToTopLeft);
    
    float leftMargin = 0.1f * _lineHeight, topMargin = 0.1f * _lineHeight;
    glm::vec2 bounds = glm::vec2(dimensions.x - 2.0f * leftMargin,
                                 dimensions.y - 2.0f * topMargin);
    _textRenderer->draw(batch, leftMargin / scale, -topMargin / scale, _text, textColor, bounds / scale);
}



