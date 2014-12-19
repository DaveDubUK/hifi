//
//  walkApi.js
//
//  version 1.003
//
//  Created by David Wooldridge, Autumn 2014
//
//  Motion, state and Transition objects for use by the walk.js script v1.2
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

Avatar = function() {

    // locomotion status
    this.isNotMoving = true;
    this.isAtWalkingSpeed = false;
    this.isAtFlyingSpeed = false;
    this.isAccelerating = false;
    this.isDecelerating = false;
    this.isDeceleratingFast = false;
    this.isGoingUp = false;
    this.isGoingDown = false;
    this.isUnderGravity = false;
    this.distanceToVoxels = 0;
    this.isOnVoxels = true;
    this.isComingInToLand = false;
    this.isTakingOff = false;

    // if Hydras are connected, the only way to enable use is by never setting any rotations on the arm joints
    this.hydraCheck = function() {

        // function courtesy of Thijs Wenker (frisbee.js)
        var numberOfButtons = Controller.getNumberOfButtons();
        var numberOfTriggers = Controller.getNumberOfTriggers();
        var numberOfSpatialControls = Controller.getNumberOfSpatialControls();
        var controllersPerTrigger = numberOfSpatialControls / numberOfTriggers;
        hydrasConnected = (numberOfButtons == 12 && numberOfTriggers == 2 && controllersPerTrigger == 2);
        return hydrasConnected;
    }

    // settings
    this.armsFree = this.hydraCheck(); // automatically sets true for Hydra support - temporary fix
    this.makesFootStepSounds = true;
    this.gender = undefined;
    this.setGender = function(gender) {

        this.gender = gender;

        switch(gender) {

            case MALE:

                this.selectedWalk = walkAssets.maleWalk;
                this.selectedIdle = walkAssets.maleIdle;
                this.selectedFlyUp = walkAssets.maleFlyUp;
                this.selectedFly = walkAssets.maleFly;
                this.selectedFlyDown = walkAssets.maleFlyDown;
                this.selectedFlyDownSlow = walkAssets.maleFlyDownSlow;
                this.selectedFlyBlend = walkAssets.maleFlyBlend;
                this.selectedHover = walkAssets.maleHover;
                this.selectedSideStepLeft = walkAssets.maleSideStepLeft;
                this.selectedSideStepRight = walkAssets.maleSideStepRight;

                this.currentAnimation = this.selectedIdle;
                return;

            case FEMALE:

                this.selectedWalk = walkAssets.femaleWalk;
                this.selectedIdle = walkAssets.femaleIdle;
                this.selectedFlyUp = walkAssets.femaleFlyUp;
                this.selectedFly = walkAssets.femaleFly;
                this.selectedFlyDown = walkAssets.femaleFlyDown;
                this.selectedFlyDownSlow = walkAssets.femaleFlyDownSlow;
                this.selectedFlyBlend = walkAssets.femaleFlyBlend;
                this.selectedHover = walkAssets.femaleHover;
                this.selectedSideStepLeft = walkAssets.femaleSideStepLeft;
                this.selectedSideStepRight = walkAssets.femaleSideStepRight;

                this.currentAnimation = this.selectedIdle;
                return;
        }
    }
    this.setGender(MALE);
    this.currentAnimation = this.selectedIdle;
    this.avatarJointNames = MyAvatar.getJointNames();

    // calibration
    this.calibration = {

        hipsToFeet: 1.35,
        strideLength: this.selectedWalk.calibration.strideLengthForwards
    }

    this.calibrate = function() {

        // measurements are taken three times to ensure accuracy
        var attempts = 3;

        do {
            for (var i = 0; i < this.avatarJointNames.length; i++) {

                var IKChain = "unknown";
                try {

                    IKChain = walkAssets.animationReference.joints[this.avatarJointNames[i]].IKChain;

                } catch (e) {

                    IKChain = "not set";
                }

                // only need to zero right leg and hips
                if (IKChain === "RightLeg" || this.avatarJointNames[i] === "Hips" ) {

                    MyAvatar.setJointData(this.avatarJointNames[i], Quat.fromPitchYawRollDegrees(0, 0, 0));
                }
            }
            this.calibration.hipsToFeet = MyAvatar.getJointPosition("Hips").y - MyAvatar.getJointPosition("RightToeBase").y;

        } while (attempts-- > 1)
    }

    // zero out avi's joints and pose the fingers
    this.poseFingers = function() {

        for (var i = 0; i < this.avatarJointNames.length; i++) {

            if (i > 17 && i < 34) {
                // right hand fingers
                MyAvatar.setJointData(this.avatarJointNames[i], Quat.fromPitchYawRollDegrees(16, 0, 0));
            } else if (i > 33 && i < 38) {
                // right hand thumb
                MyAvatar.setJointData(this.avatarJointNames[i], Quat.fromPitchYawRollDegrees(4, 0, 0));
            } else if (i > 41 && i < 58) {
                // left hand fingers
                MyAvatar.setJointData(this.avatarJointNames[i], Quat.fromPitchYawRollDegrees(16, 0, 0));
            } else if (i > 57 && i < 62) {
                // left hand thumb
                MyAvatar.setJointData(this.avatarJointNames[i], Quat.fromPitchYawRollDegrees(4, 0, 0));
            }
        }
    };

    this.calibrate();
    this.poseFingers();

    // footsteps
    this.nextStep = RIGHT; // the first step is right, because the waveforms say so
    this.makeFootStepSound = function() {

        // correlate footstep volume with avatar speed. place the audio source at the feet, not the hips
        var SPEED_THRESHOLD = 0.4;
        var VOLUME_ATTENUATION = 0.3;
        var MIN_VOLUME = 0.08;
        var options = {
            position: Vec3.sum(MyAvatar.position, {x:0, y: -this.calibration.hipsToFeet, z:0}),
            volume: motion.speed > SPEED_THRESHOLD ? VOLUME_ATTENUATION * motion.speed / MAX_WALK_SPEED : MIN_VOLUME
        };

        // different sounds for male / female
        var soundNumber = undefined;
        if (this.gender === MALE) {

            soundNumber = 0; // 0, 2 or 4

        } else {

            soundNumber = 2; // 0, 2 or 4
        }
        if (this.nextStep === RIGHT) {

            Audio.playSound(walkAssets.footsteps[soundNumber], options);
            this.nextStep = LEFT;

        } else if (this.nextStep === LEFT) {

            Audio.playSound(walkAssets.footsteps[soundNumber + 1], options);
            this.nextStep = RIGHT;
        }
    }
};

// provides user feedback whilst loading assets
fingerCounter = (function () {

    var _avatarJointNames = MyAvatar.getJointNames();
    var _fingerPoseCount = 1;

    return {

        incrementCounter: function() {

            for (var i = 0; i < _avatarJointNames.length; i++) {

                var poseAngle = 90;

                if (i > 17 && i < 34) {

                    // right hand fingers
                    if (i > 29 && _fingerPoseCount > 1) poseAngle = 4; // index
                    if (i > 25 && _fingerPoseCount > 2) poseAngle = 4; // middle
                    if (i > 21 && _fingerPoseCount > 3) poseAngle = 4; // ring
                    if (i > 17 && _fingerPoseCount > 4) poseAngle = 4; // pinky

                    MyAvatar.setJointData(_avatarJointNames[i], Quat.fromPitchYawRollDegrees(poseAngle, 0, 0));

                } else if (i > 33 && i < 38) {

                    // right hand thumb
                    poseAngle = 25;
                    if (_fingerPoseCount > 0) poseAngle = 4;
                    MyAvatar.setJointData(_avatarJointNames[i], Quat.fromPitchYawRollDegrees(poseAngle, poseAngle, poseAngle));


                } else if (i > 41 && i < 58) {

                    // left hand fingers
                    if (i > 53 && _fingerPoseCount > 6) poseAngle = 4; // index
                    if (i > 49 && _fingerPoseCount > 7) poseAngle = 4; // middle
                    if (i > 45 && _fingerPoseCount > 8) poseAngle = 4; // ring
                    if (i > 41 && _fingerPoseCount > 9) poseAngle = 4; // pinky

                    MyAvatar.setJointData(_avatarJointNames[i], Quat.fromPitchYawRollDegrees(poseAngle, 0, 0));

                } else if (i > 57 && i < 62) {

                    // left hand thumb
                    poseAngle = 25;
                    if (_fingerPoseCount > 5) poseAngle = 4;
                    MyAvatar.setJointData(_avatarJointNames[i], Quat.fromPitchYawRollDegrees(poseAngle, poseAngle, poseAngle));
                }
            }

            if (_fingerPoseCount++ > 9) {

                _fingerPoseCount = 1;
            }
        }
    }

})(); // end fingerPoser object literal

LoadingThrobber = function() {

    this.intervalTimer = Script.setInterval(function(){

        fingerCounter.incrementCounter();

    }, 500);

    this.stop = function() {

        Script.clearInterval(this.intervalTimer);
    }
};

// JS motor
jsMotor = (function () {

    var _motoring = false;
    var _braking = false;
    var _direction = FORWARDS;
    var _motorVelocity = {x:0, y:0, z:0};
    var _motorTimeScale = VERY_LONG_TIME;

    function _update() {

        MyAvatar.motorVelocity = _motorVelocity;
        MyAvatar.motorTimescale = _motorTimeScale;
    }

    return {

        isMotoring: function() {

            return _motoring;
        },

        isBraking: function() {

            return _braking;
        },

        startMotoring: function() {

            _direction = motion.lastDirection;
            var speed = MAX_WALK_SPEED;
            if (_direction === FORWARDS) {

                speed *= -1;
            }
            _motorVelocity =  {x:0.0, y:0.0, z: speed};
            _motorTimeScale = SHORT_TIME;
            _motoring = true;
            _update();
        },

        setMotorSpeed: function(speed, timeScale) {

            if (_direction === FORWARDS) {

                speed *= -1;
            }
            _motorVelocity =  {x:0.0, y:0.0, z: speed};
            _motorTimeScale = timeScale;
            _motoring = true;
            _update();
        },

        applyBrakes: function() { // aka apply brakes

            // stop the motion quickly
            _motorVelocity = {x:0.0, y:0.0, z:0};
            _motorTimeScale = VERY_SHORT_TIME;
            _motoring = false;
            _braking = true;
            _update();
        },

        stopBraking: function() {

            _motorVelocity = {x:0, y:0, z:0};
            _motorTimeScale = VERY_LONG_TIME;
            _braking = false;
            _update();
        }

    }

})(); // end jsMotor object literal


// constructor for the Motion object
Motion = function(avatar) {

    // link the motion to the avatar
    this.avatar = avatar;

    // calibration
    this.calibration = {

        pitchMax: 70,
        maxWalkAcceleration: 5,
        maxWalkDeceleration: 24,
        rollMax: 80,
        angularVelocityMax: 67
    }

    // used to make sure at least one step has been taken when transitioning from a walk cycle
    this.elapsedFTDegrees = 0;

    // current transition
    this.currentTransition = null;

    // orientation, locomotion and timing
    this.speed = 0;
    this.velocity = {x:0, y:0, z:0};
    this.accelerationMagnitude = 0;
    this.acceleration = {x:0, y:0, z:0};
    this.direction = FORWARDS;
    this.transitionCount = 0;
    this.deltaTime = 0;

    // historical orientation, locomotion and timing
    this.lastDirection = FORWARDS;
    this.lastSpeed = 0;
    this.lastVelocity = {x:0, y:0, z:0};
    this.lastAccelerationMagnitude = 0;
    this.lastAcceleration ={x:0, y:0, z:0};

    // calculate local velocity, speed, acceleration and proximity to voxel surface for the current frame
    this.initialise = function(deltaTime) {

        if (jsMotor.isBraking()) {

            // reset any braking
            jsMotor.stopBraking();
        }

        // calculate avatar frame speed, velocity and acceleration
        this.deltaTime = deltaTime;
        var inverseOrientation = Quat.inverse(MyAvatar.orientation);
        this.velocity = Vec3.multiplyQbyV(inverseOrientation, MyAvatar.getVelocity());
        this.speed = Vec3.length(this.velocity);

        if (this.speed < MOVE_THRESHOLD) {

            this.avatar.isNotMoving = true;
            this.avatar.isAtWalkingSpeed = false;
            this.avatar.isAtFlyingSpeed = false;

        } else if (this.speed < MAX_WALK_SPEED) {

            this.avatar.isNotMoving = false;
            this.avatar.isAtWalkingSpeed = true;
            this.avatar.isAtFlyingSpeed = false;

        } else {

            this.avatar.isNotMoving = false;
            this.avatar.isAtWalkingSpeed = false;
            this.avatar.isAtFlyingSpeed = true;
        }

        // determine principle direction of movement
        if (this.avatar.isNotMoving) {

            this.direction = NONE;

        } else if (Math.abs(this.velocity.x) > Math.abs(this.velocity.y) &&
                   Math.abs(this.velocity.x) > Math.abs(this.velocity.z)) {

            if (this.velocity.x < 0) {
                this.direction = LEFT;
            } else if (this.velocity.x > 0){
                this.direction = RIGHT;
            }

        } else if (Math.abs(this.velocity.y) > Math.abs(this.velocity.x) &&
                   Math.abs(this.velocity.y) > Math.abs(this.velocity.z)) {

            if (this.velocity.y > 0) {
                this.direction = UP;
            } else {
                this.direction = DOWN;
            }

        } else if (Math.abs(this.velocity.z) > Math.abs(this.velocity.x) &&
                   Math.abs(this.velocity.z) > Math.abs(this.velocity.y)) {

            if (this.velocity.z < 0) {
                this.direction = FORWARDS;
            } else {
                this.direction = BACKWARDS;
            }
        }

        // determine locomotion status
        if (this.velocity.y > MOVE_THRESHOLD) {

            this.avatar.isGoingUp = true;
            this.avatar.isGoingDown = false;

        } else if (this.velocity.y < -MOVE_THRESHOLD) {

            this.avatar.isGoingUp = false;
            this.avatar.isGoingDown = true;

        } else {

            this.avatar.isGoingUp = false;
            this.avatar.isGoingDown = false;
        }

        // use MyAvatar.getVelocity() to calculate acceleration as MyAvatar.getAcceleration() has thread sync issues
        var acceleration = {x:0, y:0, z:0};
        acceleration.x = (this.velocity.x - this.lastVelocity.x) / deltaTime;
        acceleration.y = (this.velocity.y - this.lastVelocity.y) / deltaTime;
        acceleration.z = (this.velocity.z - this.lastVelocity.z) / deltaTime;
        this.acceleration.x = acceleration.x;
        this.acceleration.y = acceleration.y;
        this.acceleration.z = acceleration.z;
        this.accelerationMagnitude = Vec3.length(acceleration);

        var directedAcceleration = 0;

        if (this.direction === FORWARDS) {

            directedAcceleration = acceleration.z;

        } else if (this.direction === BACKWARDS) {

            directedAcceleration = -acceleration.z;

        } else if (this.direction === LEFT) {

            directedAcceleration = acceleration.x;

        } else if (this.direction === RIGHT) {

            directedAcceleration = -acceleration.x;
        }

        if (directedAcceleration < ACCELERATION_THRESHOLD) {

            this.avatar.isAccelerating = true;
            this.avatar.isDecelerating = false;
            this.avatar.isDeceleratingFast = false;

        } else if (directedAcceleration > DECELERATION_THRESHOLD) {

            this.avatar.isAccelerating = false;
            this.avatar.isDecelerating = true;

            if (directedAcceleration > FAST_DECELERATION_THRESHOLD) {

                this.avatar.isDeceleratingFast = true;

            } else {

                this.avatar.isDeceleratingFast = false;
            }

        } else {

            this.avatar.isAccelerating = false;
            this.avatar.isDecelerating = false;
            this.avatar.isDeceleratingFast = false;
        }

        // how far above the voxel suface is the avatar?
        var pickRay = {origin: MyAvatar.position, direction: {x:0, y:-1, z:0}};
        var distanceToVoxels = Voxels.findRayIntersectionBlocking(pickRay).distance;
        this.avatar.distanceToVoxels = distanceToVoxels - this.avatar.calibration.hipsToFeet;

        this.avatar.isOnVoxels = false;
        this.avatar.isComingInToLand = false;
        this.avatar.isTakingOff = false;
        this.avatar.isUnderGravity = false;

        if (this.avatar.distanceToVoxels < ON_SURFACE_THRESHOLD) {

            this.avatar.isOnVoxels = true;

        }
        if (distanceToVoxels < GRAVITY_THRESHOLD) {

            this.avatar.isUnderGravity = true;
        }
        if (this.avatar.isUnderGravity && this.avatar.isGoingDown) {

            this.avatar.isComingInToLand = true;

        } else if (this.avatar.isUnderGravity && this.avatar.isGoingUp) {

            this.avatar.isTakingOff = true;
        }
    }

    // frequency time wheel (foot / ground speed matching)
    this.frequencyTimeWheelPos = 0;
    this.frequencyTimeWheelRadius = 0.5;
    this.recentFrequencyTimeIncrements = [];
    for(var i = 0; i < 8; i++) {
        this.recentFrequencyTimeIncrements.push(0);
    }
    this.averageFrequencyTimeIncrement = 0;

    this.advanceFrequencyTimeWheel = function(angle){

        this.elapsedFTDegrees += angle;

        // keep a running average of increments for use in transitions (used for non-walking cycles)
        this.recentFrequencyTimeIncrements.push(angle);
        this.recentFrequencyTimeIncrements.shift();
        for(increment in this.recentFrequencyTimeIncrements) {
            this.averageFrequencyTimeIncrement += this.recentFrequencyTimeIncrements[increment];
        }
        this.averageFrequencyTimeIncrement /= this.recentFrequencyTimeIncrements.length;

        this.frequencyTimeWheelPos += angle;
        if (this.frequencyTimeWheelPos >= 360) {
            this.frequencyTimeWheelPos = this.frequencyTimeWheelPos % 360;
        }
    }

    this.saveHistory = function() {

        this.lastDirection = this.direction;
        this.lastSpeed = this.speed;
        this.lastVelocity = this.velocity;
        this.lastAccelerationMagnitude = this.accelerationMagnitude;
        this.lastAcceleration = this.acceleration;
        this.lastDistanceToVoxels = avatar.distanceToVoxels;
    }
};  // end Motion constructor


// animation manipulation object
animationOperations = (function() {

    return {

        // helper function for renderMotion(). calculate joint translations based on animation file settings and frequency * time
        calculateTranslations: function(animation, ft, direction) {

            var jointName = "Hips";
            var joint = animation.joints[jointName];
            var jointTranslations = {x:0, y:0, z:0};

            // gather modifiers and multipliers
            modifiers = new JointModifiers(joint, direction);

            // calculate translations. Use synthesis filters where specified by the animation data file.

            // sway (oscillation on the x-axis)
            if(animation.filters.hasOwnProperty(jointName) &&
               'swayFilter' in animation.filters[jointName]) {

                jointTranslations.x = joint.sway *
                    animation.filters[jointName].swayFilter.
                    calculate(filter.degToRad(ft + joint.swayPhase)) +
                    joint.swayOffset;

            } else {

                jointTranslations.x = joint.sway *
                    Math.sin(filter.degToRad(ft + joint.swayPhase)) +
                    joint.swayOffset;
            }

            // bob (oscillation on the y-axis)
            if(animation.filters.hasOwnProperty(jointName) &&
               'bobFilter' in animation.filters[jointName]) {

                jointTranslations.y = joint.bob *
                    animation.filters[jointName].bobFilter.calculate
                    (filter.degToRad(modifiers.bobFrequencyMultiplier * ft +
                    joint.bobPhase + modifiers.bobReverseModifier)) +
                    joint.bobOffset;

            } else {

                jointTranslations.y = joint.bob *
                    Math.sin(filter.degToRad(modifiers.bobFrequencyMultiplier * ft +
                    joint.bobPhase + modifiers.bobReverseModifier)) +
                    joint.bobOffset;

                // check for specified low pass filter for this joint (currently Hips bob only)
                if(animation.filters.hasOwnProperty(jointName) &&
                       'bobLPFilter' in animation.filters[jointName]) {

                    jointTranslations.y = filter.clipTrough(jointTranslations.y, joint, 2);
                    jointTranslations.y = animation.filters[jointName].bobLPFilter.process(jointTranslations.y);
                }
            }

            // thrust (oscillation on the z-axis)
            if(animation.filters.hasOwnProperty(jointName) &&
               'thrustFilter' in animation.filters[jointName]) {

                jointTranslations.z = joint.thrust *
                    animation.filters[jointName].thrustFilter.
                    calculate(filter.degToRad(modifiers.thrustFrequencyMultiplier * ft +
                    joint.thrustPhase)) +
                    joint.thrustOffset;

            } else {

                jointTranslations.z = joint.thrust *
                    Math.sin(filter.degToRad(modifiers.thrustFrequencyMultiplier * ft +
                    joint.thrustPhase)) +
                    joint.thrustOffset;
            }

            return jointTranslations;
        },

        // helper function for renderMotion(). calculate joint rotations based on animation file settings and frequency * time
        calculateRotations: function(jointName, animation, ft, direction) {

            var joint = animation.joints[jointName];
            var jointRotations = {x:0, y:0, z:0};

            // gather modifiers and multipliers for this joint
            modifiers = new JointModifiers(joint, direction);

            // calculate rotations. Use synthesis filters where specified by the animation data file.

            // calculate pitch
            if(animation.filters.hasOwnProperty(jointName) &&
               'pitchFilter' in animation.filters[jointName]) {

                jointRotations.x = modifiers.pitchReverseInvert * (modifiers.pitchSign * joint.pitch *
                    animation.filters[jointName].pitchFilter.calculate
                    (filter.degToRad(ft * modifiers.pitchFrequencyMultiplier +
                    joint.pitchPhase + modifiers.pitchReverseModifier)) +
                    modifiers.pitchOffsetSign * joint.pitchOffset);

            } else {

                jointRotations.x = modifiers.pitchReverseInvert * (modifiers.pitchSign * joint.pitch *
                    Math.sin(filter.degToRad(ft * modifiers.pitchFrequencyMultiplier +
                    joint.pitchPhase + modifiers.pitchReverseModifier)) +
                    modifiers.pitchOffsetSign * joint.pitchOffset);
            }

            // calculate yaw
            if(animation.filters.hasOwnProperty(jointName) &&
               'yawFilter' in animation.filters[jointName]) {

                jointRotations.y = modifiers.yawSign * joint.yaw *
                    animation.filters[jointName].yawFilter.calculate
                    (filter.degToRad(ft + joint.yawPhase + modifiers.yawReverseModifier)) +
                    joint.yawOffset;

            } else {

                jointRotations.y = modifiers.yawSign * joint.yaw *
                    Math.sin(filter.degToRad(ft + joint.yawPhase + modifiers.yawReverseModifier)) +
                    joint.yawOffset;
            }

            // calculate roll
            if(animation.filters.hasOwnProperty(jointName) &&
               'rollFilter' in animation.filters[jointName]) {

                jointRotations.z = modifiers.rollSign * joint.roll *
                    animation.filters[jointName].rollFilter.calculate
                    (filter.degToRad(ft + joint.rollPhase + modifiers.rollReverseModifier)) +
                    joint.rollOffset;

            } else {

                jointRotations.z = modifiers.rollSign * joint.roll *
                    Math.sin(filter.degToRad(ft + joint.rollPhase + modifiers.rollReverseModifier)) +
                    joint.rollOffset;
            }
            return jointRotations;
        },

        zeroAnimation: function(animation) {

            for (i in animation.joints) {

                animation.joints[i].pitch = 0;
                animation.joints[i].yaw = 0;
                animation.joints[i].roll = 0;
                animation.joints[i].pitchPhase = 0;
                animation.joints[i].yawPhase = 0;
                animation.joints[i].rollPhase = 0;
                animation.joints[i].pitchOffset = 0;
                animation.joints[i].yawOffset = 0;
                animation.joints[i].rollOffset = 0;
                if (i === "Hips") {

                    // Hips only
                    animation.joints[i].thrust = 0;
                    animation.joints[i].sway = 0;
                    animation.joints[i].bob = 0;
                    animation.joints[i].thrustPhase = 0;
                    animation.joints[i].swayPhase = 0;
                    animation.joints[i].bobPhase = 0;
                }
            }
        },

        blendAnimation: function(sourceAnimation, targetAnimation, percent) {

            for (i in targetAnimation.joints) {

                targetAnimation.joints[i].pitch += percent * sourceAnimation.joints[i].pitch;
                targetAnimation.joints[i].yaw += percent * sourceAnimation.joints[i].yaw;
                targetAnimation.joints[i].roll += percent * sourceAnimation.joints[i].roll;
                targetAnimation.joints[i].pitchPhase += percent * sourceAnimation.joints[i].pitchPhase;
                targetAnimation.joints[i].yawPhase += percent * sourceAnimation.joints[i].yawPhase;
                targetAnimation.joints[i].rollPhase += percent * sourceAnimation.joints[i].rollPhase;
                targetAnimation.joints[i].pitchOffset += percent * sourceAnimation.joints[i].pitchOffset;
                targetAnimation.joints[i].yawOffset += percent * sourceAnimation.joints[i].yawOffset;
                targetAnimation.joints[i].rollOffset += percent * sourceAnimation.joints[i].rollOffset;
                if (i === "Hips") {

                    // Hips only
                    targetAnimation.joints[i].thrust += percent * sourceAnimation.joints[i].thrust;
                    targetAnimation.joints[i].sway += percent * sourceAnimation.joints[i].sway;
                    targetAnimation.joints[i].bob += percent * sourceAnimation.joints[i].bob;
                    targetAnimation.joints[i].thrustPhase += percent * sourceAnimation.joints[i].thrustPhase;
                    targetAnimation.joints[i].swayPhase += percent * sourceAnimation.joints[i].swayPhase;
                    targetAnimation.joints[i].bobPhase += percent * sourceAnimation.joints[i].bobPhase;
                }
            }
        }
    }
})(); // end animation object literal

// finite state machine. Avatar locomotion modes represent states in the FSM
state = (function () {

    return {

        // the finite list of states
        STANDING: 1,
        WALKING: 2,
        SIDE_STEP: 3,
        FLYING: 4,

        currentState: this.STANDING,

        // status vars
        powerOn: true,

        setInternalState: function(newInternalState) {

            switch (newInternalState) {

                case this.WALKING:

                    this.currentState = this.WALKING;
                    return;

                case this.FLYING:

                    this.currentState = this.FLYING;
                    return;

                case this.SIDE_STEP:

                    this.currentState = this.SIDE_STEP;
                    return;

                case this.STANDING:
                default:

                    this.currentState = this.STANDING;
                    return;
            }
        }
    }
})(); // end state object literal


// action object
Action = function(actionName, duration, strength) {

    this.name = actionName;
    this.strength = isDefined(strength) ? strength : 1;
    this.progress = 0;
    this.duration = isDefined(duration) ? duration : actions[actionName].duration;
    this.reachPose = actions[actionName].reachPose;
    this.delay = actions[actionName].delay;
    this.attack = actions[actionName].attack;
    this.decay = actions[actionName].decay;
    this.sustain = actions[actionName].sustain;
    this.release = actions[actionName].release;
    this.smoothingFilter = filter.createAveragingFilter(actions[actionName].smoothing);

    this.currentStrength = function() {

        // apply (D)ASDR envelope
        var segmentProgress = undefined; // progress through chosen segment
        var segmentTimeDelta = undefined; // total change in time over chosen segment
        var segmentStrengthDelta = undefined; // total change in strength over chosen segment
        var lastStrength = undefined; // the last value the previous segment held
        var currentStrength = undefined; // return value

        // select parameters based on segment (a segment being one of (D),A,S,D or R)
        if (this.progress >= this.sustain.timing) {

            // release segment
            segmentProgress = this.progress - this.sustain.timing;
            segmentTimeDelta = this.release.timing - this.sustain.timing;
            segmentStrengthDelta = this.release.strength - this.sustain.strength;
            lastStrength = this.sustain.strength;

        } else if (this.progress >= this.decay.timing) {

            // sustain phase
            segmentProgress = this.progress - this.decay.timing;
            segmentTimeDelta = this.sustain.timing - this.decay.timing;
            segmentStrengthDelta = this.sustain.strength - this.decay.strength;
            lastStrength = this.decay.strength;

        } else if (this.progress >= this.attack.timing) {

            // decay phase
            segmentProgress = this.progress - this.attack.timing;
            segmentTimeDelta = this.decay.timing - this.attack.timing;
            segmentStrengthDelta = this.decay.strength - this.attack.strength;
            lastStrength = this.attack.strength;

        } else if (this.progress >= this.delay.timing) {

            // attack phase
            segmentProgress = this.progress - this.delay.timing;
            segmentTimeDelta = this.attack.timing - this.delay.timing;
            segmentStrengthDelta = this.attack.strength - this.delay.strength;
            lastStrength = 0; //this.delay.strength;

        } else {

            // delay phase
            segmentProgress = this.progress;
            segmentTimeDelta = this.delay.timing;
            segmentStrengthDelta = this.delay.strength;
            lastStrength = 0;
        }

        currentStrength = segmentTimeDelta > 0 ? lastStrength + segmentStrengthDelta * segmentProgress / segmentTimeDelta
                                               : lastStrength;
        currentStrength *= this.strength;

        // smooth off the response curve
        currentStrength = this.smoothingFilter.process(currentStrength);
        return currentStrength;
    }
};

spatialAwareness = (function () {

    var _distancesToObstacle = 100;
    var _lastDistanceToObstacle = 100;
    var _collidingWithVoxel = false;
    var COLLISION_THRESHOLD = 0.7;

    return {

        update: function() {

            if (motion.speed > MOVE_THRESHOLD) {

                // look ahead for obstacles
                var inverseOrientation = Quat.inverse(MyAvatar.orientation);
                var pickRay = {origin: MyAvatar.position, direction: Vec3.multiplyQbyV(inverseOrientation, {x:0, y:0, z:1})};
                _distancesToObstacle = Voxels.findRayIntersectionBlocking(pickRay).distance;

                if (_distancesToObstacle < COLLISION_THRESHOLD &&
                    _lastDistanceToObstacle > _distancesToObstacle &&
                   !_collidingWithVoxel) {

                    _collidingWithVoxel = true;
                    liveActions.addAction(new Action("MaleProtectHeadRP"));

                } else if (_distancesToObstacle > COLLISION_THRESHOLD) {

                    _collidingWithVoxel = false;
                }
                _lastDistanceToObstacle = _distancesToObstacle;


                // respond to sudden, rapid deceleration
                if (avatar.isDeceleratingFast ||
                   (avatar.selectedAnimation === avatar.selectedFlyBlend && avatar.isDecelerating)) {

                    // if not a live action already
                    if (!liveActions.alreadyHasAction("RapidFlyingSlowdownRP")) {

                        liveActions.addAction(new Action("RapidFlyingSlowdownRP"));
                    }
                }

                // fly to walk action when slowing ascent under gravity
                var JUMP_PEAK_DECELERATION = -2.75;
                var MIN_JUMP_HEIGHT = 0.15;
                if (motion.acceleration.y < JUMP_PEAK_DECELERATION &&
                    avatar.isUnderGravity && avatar.distanceToVoxels > MIN_JUMP_HEIGHT &&
                    motion.direction !== UP && motion.direction !== DOWN) {

                    // if not a live action already
                    if (!liveActions.alreadyHasAction("MaleFlyToWalkRP")) {

                        // add the action
                        var heightToThresholdRatio = avatar.distanceToVoxels /
                                                    (GRAVITY_THRESHOLD + avatar.calibration.hipsToFeet);
                        var SHORT_DURATION = 0.85;
                        var actionIntensity = 6.5 * heightToThresholdRatio;
                        var duration = actionIntensity < SHORT_DURATION ? SHORT_DURATION : actionIntensity
                        var strength = actionIntensity > 1 ? 1 : actionIntensity;
                        liveActions.addAction(new Action("MaleFlyToWalkRP", duration, strength));
                    }
                }

                // apply a little squash on fly to walk landing
                var almostOnSurface = ON_SURFACE_THRESHOLD + 0.4;
                if (avatar.distanceToVoxels < almostOnSurface &&
                    motion.lastDistanceToVoxels > almostOnSurface &&
                    Math.abs(motion.velocity.z) > MOVE_THRESHOLD) {

                    var LANDING_HARDNESS = 0.5;

                    // if not a live action already
                    if (!liveActions.alreadyHasAction("LandOnSurface")) {

                        var strength = -motion.velocity.y * LANDING_HARDNESS > 1 ? 1 : -motion.velocity.y * LANDING_HARDNESS;
                        liveActions.addAction(new Action("LandOnSurface", 0.6, strength));
                    }

                    // if speed is significantly faster than walking speed, put on the brakes
                    var speedThreshold = MAX_WALK_SPEED + 1;
                    if (motion.velocity.z < -speedThreshold) {

                        // slow to walking speed very rapidly
                        jsMotor.setMotorSpeed(-MAX_WALK_SPEED, SHORT_TIME);

                    } else if (motion.velocity.z > speedThreshold) {

                        // slow to walking speed very rapidly
                        jsMotor.setMotorSpeed(MAX_WALK_SPEED, SHORT_TIME);
                    }
                }
            }
        }
    }

})();

// holds the current list of live actions being played (separate from actions owned by Transitons)
liveActions = (function () {

    var _actions = [];

    return {

        addAction: function(newAction) {

            _actions.push(newAction);
        },

        update: function() {

            for(action in _actions) {

                _actions[action].progress += (motion.deltaTime / _actions[action].duration);

                if (_actions[action].progress >= 1) {

                    // time to kill off this action
                    _actions.splice(action, 1);
                }
            }
        },

        actionsCount: function() {

            return _actions.length;
        },

        actions: _actions,

        alreadyHasAction: function(actionName) {

            for(action in _actions) {

                if (_actions[action].name === actionName) {

                    return true;
                }
            }
            return false;
        },

        getAction: function(actionNumber) {

            return _actions[actionNumber];
        }
    }

})(); // end liveActions


// individual joint modiers (mostly used to provide symmetry between left and right limbs)
JointModifiers = function(joint, direction) {

    // gather modifiers and multipliers
    this.pitchFrequencyMultiplier = 1;
    this.pitchReverseModifier = 0;
    this.yawReverseModifier = 0;
    this.rollReverseModifier = 0;
    this.pitchSign = 1; // for sidestepping and incorrectly rigged Ron ToeBases
    this.yawSign = 1;
    this.rollSign = 1;
    this.pitchReverseInvert = 1; // used to invert foot pitch curve on backwards walk
    this.pitchOffsetSign = 1;
    this.bobReverseModifier = 0;
    this.bobFrequencyMultiplier = 1;
    this.thrustFrequencyMultiplier = 1;

    if (isDefined(joint)) {

        if (isDefined(joint.pitchFrequencyMultiplier)) {
            this.pitchFrequencyMultiplier = joint.pitchFrequencyMultiplier;
        }
        if (isDefined(joint.pitchSign)) {
            this.pitchSign = joint.pitchSign;
        }
        if (isDefined(joint.yawSign)) {
            this.yawSign = joint.yawSign;
        }
        if (isDefined(joint.rollSign)) {
            this.rollSign = joint.rollSign;
        }
        if (isDefined(joint.pitchReverseInvert) && direction === BACKWARDS) {
            // used to flip an animation curve (used on feet pitch curves)
            this.pitchReverseInvert = joint.pitchReverseInvert;
        }
        if (isDefined(joint.pitchReverseModifier) && direction === BACKWARDS) {
            // used to reverse symmetric animations (e.g. walk backwards)
            this.pitchReverseModifier = joint.pitchReverseModifier;
        }
        if (isDefined(joint.yawReverseModifier) && direction === BACKWARDS) {
            this.yawReverseModifier = joint.yawReverseModifier;
        }
        if (isDefined(joint.rollReverseModifier) && direction === BACKWARDS) {
            this.rollReverseModifier = joint.rollReverseModifier;
        }
        if (isDefined(joint.pitchOffsetSign)) {
            this.pitchOffsetSign = joint.pitchOffsetSign;
        }
        if (isDefined(joint.bobReverseModifier) && direction === BACKWARDS) {
            this.bobReverseModifier = joint.bobReverseModifier;
        }
        if (isDefined(joint.bobFrequencyMultiplier)) {
            this.bobFrequencyMultiplier = joint.bobFrequencyMultiplier;
        }
        if (isDefined(joint.thrustFrequencyMultiplier)) {
            this.thrustFrequencyMultiplier = joint.thrustFrequencyMultiplier;
        }
    }
};