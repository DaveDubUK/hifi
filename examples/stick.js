//  stick.js
//  examples
//
//  Created by Seth Alves on 2015-6-10
//  Copyright 2015 High Fidelity, Inc.
//
//  Allow avatar to hold a stick
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var hand = "left";
var nullActionID = "00000000-0000-0000-0000-000000000000";
var controllerID;
var controllerActive;
var stickID = null;
var actionID = nullActionID;
// sometimes if this is run immediately the stick doesn't get created?  use a timer.
Script.setTimeout(function() {
    stickID = Entities.addEntity({
        type: "Model",
        modelURL: "https://hifi-public.s3.amazonaws.com/eric/models/stick.fbx",
        compoundShapeURL: "https://hifi-public.s3.amazonaws.com/eric/models/stick.obj",
        dimensions: {x: .11, y: .11, z: 1.0},
        position: MyAvatar.getRightPalmPosition(), // initial position doesn't matter, as long as it's close
        rotation: MyAvatar.orientation,
        damping: .1,
        collisionSoundURL: "http://public.highfidelity.io/sounds/Collisions-hitsandslaps/67LCollision07.wav",
        restitution: 0.01,
        collisionsWillMove: true
    });
    actionID = Entities.addAction("hold", stickID, {relativePosition: {x: 0.0, y: 0.0, z: -0.9},
                                                    hand: hand,
                                                    timeScale: 0.15});
}, 3000);


function cleanUp() {
    Entities.deleteEntity(stickID);
}


function positionStick(stickOrientation) {
    var baseOffset = {x: 0.0, y: 0.0, z: -0.9};
    var offset = Vec3.multiplyQbyV(stickOrientation, baseOffset);
    Entities.updateAction(stickID, actionID, {relativePosition: offset,
                                              relativeRotation: stickOrientation});
}


function mouseMoveEvent(event) {
    if (!stickID || actionID == nullActionID) {
        return;
    }
    var windowCenterX = Window.innerWidth / 2;
    var windowCenterY = Window.innerHeight / 2;
    var mouseXCenterOffset = event.x - windowCenterX;
    var mouseYCenterOffset = event.y - windowCenterY;
    var mouseXRatio = mouseXCenterOffset / windowCenterX;
    var mouseYRatio = mouseYCenterOffset / windowCenterY;

    var stickOrientation = Quat.fromPitchYawRollDegrees(mouseYRatio * -90, mouseXRatio * -90, 0);
    positionStick(stickOrientation);
}


function initControls(){
    if (hand == "right") {
        controllerID = 3; // right handed
    } else {
        controllerID = 4; // left handed
    }
}


function update(deltaTime){
    var palmPosition = Controller.getSpatialControlPosition(controllerID);
    controllerActive = (Vec3.length(palmPosition) > 0);
    if(!controllerActive){
        return;
    }

    stickOrientation = Controller.getSpatialControlRawRotation(controllerID);
    var adjustment = Quat.fromPitchYawRollDegrees(180, 0, 0);
    stickOrientation = Quat.multiply(stickOrientation, adjustment);

    positionStick(stickOrientation);
}


Script.scriptEnding.connect(cleanUp);
Controller.mouseMoveEvent.connect(mouseMoveEvent);
Script.update.connect(update);
