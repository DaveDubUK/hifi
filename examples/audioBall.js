//
//  audioBall.js
//  examples
//
//  Created by Athanasios Gaitatzes on 2/10/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  This script creates a particle in front of the user that stays in front of
//  the user's avatar as they move, and animates it's radius and color
//  in response to the audio intensity.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var sound = new Sound("https://s3-us-west-1.amazonaws.com/highfidelity-public/sounds/Animals/mexicanWhipoorwill.raw");
var CHANCE_OF_PLAYING_SOUND = 0.01;

var FACTOR = 0.75;

var countParticles = 0;    // the first time around we want to create the particle and thereafter to modify it.
var particleID;

function updateParticle(deltaTime) {
    // the particle should be placed in front of the user's avatar
    var avatarFront = Quat.getFront(MyAvatar.orientation);

    // move particle three units in front of the avatar
    var particlePosition = Vec3.sum(MyAvatar.position, Vec3.multiply(avatarFront, 3));

    if (Math.random() < CHANCE_OF_PLAYING_SOUND) {
        // play a sound at the location of the particle
        var options = new AudioInjectionOptions();
        options.position = particlePosition;
        options.volume = 0.75;
        Audio.playSound(sound, options);
    }

    var audioAverageLoudness = MyAvatar.audioAverageLoudness * FACTOR;
    //print ("Audio Loudness = " + MyAvatar.audioLoudness + " -- Audio Average Loudness = " + MyAvatar.audioAverageLoudness);

    if (countParticles < 1) {
        var particleProperies = {
            position: particlePosition // the particle should stay in front of the user's avatar as he moves
        ,   color: { red: 0, green: 255, blue: 0 }
        ,   radius: audioAverageLoudness
        ,   velocity: { x: 0.0, y: 0.0, z: 0.0 }
        ,   gravity: { x: 0.0, y: 0.0, z: 0.0 }
        ,   damping: 0.0
        }

        particleID = Particles.addParticle (particleProperies);
        countParticles++;
    }
    else {
        // animates the particles radius and color in response to the changing audio intensity
        var newProperties = {
            position: particlePosition // the particle should stay in front of the user's avatar as he moves
        ,   color: { red: 0, green: 255 * audioAverageLoudness, blue: 0 }
        ,   radius: audioAverageLoudness
        };

        Particles.editParticle (particleID, newProperties);
    }
}

// register the call back so it fires before each data send
Script.update.connect(updateParticle);

// register our scriptEnding callback
Script.scriptEnding.connect(function scriptEnding() {});
