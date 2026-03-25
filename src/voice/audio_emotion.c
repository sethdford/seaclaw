#include "human/voice/audio_emotion.h"
#include <math.h>
#include <string.h>

hu_error_t hu_audio_features_extract(const int16_t *pcm, size_t sample_count, uint32_t sample_rate,
                                     hu_audio_features_t *out) {
    if (!pcm || sample_count == 0 || !out || sample_rate == 0)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));

    /* RMS energy */
    double sum_sq = 0.0;
    size_t silence_samples = 0;
    const int16_t silence_thresh = 500;

    for (size_t i = 0; i < sample_count; i++) {
        double s = (double)pcm[i];
        sum_sq += s * s;
        if (pcm[i] > -silence_thresh && pcm[i] < silence_thresh)
            silence_samples++;
    }

    double rms = sqrt(sum_sq / (double)sample_count);
    out->energy_db = (rms > 0) ? (float)(20.0 * log10(rms / 32768.0)) : -96.0f;
    out->silence_ratio = (float)silence_samples / (float)sample_count;

    /* Zero-crossing rate as pitch proxy */
    size_t zero_crossings = 0;
    for (size_t i = 1; i < sample_count; i++) {
        if ((pcm[i - 1] >= 0 && pcm[i] < 0) || (pcm[i - 1] < 0 && pcm[i] >= 0))
            zero_crossings++;
    }
    float duration = (float)sample_count / (float)sample_rate;
    out->pitch_mean_hz = (duration > 0) ? (float)zero_crossings / (2.0f * duration) : 0.0f;
    out->pitch_var_hz = out->pitch_mean_hz * 0.15f; /* approximate */

    /* Speaking rate: estimate from energy transitions */
    size_t energy_transitions = 0;
    bool prev_voiced = false;
    size_t window = sample_rate / 50; /* 20ms windows */
    if (window == 0)
        window = 1;
    for (size_t i = 0; i + window <= sample_count; i += window) {
        double win_energy = 0;
        for (size_t j = 0; j < window; j++) {
            double s = (double)pcm[i + j];
            win_energy += s * s;
        }
        bool voiced = (win_energy / (double)window) > (double)(silence_thresh * silence_thresh);
        if (voiced && !prev_voiced)
            energy_transitions++;
        prev_voiced = voiced;
    }
    out->speaking_rate = (duration > 0.1f) ? (float)energy_transitions / duration : 0.0f;
    out->valid = true;
    return HU_OK;
}

hu_error_t hu_audio_features_extract_f32(const float *pcm, size_t sample_count,
                                         uint32_t sample_rate, hu_audio_features_t *out) {
    if (!pcm || sample_count == 0 || !out || sample_rate == 0)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));

    double sum_sq = 0.0;
    size_t silence_samples = 0;
    const float silence_thresh = 0.015f;

    for (size_t i = 0; i < sample_count; i++) {
        double s = (double)pcm[i];
        sum_sq += s * s;
        if (pcm[i] > -silence_thresh && pcm[i] < silence_thresh)
            silence_samples++;
    }

    double rms = sqrt(sum_sq / (double)sample_count);
    out->energy_db = (rms > 0) ? (float)(20.0 * log10(rms)) : -96.0f;
    out->silence_ratio = (float)silence_samples / (float)sample_count;

    size_t zero_crossings = 0;
    for (size_t i = 1; i < sample_count; i++) {
        if ((pcm[i - 1] >= 0.0f && pcm[i] < 0.0f) || (pcm[i - 1] < 0.0f && pcm[i] >= 0.0f))
            zero_crossings++;
    }
    float duration = (float)sample_count / (float)sample_rate;
    out->pitch_mean_hz = (duration > 0) ? (float)zero_crossings / (2.0f * duration) : 0.0f;
    out->pitch_var_hz = out->pitch_mean_hz * 0.15f;

    size_t energy_transitions = 0;
    bool prev_voiced = false;
    size_t window = sample_rate / 50;
    if (window == 0)
        window = 1;
    for (size_t i = 0; i + window <= sample_count; i += window) {
        double win_energy = 0;
        for (size_t j = 0; j < window; j++) {
            double s = (double)pcm[i + j];
            win_energy += s * s;
        }
        bool voiced = (win_energy / (double)window) > (double)(silence_thresh * silence_thresh);
        if (voiced && !prev_voiced)
            energy_transitions++;
        prev_voiced = voiced;
    }
    out->speaking_rate = (duration > 0.1f) ? (float)energy_transitions / duration : 0.0f;
    out->valid = true;
    return HU_OK;
}

hu_error_t hu_audio_emotion_classify(const hu_audio_features_t *features,
                                     hu_voice_emotion_t *out_emotion, float *out_confidence) {
    if (!features || !out_emotion || !out_confidence)
        return HU_ERR_INVALID_ARGUMENT;

    if (!features->valid) {
        *out_emotion = HU_VOICE_EMOTION_NEUTRAL;
        *out_confidence = 0.0f;
        return HU_OK;
    }

    *out_emotion = HU_VOICE_EMOTION_NEUTRAL;
    *out_confidence = 0.3f;

    if (features->energy_db > -20.0f && features->pitch_var_hz > 30.0f) {
        if (features->speaking_rate > 4.0f) {
            *out_emotion = HU_VOICE_EMOTION_URGENCY;
            *out_confidence = 0.6f;
        } else {
            *out_emotion = HU_VOICE_EMOTION_EXCITEMENT;
            *out_confidence = 0.55f;
        }
    } else if (features->energy_db < -35.0f && features->speaking_rate < 2.5f) {
        if (features->silence_ratio > 0.4f) {
            *out_emotion = HU_VOICE_EMOTION_SADNESS;
            *out_confidence = 0.5f;
        } else {
            *out_emotion = HU_VOICE_EMOTION_CALM;
            *out_confidence = 0.45f;
        }
    } else if (features->energy_db > -30.0f && features->pitch_var_hz > 15.0f) {
        *out_emotion = HU_VOICE_EMOTION_JOY;
        *out_confidence = 0.4f;
    }

    return HU_OK;
}

hu_error_t hu_emotion_fuse(hu_voice_emotion_t text_emotion, float text_confidence,
                           hu_voice_emotion_t audio_emotion, float audio_confidence,
                           hu_voice_emotion_t *out_emotion, float *out_confidence) {
    if (!out_emotion || !out_confidence)
        return HU_ERR_INVALID_ARGUMENT;

    /* If both agree, boost confidence */
    if (text_emotion == audio_emotion) {
        *out_emotion = text_emotion;
        *out_confidence = fminf(1.0f, text_confidence + audio_confidence * 0.3f);
        return HU_OK;
    }

    /* Text signal is generally more reliable for semantic emotion */
    float text_weight = 0.65f;
    float audio_weight = 0.35f;

    float text_score = text_confidence * text_weight;
    float audio_score = audio_confidence * audio_weight;

    if (text_score >= audio_score) {
        *out_emotion = text_emotion;
        *out_confidence = text_score;
    } else {
        *out_emotion = audio_emotion;
        *out_confidence = audio_score;
    }

    return HU_OK;
}
