//
//  ObjectActionSpring.cpp
//  libraries/physics/src
//
//  Created by Seth Alves 2015-6-5
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ObjectActionSpring.h"

ObjectActionSpring::ObjectActionSpring(QUuid id, EntityItemPointer ownerEntity) :
    ObjectAction(id, ownerEntity) {
    #if WANT_DEBUG
    qDebug() << "ObjectActionSpring::ObjectActionSpring";
    #endif
}

ObjectActionSpring::~ObjectActionSpring() {
    #if WANT_DEBUG
    qDebug() << "ObjectActionSpring::~ObjectActionSpring";
    #endif
}

void ObjectActionSpring::updateActionWorker(btScalar deltaTimeStep) {
    if (!tryLockForRead()) {
        // don't risk hanging the thread running the physics simulation
        qDebug() << "ObjectActionSpring::updateActionWorker lock failed";
        return;
    }

    void* physicsInfo = _ownerEntity->getPhysicsInfo();
    if (!physicsInfo) {
        unlock();
        return;
    }
    ObjectMotionState* motionState = static_cast<ObjectMotionState*>(physicsInfo);
    btRigidBody* rigidBody = motionState->getRigidBody();
    if (!rigidBody) {
        unlock();
        qDebug() << "ObjectActionSpring::updateActionWorker no rigidBody";
        return;
    }

    // handle the linear part
    if (_positionalTargetSet) {
        glm::vec3 offset = _positionalTarget - bulletToGLM(rigidBody->getCenterOfMassPosition());
        float offsetLength = glm::length(offset);
        float speed = offsetLength / _linearTimeScale;

        if (offsetLength > IGNORE_POSITION_DELTA) {
            glm::vec3 newVelocity = glm::normalize(offset) * speed;
            rigidBody->setLinearVelocity(glmToBullet(newVelocity));
            rigidBody->activate();
        } else {
            rigidBody->setLinearVelocity(glmToBullet(glm::vec3(0.0f)));
        }
    }

    // handle rotation
    if (_rotationalTargetSet) {
        glm::quat bodyRotation = bulletToGLM(rigidBody->getOrientation());
        // if qZero and qOne are too close to each other, we can get NaN for angle.
        auto alignmentDot = glm::dot(bodyRotation, _rotationalTarget);
        const float almostOne = 0.99999f;
        if (glm::abs(alignmentDot) < almostOne) {
            glm::quat target = _rotationalTarget;
            if (alignmentDot < 0) {
                target = -target;
            }
            glm::quat qZeroInverse = glm::inverse(bodyRotation);
            glm::quat deltaQ = target * qZeroInverse;
            glm::vec3 axis = glm::axis(deltaQ);
            float angle = glm::angle(deltaQ);
            assert(!isNaN(angle));
            glm::vec3 newAngularVelocity = (angle / _angularTimeScale) * glm::normalize(axis);
            rigidBody->setAngularVelocity(glmToBullet(newAngularVelocity));
            rigidBody->activate();
        } else {
            rigidBody->setAngularVelocity(glmToBullet(glm::vec3(0.0f)));
        }
    }

    unlock();
}


bool ObjectActionSpring::updateArguments(QVariantMap arguments) {
    // targets are required, spring-constants are optional
    bool ptOk = true;
    glm::vec3 positionalTarget =
        EntityActionInterface::extractVec3Argument("spring action", arguments, "targetPosition", ptOk, false);
    bool pscOk = true;
    float linearTimeScale =
        EntityActionInterface::extractFloatArgument("spring action", arguments, "linearTimeScale", pscOk, false);
    if (ptOk && pscOk && linearTimeScale <= 0.0f) {
        qDebug() << "spring action -- linearTimeScale must be greater than zero.";
        return false;
    }

    bool rtOk = true;
    glm::quat rotationalTarget =
        EntityActionInterface::extractQuatArgument("spring action", arguments, "targetRotation", rtOk, false);
    bool rscOk = true;
    float angularTimeScale =
        EntityActionInterface::extractFloatArgument("spring action", arguments, "angularTimeScale", rscOk, false);

    if (!ptOk && !rtOk) {
        qDebug() << "spring action requires either targetPosition or targetRotation argument";
        return false;
    }

    lockForWrite();

    _positionalTargetSet = _rotationalTargetSet = false;

    if (ptOk) {
        _positionalTarget = positionalTarget;
        _positionalTargetSet = true;

        if (pscOk) {
            _linearTimeScale = linearTimeScale;
        } else {
            _linearTimeScale = 0.1f;
        }
    }

    if (rtOk) {
        _rotationalTarget = rotationalTarget;
        _rotationalTargetSet = true;

        if (rscOk) {
            _angularTimeScale = angularTimeScale;
        } else {
            _angularTimeScale = 0.1f;
        }
    }

    _active = true;
    unlock();
    return true;
}
