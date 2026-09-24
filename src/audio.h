#ifndef EIK_AUDIO_H
#define EIK_AUDIO_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"

#define EIK_AUDIO_ALIAS_COUNT 4U

typedef enum EikAudioSound {
    EIK_AUDIO_JUMP,
    EIK_AUDIO_HIT,
    EIK_AUDIO_COLLECT,
    EIK_AUDIO_BOUNCE,
    EIK_AUDIO_DISAPPEAR,
    EIK_AUDIO_BUTTON_CLICK,
    EIK_AUDIO_SOUND_COUNT,
} EikAudioSound;

typedef enum EikAudioState {
    EIK_AUDIO_MAIN_MENU,
    EIK_AUDIO_ABOUT,
    EIK_AUDIO_OPTIONS,
    EIK_AUDIO_PLAYING,
} EikAudioState;

typedef struct EikAudioSettings {
    bool play_sounds;
    float sound_volume;
} EikAudioSettings;

typedef struct EikAudioPaths {
    const char *sounds[EIK_AUDIO_SOUND_COUNT];
    const char *menu_music;
} EikAudioPaths;

typedef struct EikAudioVoicePool {
    Sound voices[EIK_AUDIO_ALIAS_COUNT];
    unsigned int next_voice;
    bool loaded;
} EikAudioVoicePool;

typedef struct EikAudio {
    EikAudioVoicePool sounds[EIK_AUDIO_SOUND_COUNT];
    Music menu_music;
    EikAudioSettings settings;
    EikAudioState state;
    float music_volume;
    float music_target_volume;
    float music_fade_rate;
    bool initialized;
    bool music_loaded;
    bool music_playing;
    bool paused;
} EikAudio;

bool eik_audio_init(EikAudio *audio, const EikAudioPaths *paths, char *error,
    size_t error_size);
void eik_audio_unload(EikAudio *audio);
void eik_audio_set_settings(EikAudio *audio, EikAudioSettings settings);
void eik_audio_set_state(EikAudio *audio, EikAudioState state);
void eik_audio_play(EikAudio *audio, EikAudioSound sound);
void eik_audio_play_hover(EikAudio *audio);
void eik_audio_update(EikAudio *audio, float real_dt);
void eik_audio_pause(EikAudio *audio);
void eik_audio_resume(EikAudio *audio);

#endif
