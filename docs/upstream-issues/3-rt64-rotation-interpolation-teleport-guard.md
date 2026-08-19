# [RT64] Frame interpolation: rotation component interpolates unconditionally (existing FIXME), causing full-screen glitch frames

**Repo:** rt64 (`src/hle/rt64_rigid_body.cpp`)

## Summary

`RigidBody::updateLinear` has a teleport heuristic for translation
(velocity-tolerance + magnitude-ratio), but `updateAngular`'s AUTO path is:

```cpp
// FIXME: Defaults to always interpolate.
lerpRotation = true;
```

When frame matching pairs up unrelated transforms — easy in games that recycle
per-frame dynamic matrix buffers, like GoldenEye — the bogus "rotation" between
two different objects is smoothly interpolated, sweeping stretched geometry
across the entire screen for a frame. In GoldenEye this manifested as a
full-screen flash every few seconds, only while the player moves (matching only
degrades when the visible set changes).

## Fix that worked

Mirror the linear heuristic for the angular path: interpolate steady rotations
(even fast ones), snap sudden jumps. Also clamp the `acos` input, which can
exceed [-1, 1] by float error for near-identity rotations (producing NaN
angular velocity).

```cpp
float traceCos = std::min(std::max((diffTrace - 1.0f) / 2.0f, -1.0f), 1.0f);
float prevAngularVelocity = angularVelocity;
float curAngularVelocity = std::acos(traceCos);
angularVelocity = curAngularVelocity;

const float Epsilon = 1e-6f;
const float AngularVelocityTolerance = 0.35f; // rad/frame (~20 deg)
const float AngularMagnitudeThreshold = 10.0f;
lerpRotation = (curAngularVelocity < AngularVelocityTolerance) ||
               ((curAngularVelocity / std::max(prevAngularVelocity, Epsilon)) < AngularMagnitudeThreshold);
```

Verified: eliminates the glitch frames in GoldenEye while keeping legitimately
fast steady rotations (spinning intro logos, doors) smoothly interpolated.
Happy to PR.
