#include "Animation.h"

namespace osteon {

int keyBefore(const Timeline& tl, float time) {
    int i = -1;
    for (size_t k = 0; k < tl.keys.size(); k++) {
        if (tl.keys[k].time <= time) i = (int)k;
        else break;
    }
    return i;
}

float timelineValue(const Timeline& tl, int vi, float time) {
    if (tl.keys.empty()) return 0;
    int i = keyBefore(tl, time);
    if (i < 0) return tl.keys[0].v[vi];
    const Key& k0 = tl.keys[i];
    if (i + 1 >= (int)tl.keys.size()) return k0.v[vi];
    const Key& k1 = tl.keys[i + 1];
    switch (k0.curve) {
    case CurveType::Stepped: return k0.v[vi];
    case CurveType::Bezier:
        if ((int)k0.bezier.size() >= vi * 4 + 4) {
            const float* c = &k0.bezier[vi * 4];
            return bezierValue(time, k0.time, k0.v[vi], c[0], c[1], c[2], c[3], k1.time, k1.v[vi]);
        }
        [[fallthrough]];
    default: {
        float span = k1.time - k0.time;
        float t = span > 0 ? (time - k0.time) / span : 0;
        return k0.v[vi] + (k1.v[vi] - k0.v[vi]) * t;
    }
    }
}

// Curve percent between two keys, for interpolating non-scalar data (deform vertices).
static float curvePercent(const Timeline& tl, int i, float time) {
    const Key& k0 = tl.keys[i];
    const Key& k1 = tl.keys[i + 1];
    if (k0.curve == CurveType::Stepped) return 0;
    float span = k1.time - k0.time;
    if (span <= 0) return 0;
    if (k0.curve == CurveType::Bezier && k0.bezier.size() >= 4) // deform curves store y as 0..1 percent
        return bezierValue(time, k0.time, 0, k0.bezier[0], k0.bezier[1], k0.bezier[2], k0.bezier[3], k1.time, 1);
    return (time - k0.time) / span;
}

static void applyRotate(const Timeline& tl, Skeleton& sk, float time, float alpha, MixBlend blend) {
    Bone& bone = sk.bones[tl.target];
    if (!bone.active) return;
    const BoneData& bd = sk.data->bones[tl.target];
    if (time < tl.keys[0].time) {
        if (blend == MixBlend::Setup) bone.rotation = bd.rotation;
        else if (blend == MixBlend::First) bone.rotation += (bd.rotation - bone.rotation) * alpha;
        return;
    }
    float r = timelineValue(tl, 0, time);
    switch (blend) {
    case MixBlend::Setup: bone.rotation = bd.rotation + r * alpha; break;
    case MixBlend::First:
    case MixBlend::Replace: r += bd.rotation - bone.rotation; bone.rotation += r * alpha; break;
    case MixBlend::Add: bone.rotation += r * alpha; break;
    }
}

// Translate and shear are additive offsets from the setup pose.
static void applyOffset(float& value, float setup, float v, float alpha, MixBlend blend, bool before) {
    if (before) {
        if (blend == MixBlend::Setup) value = setup;
        else if (blend == MixBlend::First) value += (setup - value) * alpha;
        return;
    }
    switch (blend) {
    case MixBlend::Setup: value = setup + v * alpha; break;
    case MixBlend::First:
    case MixBlend::Replace: value += (setup + v - value) * alpha; break;
    case MixBlend::Add: value += v * alpha; break;
    }
}

// Scale is multiplicative relative to the setup pose.
static void applyScale(float& value, float setup, float v, float alpha, MixBlend blend, bool before) {
    if (before) {
        if (blend == MixBlend::Setup) value = setup;
        else if (blend == MixBlend::First) value += (setup - value) * alpha;
        return;
    }
    float x = v * setup;
    if (alpha == 1) {
        if (blend == MixBlend::Add) value += x - setup;
        else value = x;
        return;
    }
    switch (blend) {
    case MixBlend::Setup: value = setup + (x - setup) * alpha; break;
    case MixBlend::First:
    case MixBlend::Replace: value += (x - value) * alpha; break;
    case MixBlend::Add: value += (x - setup) * alpha; break;
    }
}

static void applyBoneTimeline(const Timeline& tl, Skeleton& sk, float time, float alpha, MixBlend blend) {
    Bone& b = sk.bones[tl.target];
    if (!b.active) return;
    const BoneData& d = sk.data->bones[tl.target];
    bool before = time < tl.keys[0].time;
    auto v = [&](int i) { return timelineValue(tl, i, time); };
    switch (tl.type) {
    case TimelineType::Rotate: applyRotate(tl, sk, time, alpha, blend); break;
    case TimelineType::Translate:
        applyOffset(b.x, d.x, before ? 0 : v(0), alpha, blend, before);
        applyOffset(b.y, d.y, before ? 0 : v(1), alpha, blend, before);
        break;
    case TimelineType::TranslateX: applyOffset(b.x, d.x, before ? 0 : v(0), alpha, blend, before); break;
    case TimelineType::TranslateY: applyOffset(b.y, d.y, before ? 0 : v(0), alpha, blend, before); break;
    case TimelineType::Scale:
        applyScale(b.scaleX, d.scaleX, before ? 1 : v(0), alpha, blend, before);
        applyScale(b.scaleY, d.scaleY, before ? 1 : v(1), alpha, blend, before);
        break;
    case TimelineType::ScaleX: applyScale(b.scaleX, d.scaleX, before ? 1 : v(0), alpha, blend, before); break;
    case TimelineType::ScaleY: applyScale(b.scaleY, d.scaleY, before ? 1 : v(0), alpha, blend, before); break;
    case TimelineType::Shear:
        applyOffset(b.shearX, d.shearX, before ? 0 : v(0), alpha, blend, before);
        applyOffset(b.shearY, d.shearY, before ? 0 : v(1), alpha, blend, before);
        break;
    case TimelineType::ShearX: applyOffset(b.shearX, d.shearX, before ? 0 : v(0), alpha, blend, before); break;
    case TimelineType::ShearY: applyOffset(b.shearY, d.shearY, before ? 0 : v(0), alpha, blend, before); break;
    case TimelineType::Inherit: {
        if (before) { if (blend == MixBlend::Setup || blend == MixBlend::First) b.inherit = d.inherit; break; }
        int i = keyBefore(tl, time);
        b.inherit = (Inherit)tl.keys[i].intValue;
        break;
    }
    default: break;
    }
}

static void mixColor(Color& c, const Color& setup, const Color& v, float alpha, MixBlend blend, bool rgbOnly) {
    if (alpha == 1) {
        c.r = v.r; c.g = v.g; c.b = v.b;
        if (!rgbOnly) c.a = v.a;
        return;
    }
    if (blend == MixBlend::Setup) {
        c.r = setup.r; c.g = setup.g; c.b = setup.b;
        if (!rgbOnly) c.a = setup.a;
    }
    c.r += (v.r - c.r) * alpha; c.g += (v.g - c.g) * alpha; c.b += (v.b - c.b) * alpha;
    if (!rgbOnly) c.a += (v.a - c.a) * alpha;
}

static void applySlotTimeline(const Timeline& tl, Skeleton& sk, float time, float alpha, MixBlend blend) {
    Slot& s = sk.slots[tl.target];
    if (!sk.bones[s.bone].active) return;
    const SlotData& sd = sk.data->slots[tl.target];
    bool before = time < tl.keys[0].time;
    auto v = [&](int i) { return timelineValue(tl, i, time); };
    switch (tl.type) {
    case TimelineType::Attachment: {
        if (before) {
            if (blend == MixBlend::Setup || blend == MixBlend::First) s.attachment = sd.attachment;
            break;
        }
        const std::string& name = tl.keys[keyBefore(tl, time)].str;
        if (s.attachment != name) { s.attachment = name; s.deform.clear(); s.deformFor.clear(); }
        break;
    }
    case TimelineType::RGBA:
    case TimelineType::RGB:
    case TimelineType::Alpha:
    case TimelineType::RGBA2:
    case TimelineType::RGB2: {
        if (before) {
            if (blend == MixBlend::Setup) { s.color = sd.color; s.dark = sd.dark; }
            else if (blend == MixBlend::First) {
                mixColor(s.color, sd.color, sd.color, alpha, blend, false);
                mixColor(s.dark, sd.dark, sd.dark, alpha, blend, true);
            }
            break;
        }
        if (tl.type == TimelineType::Alpha) {
            float a = v(0);
            if (alpha == 1) s.color.a = a;
            else {
                if (blend == MixBlend::Setup) s.color.a = sd.color.a;
                s.color.a += (a - s.color.a) * alpha;
            }
            break;
        }
        bool hasAlpha = tl.type == TimelineType::RGBA || tl.type == TimelineType::RGBA2;
        Color light(v(0), v(1), v(2), hasAlpha ? v(3) : s.color.a);
        mixColor(s.color, sd.color, light, alpha, blend, !hasAlpha);
        if (tl.type == TimelineType::RGBA2 || tl.type == TimelineType::RGB2) {
            int o = hasAlpha ? 4 : 3;
            mixColor(s.dark, sd.dark, Color(v(o), v(o + 1), v(o + 2), 1), alpha, blend, true);
        }
        break;
    }
    default: break;
    }
}

static void applyDeform(const Timeline& tl, Skeleton& sk, float time, float alpha, MixBlend blend) {
    Slot& s = sk.slots[tl.target];
    if (!sk.bones[s.bone].active) return;
    // A deform timeline applies to its attachment, or to linked meshes that inherit timelines.
    const Attachment* cur = sk.attachmentFor(s);
    if (!cur) return;
    bool match = cur->name == tl.attachment;
    if (!match && cur->type == AttachmentType::LinkedMesh && cur->inheritTimelines) match = cur->parentMesh == tl.attachment;
    if (!match) return;
    const Attachment* geom = cur;
    if (cur->type == AttachmentType::LinkedMesh) {
        int ps = cur->parentSkin.empty() ? 0 : sk.data->findSkin(cur->parentSkin);
        geom = sk.data->findAttachment(ps, s.index, cur->parentMesh);
        if (!geom) return;
    }
    size_t n = (size_t)geom->deformLength();
    if (s.deformFor != cur->name) { s.deform.clear(); s.deformFor = cur->name; }
    if (s.deform.size() != n) s.deform.assign(n, 0.0f);
    std::vector<float> target(n, 0.0f);
    if (time >= tl.keys[0].time) {
        int i = keyBefore(tl, time);
        auto keyVerts = [&](const Key& k, size_t j) { return j < k.deform.size() ? k.deform[j] : 0.0f; };
        if (i + 1 >= (int)tl.keys.size()) {
            for (size_t j = 0; j < n; j++) target[j] = keyVerts(tl.keys[i], j);
        } else {
            float p = curvePercent(tl, i, time);
            for (size_t j = 0; j < n; j++) {
                float a = keyVerts(tl.keys[i], j), b = keyVerts(tl.keys[i + 1], j);
                target[j] = a + (b - a) * p;
            }
        }
    } else if (blend != MixBlend::Setup && blend != MixBlend::First) {
        return;
    }
    for (size_t j = 0; j < n; j++) {
        switch (blend) {
        case MixBlend::Setup: s.deform[j] = target[j] * alpha; break;
        case MixBlend::Add: s.deform[j] += target[j] * alpha; break;
        default: s.deform[j] += (target[j] - s.deform[j]) * alpha; break;
        }
    }
}

// Sequence keys pick the frame of a frame-by-frame attachment: v[0] = delay per frame,
// intValue = start index, str = mode (hold, once, loop, pingpong and their *Reverse forms).
static void applySequence(const Timeline& tl, Skeleton& sk, float time, MixBlend blend) {
    Slot& s = sk.slots[tl.target];
    if (!sk.bones[s.bone].active) return;
    const Attachment* att = sk.attachmentFor(s);
    if (!att || att->name != tl.attachment || att->seqCount <= 0) return;
    if (time < tl.keys[0].time) {
        if (blend == MixBlend::Setup || blend == MixBlend::First) s.sequenceIndex = -1;
        return;
    }
    const Key& k = tl.keys[keyBefore(tl, time)];
    int count = att->seqCount, index = k.intValue;
    const std::string& mode = k.str;
    if (mode != "hold" && !mode.empty() && k.v[0] > 0) {
        index += (int)((time - k.time) / k.v[0] + 0.0001f);
        int n = count * 2 - 2;
        if (mode == "once") index = std::min(count - 1, index);
        else if (mode == "loop") index %= count;
        else if (mode == "pingpong") { index = n == 0 ? 0 : index % n; if (index >= count) index = n - index; }
        else if (mode == "onceReverse") index = std::max(count - 1 - index, 0);
        else if (mode == "loopReverse") index = count - 1 - (index % count);
        else if (mode == "pingpongReverse") { index = n == 0 ? 0 : (index + count - 1) % n; if (index >= count) index = n - index; }
    }
    s.sequenceIndex = std::max(0, std::min(count - 1, index));
}

static void applyDrawOrder(const Timeline& tl, Skeleton& sk, float time, MixBlend blend) {
    auto setup = [&] { for (size_t i = 0; i < sk.drawOrder.size(); i++) sk.drawOrder[i] = (int)i; };
    if (time < tl.keys[0].time) {
        if (blend == MixBlend::Setup || blend == MixBlend::First) setup();
        return;
    }
    const Key& k = tl.keys[keyBefore(tl, time)];
    if (k.drawOrder.size() != sk.drawOrder.size()) setup();
    else sk.drawOrder = k.drawOrder;
}

static void fireEvents(const Timeline& tl, const SkeletonData& data, float lastTime, float time, std::vector<FiredEvent>* out) {
    if (!out) return;
    for (const Key& k : tl.keys) {
        if (k.time > lastTime && k.time <= time) {
            FiredEvent e;
            e.event = data.findEvent(k.str);
            e.time = k.time;
            e.intValue = k.intValue;
            e.floatValue = k.v[0];
            e.volume = k.volume;
            e.balance = k.balance;
            if (e.event >= 0) {
                const EventData& ed = data.events[e.event];
                // Keys only store overrides; fall back to event defaults like Spine does.
                if (!k.flag1) e.intValue = ed.intValue;
                if (!k.flag2) e.floatValue = ed.floatValue;
                e.stringValue = k.flag3 ? k.str2 : ed.stringValue;
            }
            out->push_back(e);
        }
    }
}

static void applyConstraintTimeline(const Timeline& tl, Skeleton& sk, float time, float alpha, MixBlend blend) {
    bool before = time < tl.keys[0].time;
    auto v = [&](int i) { return timelineValue(tl, i, time); };
    auto mix = [&](float& value, float setup, float target) {
        if (before) {
            if (blend == MixBlend::Setup) value = setup;
            else if (blend == MixBlend::First) value += (setup - value) * alpha;
            return;
        }
        if (blend == MixBlend::Setup) value = setup + (target - setup) * alpha;
        else value += (target - value) * alpha;
    };
    switch (tl.type) {
    case TimelineType::Ik: {
        if (tl.target >= (int)sk.ik.size()) return;
        IkConstraint& c = sk.ik[tl.target];
        const IkConstraintData& d = sk.data->ik[tl.target];
        mix(c.mix, d.mix, v(0));
        mix(c.softness, d.softness, v(1));
        if (before) {
            if (blend == MixBlend::Setup || blend == MixBlend::First) {
                c.bendDirection = d.bendDirection; c.compress = d.compress; c.stretch = d.stretch;
            }
        } else {
            const Key& k = tl.keys[keyBefore(tl, time)];
            c.bendDirection = k.flag1 ? 1 : -1;
            c.compress = k.flag2;
            c.stretch = k.flag3;
        }
        break;
    }
    case TimelineType::Transform: {
        if (tl.target >= (int)sk.transforms.size()) return;
        TransformConstraint& c = sk.transforms[tl.target];
        const TransformConstraintData& d = sk.data->transforms[tl.target];
        mix(c.mixRotate, d.mixRotate, v(0)); mix(c.mixX, d.mixX, v(1)); mix(c.mixY, d.mixY, v(2));
        mix(c.mixScaleX, d.mixScaleX, v(3)); mix(c.mixScaleY, d.mixScaleY, v(4)); mix(c.mixShearY, d.mixShearY, v(5));
        break;
    }
    case TimelineType::Path: {
        if (tl.target >= (int)sk.paths.size()) return;
        mix(sk.paths[tl.target].position, sk.data->paths[tl.target].position, v(0));
        break;
    }
    case TimelineType::PathMix: {
        if (tl.target >= (int)sk.paths.size()) return;
        PathConstraint& c = sk.paths[tl.target];
        const PathConstraintData& d = sk.data->paths[tl.target];
        mix(c.mixRotate, d.mixRotate, v(0)); mix(c.mixX, d.mixX, v(1)); mix(c.mixY, d.mixY, v(2));
        break;
    }
    default: break;
    }
}

void applyAnimation(const Animation& anim, Skeleton& sk, float lastTime, float time, bool loop,
                    std::vector<FiredEvent>* events, float alpha, MixBlend blend) {
    float duration = anim.duration();
    if (loop && duration > 0) {
        time = std::fmod(time, duration);
        if (lastTime > 0) lastTime = std::fmod(lastTime, duration);
    }
    for (const Timeline& tl : anim.timelines) {
        if (tl.keys.empty()) continue;
        if (tl.type == TimelineType::Event) {
            if (lastTime > time) { // wrapped around the loop
                fireEvents(tl, *sk.data, lastTime, duration, events);
                fireEvents(tl, *sk.data, -1, time, events);
            } else {
                fireEvents(tl, *sk.data, lastTime, time, events);
            }
            continue;
        }
        if (isBoneTimeline(tl.type)) {
            if (tl.target >= 0 && tl.target < (int)sk.bones.size()) applyBoneTimeline(tl, sk, time, alpha, blend);
        } else if (isSlotTimeline(tl.type)) {
            if (tl.target >= 0 && tl.target < (int)sk.slots.size()) applySlotTimeline(tl, sk, time, alpha, blend);
        } else if (tl.type == TimelineType::Deform) {
            if (tl.target >= 0 && tl.target < (int)sk.slots.size()) applyDeform(tl, sk, time, alpha, blend);
        } else if (tl.type == TimelineType::Sequence) {
            if (tl.target >= 0 && tl.target < (int)sk.slots.size()) applySequence(tl, sk, time, blend);
        } else if (tl.type == TimelineType::DrawOrder) {
            if (alpha >= 0.5f || blend == MixBlend::Setup) applyDrawOrder(tl, sk, time, blend);
        } else {
            applyConstraintTimeline(tl, sk, time, alpha, blend);
        }
    }
}

// ---- AnimationState -------------------------------------------------------------------------

float AnimationState::mixDuration(int from, int to) const {
    auto it = mixes.find({from, to});
    return it != mixes.end() ? it->second : defaultMix;
}

std::shared_ptr<AnimationState::Entry> AnimationState::setAnimation(int track, int animation, bool loop) {
    if ((int)tracks.size() <= track) tracks.resize(track + 1);
    auto e = std::make_shared<Entry>();
    e->animation = animation;
    e->loop = loop;
    if (data && animation >= 0) e->animationEnd = data->animations[animation].duration();
    auto cur = tracks[track];
    if (cur && cur->animation >= 0) {
        e->from = cur;
        cur->next.reset();
        e->mixDuration = mixDuration(cur->animation, animation);
    }
    tracks[track] = e;
    return e;
}

std::shared_ptr<AnimationState::Entry> AnimationState::addAnimation(int track, int animation, bool loop, float delay) {
    if ((int)tracks.size() <= track || !tracks[track]) return setAnimation(track, animation, loop);
    auto last = tracks[track];
    while (last->next) last = last->next;
    auto e = std::make_shared<Entry>();
    e->animation = animation;
    e->loop = loop;
    if (data && animation >= 0) e->animationEnd = data->animations[animation].duration();
    if (delay <= 0) {
        float d = last->animationEnd - last->animationStart;
        delay = std::max(0.0f, d + delay - mixDuration(last->animation, animation));
    }
    e->delay = delay;
    last->next = e;
    return e;
}

void AnimationState::clearTrack(int track) {
    if (track < (int)tracks.size()) tracks[track].reset();
}

void AnimationState::update(float delta) {
    delta *= timeScale;
    for (auto& t : tracks) {
        if (!t) continue;
        float d = delta * t->timeScale;
        t->lastTime = t->trackTime;
        t->trackTime += d;
        if (t->from) {
            t->mixTime += d;
            if (t->mixTime >= t->mixDuration) t->from.reset();
            else { t->from->lastTime = t->from->trackTime; t->from->trackTime += d; }
        }
        if (t->next) {
            float dur = t->animationEnd - t->animationStart;
            if (t->trackTime >= t->next->delay || (!t->loop && t->trackTime >= dur && t->next->delay <= t->trackTime)) {
                auto n = t->next;
                n->from = t;
                n->mixDuration = mixDuration(t->animation, n->animation);
                t->next.reset();
                t = n;
            }
        }
    }
}

void AnimationState::apply(Skeleton& sk) {
    std::vector<FiredEvent> events;
    for (size_t ti = 0; ti < tracks.size(); ti++) {
        auto& t = tracks[ti];
        if (!t || t->animation < 0) continue;
        MixBlend blend = ti == 0 ? MixBlend::First : MixBlend::Replace;
        float alpha = t->alpha;
        if (t->from && t->from->animation >= 0) {
            const Animation& fa = data->animations[t->from->animation];
            applyAnimation(fa, sk, t->from->lastTime, t->from->trackTime, t->from->loop, nullptr, 1, blend);
            blend = MixBlend::Replace;
            alpha *= t->mixDuration > 0 ? std::min(1.0f, t->mixTime / t->mixDuration) : 1.0f;
        }
        const Animation& a = data->animations[t->animation];
        float dur = a.duration();
        float time = t->loop ? t->trackTime : std::min(t->trackTime, dur);
        float last = t->lastTime < 0 ? -1 : (t->loop ? t->lastTime : std::min(t->lastTime, dur));
        size_t before = events.size();
        applyAnimation(a, sk, last, time, t->loop, &events, alpha, blend);
        if (onEvent)
            for (size_t i = before; i < events.size(); i++) onEvent((int)ti, events[i]);
    }
}

} // namespace osteon
