// Animation playback: applying timelines to a Skeleton, and AnimationState mixing.
#pragma once
#include "Skeleton.h"
#include <functional>
#include <memory>

namespace osteon {

/** How a timeline's value combines with the current pose (Spine's MixBlend semantics). */
enum class MixBlend { Setup, First, Replace, Add };

struct FiredEvent {
    int event = -1; // index into SkeletonData::events
    float time = 0;
    int intValue = 0;
    float floatValue = 0;
    std::string stringValue;
    float volume = 1, balance = 0;
};

/** Events whose keys the playhead passed moving forward from lastTime to time (lastTime < key <= time). With `loop`, a time that
 *  is smaller than lastTime means the animation wrapped: the keys after lastTime and up to `time` both count. */
std::vector<FiredEvent> eventsBetween(const SkeletonData& data, const Animation& anim, float lastTime, float time, bool loop);

/** Interpolated value `valueIndex` of a curve timeline at `time` (clamped to the key range). */
float timelineValue(const Timeline& tl, int valueIndex, float time);
/** Index of the key at or before `time`, or -1 if before the first key. */
int keyBefore(const Timeline& tl, float time);

/**
 * Applies `anim` to `sk`. Time wraps when `loop`. Events with lastTime < t <= time are appended
 * to `events` (if non-null). `alpha` blends between the current pose and the animation.
 */
void applyAnimation(const Animation& anim, Skeleton& sk, float lastTime, float time, bool loop,
                    std::vector<FiredEvent>* events, float alpha, MixBlend blend);

/** Track-based playback with crossfade mixing (used by the Preview view and playback). */
class AnimationState {
public:
    struct Entry {
        int animation = -1;
        bool loop = true;
        float trackTime = 0, lastTime = -1, timeScale = 1, alpha = 1;
        float mixTime = 0, mixDuration = 0, delay = 0;
        float animationStart = 0, animationEnd = 0;
        std::shared_ptr<Entry> from;
        std::shared_ptr<Entry> next;
    };

    explicit AnimationState(const SkeletonData* data = nullptr) : data(data) {}
    const SkeletonData* data;
    float timeScale = 1;
    float defaultMix = 0.2f;
    std::map<std::pair<int, int>, float> mixes; // (from, to) -> duration
    std::vector<std::shared_ptr<Entry>> tracks;
    std::function<void(int track, const FiredEvent&)> onEvent;

    std::shared_ptr<Entry> setAnimation(int track, int animation, bool loop);
    std::shared_ptr<Entry> addAnimation(int track, int animation, bool loop, float delay);
    void clearTrack(int track);
    void clearTracks() { tracks.clear(); }
    void update(float delta);
    void apply(Skeleton& sk);
    float mixDuration(int from, int to) const;
};

} // namespace osteon
