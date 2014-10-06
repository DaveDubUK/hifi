//
//  avatarCollision.js
//  examples
//
//  Created by Andrew Meadows on 2014-04-09
//  Copyright 2014 High Fidelity, Inc.
//
//  Play a sound on collisions with your avatar
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var SOUND_TRIGGER_CLEAR = 1000; // milliseconds
var SOUND_TRIGGER_DELAY = 200; // milliseconds
var soundExpiry = 0;
var DateObj = new Date();
var audioOptions = new AudioInjectionOptions();
audioOptions.volume = 0.5;
audioOptions.position = { x: 0, y: 0, z: 0 };

var hitSounds = new Array();
hitSounds[0] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit1.raw");
hitSounds[1] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit2.raw");
hitSounds[2] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit3.raw");
hitSounds[3] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit4.raw");
hitSounds[4] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit5.raw");
hitSounds[5] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit6.raw");
hitSounds[6] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit7.raw");
hitSounds[7] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit8.raw");
hitSounds[8] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit9.raw");
hitSounds[9] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit10.raw");
hitSounds[10] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit11.raw");
hitSounds[11] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit12.raw");
hitSounds[12] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit13.raw");
hitSounds[13] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit14.raw");
hitSounds[14] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit15.raw");
hitSounds[15] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit16.raw");
hitSounds[16] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit17.raw");
hitSounds[17] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit18.raw");
hitSounds[18] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit19.raw");
hitSounds[19] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit20.raw");
hitSounds[20] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit21.raw");
hitSounds[21] = new Sound("http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Collisions-hitsandslaps/Hit22.raw");

function playHitSound(mySessionID, theirSessionID, collision) {
    var now = new Date();
    var msec = now.getTime();
    if (msec > soundExpiry) {
        // this is a new contact --> play a new sound
        var soundIndex = Math.floor((Math.random() * hitSounds.length) % hitSounds.length);
        audioOptions.position = collision.contactPoint;
        Audio.playSound(hitSounds[soundIndex], audioOptions);

        // bump the expiry
        soundExpiry = msec + SOUND_TRIGGER_CLEAR;

        // log the collision info
        Uuid.print("my sessionID = ", mySessionID);
        Uuid.print(" their sessionID = ", theirSessionID);
        Vec3.print("  penetration = ", collision.penetration);
        Vec3.print("   contactPoint = ", collision.contactPoint);
    } else {
        // this is a recurring contact --> continue to delay sound trigger
        soundExpiry = msec + SOUND_TRIGGER_DELAY;
    }
}
MyAvatar.collisionWithAvatar.connect(playHitSound);
