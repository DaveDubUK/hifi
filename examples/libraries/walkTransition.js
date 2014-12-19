//
//  walkTransition.js
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

TransitionParameters = function() {

    this.duration = 0.7;
    this.actions = [];
    this.easingLower = {x:0.5, y:0.5};
    this.easingUpper = {x:0.5, y:0.5};
}

// constructor for animation Transition
Transition = function(nextAnimation, lastAnimation, lastTransition, playTransitionActions) {

    if (playTransitionActions === undefined ) {
        playTransitionActions = true;
    }

    this.id = motion.transitionCount++; // serial number for this transition

    // record the current state of animation
    this.nextAnimation = nextAnimation;
    this.lastAnimation = lastAnimation;
    this.lastTransition = lastTransition;

    // collect information about the currently playing animation
    this.direction = motion.direction;
    this.lastDirection = motion.lastDirection;
    this.lastFrequencyTimeWheelPos = motion.frequencyTimeWheelPos;
    this.lastFrequencyTimeIncrement = motion.averageFrequencyTimeIncrement;
    this.lastFrequencyTimeWheelRadius = motion.frequencyTimeWheelRadius;
    this.stopAngle = 0; // what angle should we stop turning this frequency time wheel?
    this.degreesToTurn = 0; // total degrees to turn the ft wheel before the avatar stops (walk only)
    this.degreesRemaining = 0; // remaining degrees to turn the ft wheel before the avatar stops (walk only)
    this.lastElapsedFTDegrees = motion.elapsedFTDegrees; // degrees elapsed since last transition start
    motion.elapsedFTDegrees = 0; // reset ready for the next transtion

    // set the inital parameters for the transition
    this.parameters = new TransitionParameters();
    this.liveActions = []; // to be populate with action objects as per action names supplied by TransitionParameters

    if (walkAssets && isDefined(lastAnimation) && isDefined(nextAnimation)) {

        // start up any actions for this transition
        walkAssets.transitions.getTransitionParameters(lastAnimation, nextAnimation, this.parameters);
        if (playTransitionActions) {

            for(action in this.parameters.actions) {

                // create the action and add it to this Transition's live actions
                this.liveActions.push(new Action(this.parameters.actions[action]));
            }
        }
    }

    this.continuedMotionDuration = 0;
    this.startTime = new Date().getTime(); // Starting timestamp (seconds)
    this.progress = 0; // how far are we through the transition?
    this.filteredProgress = 0;
    this.startLocation = MyAvatar.position;

    // will we need to continue motion? if the motor has been turned on, then we do
    if (jsMotor.isMotoring()) {

        // decide at which angle we should stop the frequency time wheel
        var stopAngle = this.lastAnimation.calibration.stopAngleForwards;
        if (motion.lastDirection === BACKWARDS) {

            stopAngle = this.lastAnimation.calibration.stopAngleBackwards;
        }
        var degreesToTurn = 0;
        var lastFrequencyTimeWheelPos = this.lastFrequencyTimeWheelPos;
        var lastElapsedFTDegrees = this.lastElapsedFTDegrees;

        var debug = '';

        // set the stop angle depending on which quadrant of the walk cycle we are currently in
        // and decide whether we need to take an extra step to complete the walk cycle or not
        if(lastFrequencyTimeWheelPos <= stopAngle && lastElapsedFTDegrees < 180) {

            // we have not taken a complete step yet, so we advance to the second stop angle
            stopAngle += 180;
            degreesToTurn = stopAngle  - lastFrequencyTimeWheelPos;

        } else if(lastFrequencyTimeWheelPos > stopAngle && lastFrequencyTimeWheelPos <= stopAngle + 90) {

            // take an extra step to complete the walk cycle and stop at the second stop angle
            stopAngle += 180;
            degreesToTurn = stopAngle - lastFrequencyTimeWheelPos;

        } else if(lastFrequencyTimeWheelPos > stopAngle + 90 && lastFrequencyTimeWheelPos <= stopAngle + 180) {

            // stop on the other foot at the second stop angle for this walk cycle
            stopAngle += 180;
            degreesToTurn = stopAngle - lastFrequencyTimeWheelPos;

        } else if(lastFrequencyTimeWheelPos > stopAngle + 180 && lastFrequencyTimeWheelPos <= stopAngle + 270) {

            // take an extra step to complete the walk cycle and stop at the first stop angle
            degreesToTurn = stopAngle + 360 - lastFrequencyTimeWheelPos;

        } else if(lastFrequencyTimeWheelPos <= stopAngle) {

            degreesToTurn = stopAngle - lastFrequencyTimeWheelPos;

        } else {

            degreesToTurn = 360 - lastFrequencyTimeWheelPos + stopAngle;
        }
        this.stopAngle = stopAngle;
        this.degreesToTurn = degreesToTurn;
        this.degreesRemaining = degreesToTurn;

        // work out the distance we need to cover to complete the cycle
        var distance = degreesToTurn * avatar.calibration.strideLength / 180;

        // work out the duration for this transition (assuming starting from MAX_WALK_SPEED as we have already set that on the JS motor)
        this.continuedMotionDuration = distance / MAX_WALK_SPEED;

        // do we need more time to complete the cyccle than the set duration allows?
        if (this.continuedMotionDuration > this.parameters.duration) {

            this.parameters.duration = this.continuedMotionDuration;
        }
    }


    // deal with transition recursion (overlapping transitions)
    this.recursionDepth = 0;
    this.incrementRecursion = function() {

        this.recursionDepth += 1;

        // cancel any continued motion
        this.degreesToTurn = 0;

        if(this.lastTransition !== nullTransition) {

            this.lastTransition.incrementRecursion();

            if(this.lastTransition.recursionDepth > MAX_TRANSITION_RECURSION) {

                this.lastTransition.die();
                this.lastTransition = nullTransition;
            }
        }
    };

    if(this.lastTransition !== nullTransition) {

        this.lastTransition.incrementRecursion();
    }


    // end of transition initialisation. begin transition public methods


    this.advancePreviousFrequencyTimeWheel = function(deltaTime) {

        var wheelAdvance = undefined;

        if (this.lastAnimation === avatar.selectedWalk &&
            this.nextAnimation === avatar.selectedIdle) {

            if(this.degreesRemaining <= 0) {

                // stop continued motion
                wheelAdvance = 0;
                if (jsMotor.isMotoring()) {

                    jsMotor.applyBrakes();
                }

            } else {

                wheelAdvance = this.degreesRemaining * deltaTime / this.continuedMotionDuration;
                var distanceToTravel = avatar.calibration.strideLength * wheelAdvance / 180;
                var MIN_BRAKING_DISTANCE = 0.01;

                if (distanceToTravel < MIN_BRAKING_DISTANCE) {

                    distanceToTravel = 0;
                    this.degreesRemaining = 0;

                } else {

                    this.degreesRemaining -= wheelAdvance;
                }
                var newSpeed = distanceToTravel / deltaTime > MAX_WALK_SPEED ? MAX_WALK_SPEED : distanceToTravel / deltaTime;

                jsMotor.setMotorSpeed(newSpeed, SHORT_TIME);
            }

        } else {

            wheelAdvance = this.lastFrequencyTimeIncrement;
        }

        // advance the ft wheel
        this.lastFrequencyTimeWheelPos += wheelAdvance;
        if (this.lastFrequencyTimeWheelPos >= 360) {

            this.lastFrequencyTimeWheelPos = this.lastFrequencyTimeWheelPos % 360;
        }

        // advance ft wheel for the nested (previous) Transition
        if (this.lastTransition !== nullTransition) {

            this.lastTransition.advancePreviousFrequencyTimeWheel(deltaTime);
        }

        // update the lastElapsedFTDegrees for short stepping
        this.lastElapsedFTDegrees += wheelAdvance;
        this.degreesTurned += wheelAdvance;
    };

    this.updateProgress = function() {

        var elapasedTime = (new Date().getTime() - this.startTime) / 1000;
        this.progress = elapasedTime / this.parameters.duration;
        this.progress = Math.round(this.progress * 1000) / 1000;

        // updated nested transition/s
        if(this.lastTransition !== nullTransition) {

            if(this.lastTransition.updateProgress() === TRANSITION_COMPLETE) {

                // the previous transition is now complete
                this.lastTransition.die();
                this.lastTransition = nullTransition;
            }
        }

        // update any actions
        for(action in this.liveActions) {

            // use independent timing for actions so as not to apply Bezier response above
            this.liveActions[action].progress += (motion.deltaTime / this.liveActions[action].duration);

            if (this.liveActions[action].progress >= 1) {

                // time to kill off this action
                this.liveActions.splice(action, 1);
            }
        }

        this.filteredProgress = filter.bezier(this.progress, this.parameters.easingLower, this.parameters.easingUpper);
        return this.progress >= 1 ? TRANSITION_COMPLETE : false;
    };

    this.blendTranslations = function(frequencyTimeWheelPos, direction) {

        var lastTranslations = {x:0, y:0, z:0};
        var nextTranslations = animationOperations.calculateTranslations(this.nextAnimation,
                                                     frequencyTimeWheelPos,
                                                     direction);

        // are we blending with a previous, still live transition?
        if(this.lastTransition !== nullTransition) {

            lastTranslations = this.lastTransition.blendTranslations(this.lastFrequencyTimeWheelPos,
                                                                     this.lastDirection);

        } else {

            lastTranslations = animationOperations.calculateTranslations(this.lastAnimation,
                                                     this.lastFrequencyTimeWheelPos,
                                                     this.lastDirection);
        }

        nextTranslations.x = this.filteredProgress * nextTranslations.x +
                             (1 - this.filteredProgress) * lastTranslations.x;

        nextTranslations.y = this.filteredProgress * nextTranslations.y +
                             (1 - this.filteredProgress) * lastTranslations.y;

        nextTranslations.z = this.filteredProgress * nextTranslations.z +
                             (1 - this.filteredProgress) * lastTranslations.z;

        if (this.liveActions.length > 0) {

            for(action in this.liveActions) {

                var actionStrength = filter.bezier(this.liveActions[action].currentStrength(), {x:1, y:0}, {x:0, y:1});
                var poseTranslations = animationOperations.calculateTranslations(walkAssets.getReachPose(this.liveActions[action].reachPose),
                                                             frequencyTimeWheelPos,
                                                             direction);

                if(Math.abs(poseTranslations.x) > 0) {

                    nextTranslations.x = actionStrength * poseTranslations.x +
                                        (1 - actionStrength) * nextTranslations.x;
                }

                if(Math.abs(poseTranslations.y) > 0) {

                    nextTranslations.y = actionStrength * poseTranslations.y +
                                        (1 - actionStrength) * nextTranslations.y;
                }

                if(Math.abs(poseTranslations.z) > 0) {

                    nextTranslations.z = actionStrength * poseTranslations.z +
                                        (1 - actionStrength) * nextTranslations.z;
                }
            }
        }

        return nextTranslations;
    };

    this.blendRotations = function(jointName, frequencyTimeWheelPos, direction) {

        var lastRotations = {x:0, y:0, z:0};
        var nextRotations = animationOperations.calculateRotations(jointName,
                                               this.nextAnimation,
                                               frequencyTimeWheelPos,
                                               direction);

        // attenuate transition effects for shorter steps
        var shortStepAdjust = 1;
        var SHORT_STEP = 120; // minimum ft wheel degrees turned to define a short step

        if (this.lastAnimation === avatar.selectedWalk &&
            this.lastElapsedFTDegrees < SHORT_STEP) {

            shortStepAdjust = 1 - ((SHORT_STEP - this.lastElapsedFTDegrees) / SHORT_STEP);
            shortStepAdjust = filter.bezier(shortStepAdjust, {x:1, y:0}, {x:0, y:1});
        }

        // are we blending with a previous, still live transition?
        if(this.lastTransition !== nullTransition) {

            lastRotations = this.lastTransition.blendRotations(jointName,
                                                               this.lastFrequencyTimeWheelPos,
                                                               this.lastDirection);
        } else {

            lastRotations = animationOperations.calculateRotations(jointName,
                                               this.lastAnimation,
                                               this.lastFrequencyTimeWheelPos,
                                               this.lastDirection);
        }

        nextRotations.x = shortStepAdjust * this.filteredProgress * nextRotations.x +
                          (1 - shortStepAdjust * this.filteredProgress) * lastRotations.x;

        nextRotations.y = shortStepAdjust * this.filteredProgress * nextRotations.y +
                          (1 - shortStepAdjust * this.filteredProgress) * lastRotations.y;

        nextRotations.z = shortStepAdjust * this.filteredProgress * nextRotations.z +
                          (1 - shortStepAdjust * this.filteredProgress) * lastRotations.z;


        // are there actions defined for this transition?
        if (this.liveActions.length > 0) {

            for(action in this.liveActions) {

                var actionStrength = filter.bezier(this.liveActions[action].currentStrength(), {x:1, y:0}, {x:0, y:1});
                var poseRotations = animationOperations.calculateRotations(jointName,
                                                       walkAssets.getReachPose(this.liveActions[action].reachPose),
                                                       frequencyTimeWheelPos,
                                                       direction);
                if(Math.abs(poseRotations.x) > 0) {

                    nextRotations.x = shortStepAdjust * actionStrength * poseRotations.x +
                                     (1 - shortStepAdjust * actionStrength) * nextRotations.x;
                }

                if(Math.abs(poseRotations.y) > 0) {

                    nextRotations.y = shortStepAdjust * actionStrength * poseRotations.y +
                                     (1 - shortStepAdjust * actionStrength) * nextRotations.y;
                }

                if(Math.abs(poseRotations.z) > 0) {

                    nextRotations.z = shortStepAdjust * actionStrength * poseRotations.z +
                                     (1 - shortStepAdjust * actionStrength) * nextRotations.z;
                }
            }
        }

        return nextRotations;
    };

    this.die = function() {

        if (jsMotor.isMotoring()) {

            jsMotor.applyBrakes();
        }
    };

}; // end Transition constructor
