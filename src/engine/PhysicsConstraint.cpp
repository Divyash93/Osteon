// Physics constraint solver: fixed-step spring-damper secondary motion on one bone.
#include "Skeleton.h"

namespace osteon {

namespace {

constexpr float MAX_DELTA = 1.0f; // seconds simulated per update at most (large jumps are dropped)

void resetState(PhysicsConstraint& c, float time) {
    c.xOffset = c.xVelocity = c.yOffset = c.yVelocity = 0;
    c.rotateOffset = c.rotateVelocity = c.scaleOffset = c.scaleVelocity = 0;
    c.remaining = 0;
    c.lastTime = time;
    c.reset = true;
}

/** Clamps a displacement to `max` length. */
void limitMove(float& dx, float& dy, float max) {
    float l2 = dx * dx + dy * dy;
    if (l2 > max * max && l2 > 0) {
        float s = max / std::sqrt(l2);
        dx *= s;
        dy *= s;
    }
}

} // namespace

void applyPhysics(Skeleton& sk, int index, Physics mode) {
    if (mode == Physics::None) return;
    const PhysicsConstraintData& d = sk.data->physics[index];
    PhysicsConstraint& c = sk.physics[index];
    if (!c.active) return;
    Bone& bone = sk.bones[d.bone];
    bool moveX = d.x > 0, moveY = d.y > 0;
    bool rotateOrShear = d.rotate > 0 || d.shearX > 0, scaleX = d.scaleX > 0;
    float length = sk.data->bones[d.bone].length;
    // Rotation/scale need a lever arm: use the bone's world length.
    float armX = bone.a * length, armY = bone.c * length;
    float arm = std::sqrt(armX * armX + armY * armY);
    bool angular = (rotateOrShear || scaleX) && arm > 0.0001f;
    float bx = bone.worldX, by = bone.worldY, tipX = bx + armX, tipY = by + armY;

    if (mode == Physics::Reset) resetState(c, sk.time);
    if (mode == Physics::Update) {
        float delta = sk.time - c.lastTime;
        c.lastTime = sk.time;
        if (c.reset || delta < 0) {
            // First tick after a reset (or the clock went backwards): start tracking, no impulse.
            c.reset = false;
        } else if (delta > 0) {
            float maxMove = d.limit * std::min(delta, MAX_DELTA);
            float inertia = c.inertia;
            if (moveX || moveY) {
                // The bone moved; the simulated point lags behind by the inverse of the motion.
                float dx = c.ux - bx, dy = c.uy - by;
                limitMove(dx, dy, maxMove);
                if (moveX) c.xOffset += dx * inertia;
                if (moveY) c.yOffset += dy * inertia;
            }
            if (angular) {
                // The tip wants to stay where it was: rotate/stretch toward the old tip from the new base.
                float px = c.cx - bx, py = c.cy - by;
                float mx = px - armX, my = py - armY; // tip lag relative to the new pose
                limitMove(mx, my, maxMove);
                px = armX + mx;
                py = armY + my;
                if (rotateOrShear) {
                    float dr = std::atan2(armX * py - armY * px, armX * px + armY * py);
                    c.rotateOffset += dr * inertia;
                }
                if (scaleX) c.scaleOffset += ((px * armX + py * armY) / (arm * arm) - 1) * inertia;
            }
            c.remaining += std::min(delta, MAX_DELTA);
            float step = d.step > 0 ? d.step : 1.0f / 60;
            float m = c.massInverse * step, k = c.strength;
            float damp = std::pow(clampf(c.damping, 0, 1), 60 * step);
            float ref = sk.data->referenceScale;
            float fx = c.wind * ref * sk.scaleX, fy = -c.gravity * ref * sk.scaleY;
            // External force direction relative to the bone, for angular response.
            float ux = arm > 0 ? armX / arm : 1, uy = arm > 0 ? armY / arm : 0;
            float torque = angular ? (ux * fy - uy * fx) / arm : 0;
            float stretch = angular ? (ux * fx + uy * fy) / arm : 0;
            while (c.remaining >= step) {
                c.remaining -= step;
                if (moveX) {
                    c.xVelocity += (fx - c.xOffset * k) * m;
                    c.xOffset += c.xVelocity * step;
                    c.xVelocity *= damp;
                }
                if (moveY) {
                    c.yVelocity += (fy - c.yOffset * k) * m;
                    c.yOffset += c.yVelocity * step;
                    c.yVelocity *= damp;
                }
                if (rotateOrShear) {
                    c.rotateVelocity += (torque - c.rotateOffset * k) * m;
                    c.rotateOffset += c.rotateVelocity * step;
                    c.rotateVelocity *= damp;
                }
                if (scaleX) {
                    c.scaleVelocity += (stretch - c.scaleOffset * k) * m;
                    c.scaleOffset += c.scaleVelocity * step;
                    c.scaleVelocity *= damp;
                }
            }
        }
    }
    // Remember the unconstrained pose for the next tick's motion.
    if (mode != Physics::Pose || c.reset) {
        c.ux = bx; c.uy = by;
        c.cx = tipX; c.cy = tipY;
    }

    // ---- Apply offsets, blended by mix.
    float mix = c.mix;
    if (mix == 0) return;
    if (moveX) bone.worldX += c.xOffset * mix * d.x;
    if (moveY) bone.worldY += c.yOffset * mix * d.y;
    if (angular) {
        float r = c.rotateOffset * mix * d.rotate;
        if (r != 0) {
            float cs = std::cos(r), sn = std::sin(r), a = bone.a, b = bone.b, cc = bone.c, dd = bone.d;
            bone.a = cs * a - sn * cc;
            bone.b = cs * b - sn * dd;
            bone.c = sn * a + cs * cc;
            bone.d = sn * b + cs * dd;
        }
        float sh = c.rotateOffset * mix * d.shearX; // shear: rotate only the x axis
        if (sh != 0) {
            float cs = std::cos(sh), sn = std::sin(sh), a = bone.a, cc = bone.c;
            bone.a = cs * a - sn * cc;
            bone.c = sn * a + cs * cc;
        }
        if (scaleX) {
            float s = 1 + c.scaleOffset * mix * d.scaleX;
            if (s > 0) { bone.a *= s; bone.c *= s; }
        }
    }
    bone.updateAppliedTransform(sk);
}

} // namespace osteon
