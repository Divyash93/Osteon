// Plays event audio during playback (Windows MCI: wav, mp3 and other formats the system can decode).
#pragma once
#include <string>

namespace osteon {

/** Starts a sound without blocking. volume 0..1. Returns false when the file cannot be played. */
bool playAudioFile(const std::string& path, float volume);
/** Stops and releases every sound started by playAudioFile. */
void stopAllAudio();

} // namespace osteon
