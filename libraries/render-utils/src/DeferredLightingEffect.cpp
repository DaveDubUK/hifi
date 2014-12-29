//
//  DeferredLightingEffect.cpp
//  interface/src/renderer
//
//  Created by Andrzej Kapolka on 9/11/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

// include this before QOpenGLFramebufferObject, which includes an earlier version of OpenGL
#include <gpu/GPUConfig.h>

#include <QOpenGLFramebufferObject>

#include <GLMHelpers.h>
#include <PathUtils.h>
#include <ViewFrustum.h>

#include "AbstractViewStateInterface.h"
#include "DeferredLightingEffect.h"
#include "GeometryCache.h"
#include "GlowEffect.h"
#include "RenderUtil.h"
#include "TextureCache.h"


void DeferredLightingEffect::init(AbstractViewStateInterface* viewState) {
    _viewState = viewState;
    _simpleProgram.addShaderFromSourceFile(QGLShader::Vertex, PathUtils::resourcesPath() + "shaders/simple.vert");
    _simpleProgram.addShaderFromSourceFile(QGLShader::Fragment, PathUtils::resourcesPath() + "shaders/simple.frag");
    _simpleProgram.link();
    
    _simpleProgram.bind();
    _glowIntensityLocation = _simpleProgram.uniformLocation("glowIntensity");
    _simpleProgram.release();
    
    loadLightProgram("shaders/directional_light.frag", false, _directionalLight, _directionalLightLocations);
    loadLightProgram("shaders/directional_light_shadow_map.frag", false, _directionalLightShadowMap,
        _directionalLightShadowMapLocations);
    loadLightProgram("shaders/directional_light_cascaded_shadow_map.frag", false, _directionalLightCascadedShadowMap,
        _directionalLightCascadedShadowMapLocations);
    loadLightProgram("shaders/point_light.frag", true, _pointLight, _pointLightLocations);
    loadLightProgram("shaders/spot_light.frag", true, _spotLight, _spotLightLocations);
}

void DeferredLightingEffect::bindSimpleProgram() {
    DependencyManager::get<TextureCache>()->setPrimaryDrawBuffers(true, true, true);
    _simpleProgram.bind();
    _simpleProgram.setUniformValue(_glowIntensityLocation, DependencyManager::get<GlowEffect>()->getIntensity());
    glDisable(GL_BLEND);
}

void DeferredLightingEffect::releaseSimpleProgram() {
    glEnable(GL_BLEND);
    _simpleProgram.release();
    DependencyManager::get<TextureCache>()->setPrimaryDrawBuffers(true, false, false);
}

void DeferredLightingEffect::renderSolidSphere(float radius, int slices, int stacks) {
    bindSimpleProgram();
    DependencyManager::get<GeometryCache>()->renderSphere(radius, slices, stacks); 
    releaseSimpleProgram();
}

void DeferredLightingEffect::renderWireSphere(float radius, int slices, int stacks) {
    bindSimpleProgram();
    DependencyManager::get<GeometryCache>()->renderSphere(radius, slices, stacks, false); 
    releaseSimpleProgram();
}

void DeferredLightingEffect::renderSolidCube(float size) {
    bindSimpleProgram();
    DependencyManager::get<GeometryCache>()->renderSolidCube(size); 
    releaseSimpleProgram();
}

void DeferredLightingEffect::renderWireCube(float size) {
    bindSimpleProgram();
    DependencyManager::get<GeometryCache>()->renderWireCube(size);
    releaseSimpleProgram();
}

void DeferredLightingEffect::renderSolidCone(float base, float height, int slices, int stacks) {
    bindSimpleProgram();
    DependencyManager::get<GeometryCache>()->renderCone(base, height, slices, stacks);
    releaseSimpleProgram();
}

void DeferredLightingEffect::addPointLight(const glm::vec3& position, float radius, const glm::vec3& ambient,
        const glm::vec3& diffuse, const glm::vec3& specular, float constantAttenuation,
        float linearAttenuation, float quadraticAttenuation) {
    addSpotLight(position, radius, ambient, diffuse, specular, constantAttenuation, linearAttenuation, quadraticAttenuation);    
}

void DeferredLightingEffect::addSpotLight(const glm::vec3& position, float radius, const glm::vec3& ambient,
        const glm::vec3& diffuse, const glm::vec3& specular, float constantAttenuation, float linearAttenuation,
        float quadraticAttenuation, const glm::vec3& direction, float exponent, float cutoff) {
    if (exponent == 0.0f && cutoff == PI) {
        PointLight light;
        light.position = glm::vec4(position, 1.0f);
        light.radius = radius;
        light.ambient = glm::vec4(ambient, 1.0f);
        light.diffuse = glm::vec4(diffuse, 1.0f);
        light.specular = glm::vec4(specular, 1.0f);
        light.constantAttenuation = constantAttenuation;
        light.linearAttenuation = linearAttenuation;
        _pointLights.append(light);
        
    } else {
        SpotLight light;
        light.position = glm::vec4(position, 1.0f);
        light.radius = radius;
        light.ambient = glm::vec4(ambient, 1.0f);
        light.diffuse = glm::vec4(diffuse, 1.0f);
        light.specular = glm::vec4(specular, 1.0f);
        light.constantAttenuation = constantAttenuation;
        light.linearAttenuation = linearAttenuation;
        light.direction = direction;
        light.exponent = exponent;
        light.cutoff = cutoff;
        _spotLights.append(light);
    }
}

void DeferredLightingEffect::prepare() {
    // clear the normal and specular buffers
    TextureCache::SharedPointer textureCache = DependencyManager::get<TextureCache>();
    textureCache->setPrimaryDrawBuffers(false, true, false);
    glClear(GL_COLOR_BUFFER_BIT);
    textureCache->setPrimaryDrawBuffers(false, false, true);
    // clearing to zero alpha for specular causes problems on my Nvidia card; clear to lowest non-zero value instead
    const float MAX_SPECULAR_EXPONENT = 128.0f;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f / MAX_SPECULAR_EXPONENT);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    textureCache->setPrimaryDrawBuffers(true, false, false);
}

void DeferredLightingEffect::render() {
    // perform deferred lighting, rendering to free fbo
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);    
    
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_COLOR_MATERIAL);
    glDepthMask(false);

    TextureCache::SharedPointer textureCache = DependencyManager::get<TextureCache>();
    
    QOpenGLFramebufferObject* primaryFBO = textureCache->getPrimaryFramebufferObject();
    primaryFBO->release();
    
    QOpenGLFramebufferObject* freeFBO = DependencyManager::get<GlowEffect>()->getFreeFramebufferObject();
    freeFBO->bind();
    glClear(GL_COLOR_BUFFER_BIT);
    
    glBindTexture(GL_TEXTURE_2D, primaryFBO->texture());
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureCache->getPrimaryNormalTextureID());
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textureCache->getPrimarySpecularTextureID());
    
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, textureCache->getPrimaryDepthTextureID());
        
    // get the viewport side (left, right, both)
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int VIEWPORT_X_INDEX = 0;
    const int VIEWPORT_Y_INDEX = 1;
    const int VIEWPORT_WIDTH_INDEX = 2;
    const int VIEWPORT_HEIGHT_INDEX = 3;
    float sMin = viewport[VIEWPORT_X_INDEX] / (float)primaryFBO->width();
    float sWidth = viewport[VIEWPORT_WIDTH_INDEX] / (float)primaryFBO->width();
    float tMin = viewport[VIEWPORT_Y_INDEX] / (float)primaryFBO->height();
    float tHeight = viewport[VIEWPORT_HEIGHT_INDEX] / (float)primaryFBO->height();
   
    ProgramObject* program = &_directionalLight;
    const LightLocations* locations = &_directionalLightLocations;
    bool shadowsEnabled = _viewState->getShadowsEnabled();
    if (shadowsEnabled) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, textureCache->getShadowDepthTextureID());
        
        program = &_directionalLightShadowMap;
        locations = &_directionalLightShadowMapLocations;
        if (_viewState->getCascadeShadowsEnabled()) {
            program = &_directionalLightCascadedShadowMap;
            locations = &_directionalLightCascadedShadowMapLocations;
            _directionalLightCascadedShadowMap.bind();
            _directionalLightCascadedShadowMap.setUniform(locations->shadowDistances, _viewState->getShadowDistances());
        
        } else {
            program->bind();
        }
        program->setUniformValue(locations->shadowScale,
            1.0f / textureCache->getShadowFramebufferObject()->width());
        
    } else {
        program->bind();
    }
    
    float left, right, bottom, top, nearVal, farVal;
    glm::vec4 nearClipPlane, farClipPlane;
    _viewState->computeOffAxisFrustum(left, right, bottom, top, nearVal, farVal, nearClipPlane, farClipPlane);
    program->setUniformValue(locations->nearLocation, nearVal);
    float depthScale = (farVal - nearVal) / farVal;
    program->setUniformValue(locations->depthScale, depthScale);
    float nearScale = -1.0f / nearVal;
    float depthTexCoordScaleS = (right - left) * nearScale / sWidth;
    float depthTexCoordScaleT = (top - bottom) * nearScale / tHeight;
    float depthTexCoordOffsetS = left * nearScale - sMin * depthTexCoordScaleS;
    float depthTexCoordOffsetT = bottom * nearScale - tMin * depthTexCoordScaleT;
    program->setUniformValue(locations->depthTexCoordOffset, depthTexCoordOffsetS, depthTexCoordOffsetT);
    program->setUniformValue(locations->depthTexCoordScale, depthTexCoordScaleS, depthTexCoordScaleT);
    
    renderFullscreenQuad(sMin, sMin + sWidth, tMin, tMin + tHeight);
    
    program->release();
    
    if (shadowsEnabled) {
        glBindTexture(GL_TEXTURE_2D, 0);        
        glActiveTexture(GL_TEXTURE3);
    }
    
    // additive blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    
    glEnable(GL_CULL_FACE);
    
    glm::vec4 sCoefficients(sWidth / 2.0f, 0.0f, 0.0f, sMin + sWidth / 2.0f);
    glm::vec4 tCoefficients(0.0f, tHeight / 2.0f, 0.0f, tMin + tHeight / 2.0f);
    glTexGenfv(GL_S, GL_OBJECT_PLANE, (const GLfloat*)&sCoefficients);
    glTexGenfv(GL_T, GL_OBJECT_PLANE, (const GLfloat*)&tCoefficients);
    
    // enlarge the scales slightly to account for tesselation
    const float SCALE_EXPANSION = 0.05f;
    
    const glm::vec3& eyePoint = _viewState->getCurrentViewFrustum()->getPosition();
    float nearRadius = glm::distance(eyePoint, _viewState->getCurrentViewFrustum()->getNearTopLeft());

    GeometryCache::SharedPointer geometryCache = DependencyManager::get<GeometryCache>();
    
    if (!_pointLights.isEmpty()) {
        _pointLight.bind();
        _pointLight.setUniformValue(_pointLightLocations.nearLocation, nearVal);
        _pointLight.setUniformValue(_pointLightLocations.depthScale, depthScale);
        _pointLight.setUniformValue(_pointLightLocations.depthTexCoordOffset, depthTexCoordOffsetS, depthTexCoordOffsetT);
        _pointLight.setUniformValue(_pointLightLocations.depthTexCoordScale, depthTexCoordScaleS, depthTexCoordScaleT);

        foreach (const PointLight& light, _pointLights) {
            _pointLight.setUniformValue(_pointLightLocations.radius, light.radius);
            glLightfv(GL_LIGHT1, GL_AMBIENT, (const GLfloat*)&light.ambient);
            glLightfv(GL_LIGHT1, GL_DIFFUSE, (const GLfloat*)&light.diffuse);
            glLightfv(GL_LIGHT1, GL_SPECULAR, (const GLfloat*)&light.specular);
            glLightfv(GL_LIGHT1, GL_POSITION, (const GLfloat*)&light.position);
            glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, (light.constantAttenuation > 0.0f ? light.constantAttenuation : 0.0f));
            glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, (light.linearAttenuation > 0.0f ? light.linearAttenuation : 0.0f));
            glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, (light.quadraticAttenuation > 0.0f ? light.quadraticAttenuation : 0.0f));
         
            glPushMatrix();
            
            float expandedRadius = light.radius * (1.0f + SCALE_EXPANSION);
            if (glm::distance(eyePoint, glm::vec3(light.position)) < expandedRadius + nearRadius) {
                glLoadIdentity();
                glTranslatef(0.0f, 0.0f, -1.0f);
                
                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                
                renderFullscreenQuad();
            
                glPopMatrix();
                glMatrixMode(GL_MODELVIEW);
                
            } else {
                glTranslatef(light.position.x, light.position.y, light.position.z);   
                geometryCache->renderSphere(expandedRadius, 32, 32);
            }
            
            glPopMatrix();
        }
        _pointLights.clear();
        
        _pointLight.release();
    }
    
    if (!_spotLights.isEmpty()) {
        _spotLight.bind();
        _spotLight.setUniformValue(_spotLightLocations.nearLocation, nearVal);
        _spotLight.setUniformValue(_spotLightLocations.depthScale, depthScale);
        _spotLight.setUniformValue(_spotLightLocations.depthTexCoordOffset, depthTexCoordOffsetS, depthTexCoordOffsetT);
        _spotLight.setUniformValue(_spotLightLocations.depthTexCoordScale, depthTexCoordScaleS, depthTexCoordScaleT);
        
        foreach (const SpotLight& light, _spotLights) {
            _spotLight.setUniformValue(_spotLightLocations.radius, light.radius);
            glLightfv(GL_LIGHT1, GL_AMBIENT, (const GLfloat*)&light.ambient);
            glLightfv(GL_LIGHT1, GL_DIFFUSE, (const GLfloat*)&light.diffuse);
            glLightfv(GL_LIGHT1, GL_SPECULAR, (const GLfloat*)&light.specular);
            glLightfv(GL_LIGHT1, GL_POSITION, (const GLfloat*)&light.position);
            glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, (light.constantAttenuation > 0.0f ? light.constantAttenuation : 0.0f));
            glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, (light.linearAttenuation > 0.0f ? light.linearAttenuation : 0.0f));
            glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, (light.quadraticAttenuation > 0.0f ? light.quadraticAttenuation : 0.0f));
            glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, (const GLfloat*)&light.direction);
            glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, light.exponent);
            glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, glm::degrees(light.cutoff));
            
            glPushMatrix();
            
            float expandedRadius = light.radius * (1.0f + SCALE_EXPANSION);
            float edgeRadius = expandedRadius / glm::cos(light.cutoff);
            if (glm::distance(eyePoint, glm::vec3(light.position)) < edgeRadius + nearRadius) {
                glLoadIdentity();
                glTranslatef(0.0f, 0.0f, -1.0f);
                
                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                
                renderFullscreenQuad();
                
                glPopMatrix();
                glMatrixMode(GL_MODELVIEW);
                
            } else {
                glTranslatef(light.position.x, light.position.y, light.position.z);
                glm::quat spotRotation = rotationBetween(glm::vec3(0.0f, 0.0f, -1.0f), light.direction);
                glm::vec3 axis = glm::axis(spotRotation);
                glRotatef(glm::degrees(glm::angle(spotRotation)), axis.x, axis.y, axis.z);   
                glTranslatef(0.0f, 0.0f, -light.radius * (1.0f + SCALE_EXPANSION * 0.5f));  
                geometryCache->renderCone(expandedRadius * glm::tan(light.cutoff),
                    expandedRadius, 32, 1);
            }
            
            glPopMatrix();
        }
        _spotLights.clear();
        
        _spotLight.release();
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
        
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    freeFBO->release();
    
    glDisable(GL_CULL_FACE);
    
    // now transfer the lit region to the primary fbo
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_CONSTANT_ALPHA, GL_ONE);
    glColorMask(true, true, true, false);
    
    primaryFBO->bind();
    
    glBindTexture(GL_TEXTURE_2D, freeFBO->texture());
    glEnable(GL_TEXTURE_2D);
    
    glPushMatrix();
    glLoadIdentity();
    
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    
    renderFullscreenQuad(sMin, sMin + sWidth, tMin, tMin + tHeight);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    
    glColorMask(true, true, true, true);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(true);
    
    glPopMatrix();
    
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    // now render the objects we held back until after deferred lighting
    foreach (PostLightingRenderable* renderable, _postLightingRenderables) {
        renderable->renderPostLighting();
    }
    _postLightingRenderables.clear();
}

void DeferredLightingEffect::loadLightProgram(const char* name, bool limited, ProgramObject& program, LightLocations& locations) {
    program.addShaderFromSourceFile(QGLShader::Vertex, PathUtils::resourcesPath() +
        (limited ? "shaders/deferred_light_limited.vert" : "shaders/deferred_light.vert"));
    program.addShaderFromSourceFile(QGLShader::Fragment, PathUtils::resourcesPath() + name);
    program.link();
    
    program.bind();
    program.setUniformValue("diffuseMap", 0);
    program.setUniformValue("normalMap", 1);
    program.setUniformValue("specularMap", 2);
    program.setUniformValue("depthMap", 3);
    program.setUniformValue("shadowMap", 4);
    locations.shadowDistances = program.uniformLocation("shadowDistances");
    locations.shadowScale = program.uniformLocation("shadowScale");
    locations.nearLocation = program.uniformLocation("near");
    locations.depthScale = program.uniformLocation("depthScale");
    locations.depthTexCoordOffset = program.uniformLocation("depthTexCoordOffset");
    locations.depthTexCoordScale = program.uniformLocation("depthTexCoordScale");
    locations.radius = program.uniformLocation("radius");
    program.release();
}
