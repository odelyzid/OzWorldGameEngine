#ifndef OZ_AUDIO_H
#define OZ_AUDIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal music playback API backed by SDL_mixer when available.
// These functions are no-ops and return false when audio is disabled
// or the backend is unavailable at build/runtime.

bool oz_audio_init(void);           // initialize audio subsystem
void oz_audio_shutdown(void);       // shutdown and free resources

// Play a music file from disk. Loops: -1 infinite, 0 once, N repeats.
// Path may be .ozmux (mp3 content) or any SDL_mixer-supported format.
bool oz_audio_play_music(const char* path, int loops);
void oz_audio_stop_music(void);

#ifdef __cplusplus
}
#endif

#endif // OZ_AUDIO_H
