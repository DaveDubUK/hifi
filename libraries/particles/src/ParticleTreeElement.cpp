//
//  ParticleTreeElement.cpp
//  hifi
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright (c) 2013 HighFidelity, Inc. All rights reserved.
//

#include <GeometryUtil.h>

#include "ParticleTree.h"
#include "ParticleTreeElement.h"

ParticleTreeElement::ParticleTreeElement(unsigned char* octalCode) : OctreeElement(), _particles(NULL) { 
    init(octalCode);
};

ParticleTreeElement::~ParticleTreeElement() {
    _voxelMemoryUsage -= sizeof(ParticleTreeElement);
    delete _particles;
    _particles = NULL;
}

// This will be called primarily on addChildAt(), which means we're adding a child of our
// own type to our own tree. This means we should initialize that child with any tree and type 
// specific settings that our children must have. One example is out VoxelSystem, which
// we know must match ours.
OctreeElement* ParticleTreeElement::createNewElement(unsigned char* octalCode) const {
    ParticleTreeElement* newChild = new ParticleTreeElement(octalCode);
    newChild->setTree(_myTree);
    return newChild;
}

void ParticleTreeElement::init(unsigned char* octalCode) {
    OctreeElement::init(octalCode);
    _particles = new QList<Particle>;
    _voxelMemoryUsage += sizeof(ParticleTreeElement);
}

ParticleTreeElement* ParticleTreeElement::addChildAtIndex(int index) { 
    ParticleTreeElement* newElement = (ParticleTreeElement*)OctreeElement::addChildAtIndex(index); 
    newElement->setTree(_myTree);
    return newElement;
}


bool ParticleTreeElement::appendElementData(OctreePacketData* packetData) const {
    bool success = true; // assume the best...

    // write our particles out...
    uint16_t numberOfParticles = _particles->size();
    success = packetData->appendValue(numberOfParticles);

    if (success) {
        for (uint16_t i = 0; i < numberOfParticles; i++) {
            const Particle& particle = (*_particles)[i];
            success = particle.appendParticleData(packetData);
            if (!success) {
                break;
            }
        }
    }
    return success;
}

void ParticleTreeElement::update(ParticleTreeUpdateArgs& args) {
    markWithChangedTime();
    // TODO: early exit when _particles is empty

    // update our contained particles
    QList<Particle>::iterator particleItr = _particles->begin();
    while(particleItr != _particles->end()) {
        Particle& particle = (*particleItr);
        particle.update(_lastChanged);

        // If the particle wants to die, or if it's left our bounding box, then move it
        // into the arguments moving particles. These will be added back or deleted completely
        if (particle.getShouldDie() || !_box.contains(particle.getPosition())) {
            args._movingParticles.push_back(particle);
            
            // erase this particle
            particleItr = _particles->erase(particleItr);
        }
        else
        {
            ++particleItr;
        }
    }
    // TODO: if _particles is empty after while loop consider freeing memory in _particles if
    // internal array is too big (QList internal array does not decrease size except in dtor and
    // assignment operator).  Otherwise _particles could become a "resource leak" for large
    // roaming piles of particles.
}

bool ParticleTreeElement::findSpherePenetration(const glm::vec3& center, float radius, 
                                    glm::vec3& penetration, void** penetratedObject) const {
                                    
    QList<Particle>::iterator particleItr = _particles->begin();
    QList<Particle>::const_iterator particleEnd = _particles->end();
    while(particleItr != particleEnd) {
        Particle& particle = (*particleItr);
        glm::vec3 particleCenter = particle.getPosition();
        float particleRadius = particle.getRadius();
        
        // don't penetrate yourself
        if (particleCenter == center && particleRadius == radius) {
            return false;
        }

        // We've considered making "inHand" particles not collide, if we want to do that,
        // we should change this setting... but now, we do allow inHand particles to collide
        const bool IN_HAND_PARTICLES_DONT_COLLIDE = false;
        if (IN_HAND_PARTICLES_DONT_COLLIDE) {
            // don't penetrate if the particle is "inHand" -- they don't collide
            if (particle.getInHand()) {
                ++particleItr;
                continue;
            }
        }
        
        if (findSphereSpherePenetration(center, radius, particleCenter, particleRadius, penetration)) {
            // return true on first valid particle penetration
            *penetratedObject = (void*)(&particle);
            return true;
        }
        ++particleItr;
    }
    return false;
}

bool ParticleTreeElement::containsParticle(const Particle& particle) const {
    // TODO: remove this method and force callers to use getParticleWithID() instead
    uint16_t numberOfParticles = _particles->size();
    uint32_t particleID = particle.getID();
    for (uint16_t i = 0; i < numberOfParticles; i++) {
        if ((*_particles)[i].getID() == particleID) {
            return true;
        }
    }
    return false;    
}

bool ParticleTreeElement::updateParticle(const Particle& particle) {
    // NOTE: this method must first lookup the particle by ID, hence it is O(N) 
    // and "particle is not found" is worst-case (full N) but maybe we don't care? 
    // (guaranteed that num particles per elemen is small?)
    const bool wantDebug = false;
    uint16_t numberOfParticles = _particles->size();
    for (uint16_t i = 0; i < numberOfParticles; i++) {
        Particle& thisParticle = (*_particles)[i];
        if (thisParticle.getID() == particle.getID()) {
            int difference = thisParticle.getLastUpdated() - particle.getLastUpdated();
            bool changedOnServer = thisParticle.getLastEdited() < particle.getLastEdited();
            bool localOlder = thisParticle.getLastUpdated() < particle.getLastUpdated();
            if (changedOnServer || localOlder) {                
                if (wantDebug) {
                    printf("local particle [id:%d] %s and %s than server particle by %d, particle.isNewlyCreated()=%s\n", 
                            particle.getID(), (changedOnServer ? "CHANGED" : "same"),
                            (localOlder ? "OLDER" : "NEWER"),               
                            difference, debug::valueOf(particle.isNewlyCreated()) );
                }
                thisParticle.copyChangedProperties(particle);
            } else {
                if (wantDebug) {
                    printf(">>> IGNORING SERVER!!! Would've caused jutter! <<<  "
                            "local particle [id:%d] %s and %s than server particle by %d, particle.isNewlyCreated()=%s\n", 
                            particle.getID(), (changedOnServer ? "CHANGED" : "same"),
                            (localOlder ? "OLDER" : "NEWER"),               
                            difference, debug::valueOf(particle.isNewlyCreated()) );
                }
            }
            return true;
        }
    }
    return false;    
}

const Particle* ParticleTreeElement::getClosestParticle(glm::vec3 position) const {
    const Particle* closestParticle = NULL;
    float closestParticleDistance = FLT_MAX;
    uint16_t numberOfParticles = _particles->size();
    for (uint16_t i = 0; i < numberOfParticles; i++) {
        float distanceToParticle = glm::distance(position, (*_particles)[i].getPosition());
        if (distanceToParticle < closestParticleDistance) {
            closestParticle = &(*_particles)[i];
        }
    }
    return closestParticle;    
}

const Particle* ParticleTreeElement::getParticleWithID(uint32_t id) const {
    // NOTE: this lookup is O(N) but maybe we don't care? (guaranteed that num particles per elemen is small?)
    const Particle* foundParticle = NULL;
    uint16_t numberOfParticles = _particles->size();
    for (uint16_t i = 0; i < numberOfParticles; i++) {
        if ((*_particles)[i].getID() == id) {
            foundParticle = &(*_particles)[i];
            break;
        }
    }
    return foundParticle;    
}


int ParticleTreeElement::readElementDataFromBuffer(const unsigned char* data, int bytesLeftToRead, 
            ReadBitstreamToTreeParams& args) { 
    
    const unsigned char* dataAt = data;
    int bytesRead = 0;
    uint16_t numberOfParticles = 0;
    int expectedBytesPerParticle = Particle::expectedBytes();

    if (bytesLeftToRead >= sizeof(numberOfParticles)) {
    
        // read our particles in....
        numberOfParticles = *(uint16_t*)dataAt;
        dataAt += sizeof(numberOfParticles);
        bytesLeftToRead -= sizeof(numberOfParticles);
        bytesRead += sizeof(numberOfParticles);
        
        if (bytesLeftToRead >= (numberOfParticles * expectedBytesPerParticle)) {
            for (uint16_t i = 0; i < numberOfParticles; i++) {
                Particle tempParticle;
                int bytesForThisParticle = tempParticle.readParticleDataFromBuffer(dataAt, bytesLeftToRead, args);
                _myTree->storeParticle(tempParticle);
                dataAt += bytesForThisParticle;
                bytesLeftToRead -= bytesForThisParticle;
                bytesRead += bytesForThisParticle;
            }
        }
    }
    
    return bytesRead;
}

// will average a "common reduced LOD view" from the the child elements...
void ParticleTreeElement::calculateAverageFromChildren() {
    // nothing to do here yet...
}

// will detect if children are leaves AND collapsable into the parent node
// and in that case will collapse children and make this node
// a leaf, returns TRUE if all the leaves are collapsed into a 
// single node
bool ParticleTreeElement::collapseChildren() {
    // nothing to do here yet...
    return false;
}


void ParticleTreeElement::storeParticle(const Particle& particle, Node* senderNode) {
    _particles->push_back(particle);
    markWithChangedTime();
}

