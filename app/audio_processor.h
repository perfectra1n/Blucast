// ══════════════════════════════════════════════════════════════════════════
// AudioFX Processor
//
// Mirrors VideoFXProcessor (server.cpp) but for NVIDIA Maxine *Audio* Effects
// (NvAFX). Same lifecycle as the video side: Create → Set params → Load → Run
// → Destroy. Processes mono float32 PCM in fixed-size frames.
//
// NOTE ON SDK VERSIONS: the NVAFX_* selector/parameter macros and the model
// file names below come from the documented AFX 2.x API. If the vendored SDK
// (sdk/AudioFX) ships different macro names (the 1.x SDK used NVAFX_PARAM_DENOISER_*
// style names) or differently-named .trtpkg model files, adjust selectorFor(),
// modelPathFor(), and the param macros to match the installed nvAudioEffects.h.
// ══════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "nvAudioEffects.h"

//  Audio effect modes ──────────────────────────────────────────────────────
//  Int values are the wire-protocol values emitted by the GUI (AUDIO_MODE:n),
//  exactly like EffectMode for the video side.
enum AudioEffectMode {
    AMODE_NONE     = 0,   // passthrough
    AMODE_DENOISE  = 1,   // NVAFX_EFFECT_DENOISER
    AMODE_DEREVERB = 2,   // NVAFX_EFFECT_DEREVERB
    AMODE_STUDIO   = 3,   // NVAFX_EFFECT_STUDIO_VOICE_* (voice enhancement)
};

class AudioFXProcessor {
public:
    AudioFXProcessor() = default;
    ~AudioFXProcessor() { destroy(); }

    // Store the model directory. Effects are created lazily in setMode() (each
    // effect loads a separate TensorRT engine + GPU memory, so we only ever
    // hold the active one — unlike the video side which creates all effects up
    // front).
    bool init(const std::string &modelDir) {
        modelDir_ = modelDir;
        // Passthrough geometry until a real effect is loaded.
        inputSampleRate_    = 48000;
        outputSampleRate_   = 48000;
        inSamplesPerFrame_  = kPassthroughFrame;
        outSamplesPerFrame_ = kPassthroughFrame;
        return true;
    }

    // Switch effect / update intensity. Returns false if a requested effect
    // failed to create/load (caller falls back to passthrough). Intensity-only
    // changes update live with no reload (mirrors how BLUR: tweaks the video
    // blur strength without reloading the model).
    bool setMode(int mode, float intensity) {
        intensity_ = clampUnit(intensity);

        if (mode == curMode_) {
            if (handle_ && mode != AMODE_NONE) {
                NvAFX_SetFloat(handle_, NVAFX_PARAM_INTENSITY_RATIO, intensity_);
            }
            return true;
        }

        // Tear down the previous effect before loading the next (bounds VRAM).
        if (handle_) {
            NvAFX_DestroyEffect(handle_);
            handle_ = nullptr;
        }

        if (mode == AMODE_NONE) {
            curMode_            = AMODE_NONE;
            inputSampleRate_    = 48000;
            outputSampleRate_   = 48000;
            inSamplesPerFrame_  = kPassthroughFrame;
            outSamplesPerFrame_ = kPassthroughFrame;
            return true;
        }

        const char *selector = selectorFor(mode);
        if (!selector) {
            std::cerr << "AudioFX: unknown effect mode " << mode << std::endl;
            curMode_ = AMODE_NONE;
            return false;
        }

        NvAFX_Handle h = nullptr;
        if (NvAFX_CreateEffect(selector, &h) != NVAFX_STATUS_SUCCESS) {
            std::cerr << "AudioFX: CreateEffect failed for mode " << mode << std::endl;
            curMode_ = AMODE_NONE;
            return false;
        }

        const unsigned reqRate = preferredInputRate(mode);
        const std::string modelPath = modelPathFor(mode, reqRate);

        NvAFX_SetString(h, NVAFX_PARAM_MODEL_PATH,        modelPath.c_str());
        NvAFX_SetU32   (h, NVAFX_PARAM_NUM_STREAMS,       1);
        NvAFX_SetU32   (h, NVAFX_PARAM_INPUT_SAMPLE_RATE, reqRate);
        NvAFX_SetU32   (h, NVAFX_PARAM_NUM_INPUT_CHANNELS, 1);
        NvAFX_SetFloat (h, NVAFX_PARAM_INTENSITY_RATIO,   intensity_);

        std::cout << "AudioFX: loading " << selector << " (" << modelPath << ")..." << std::endl;
        if (NvAFX_Load(h) != NVAFX_STATUS_SUCCESS) {
            std::cerr << "AudioFX: Load failed (mode " << mode
                      << ", model " << modelPath << ")" << std::endl;
            NvAFX_DestroyEffect(h);
            curMode_ = AMODE_NONE;
            return false;
        }

        // Read back the authoritative geometry the model actually wants
        // (analogous to re-reading the real camera resolution after open()).
        unsigned spfIn = 0, spfOut = 0, inSr = reqRate, outSr = reqRate;
        NvAFX_GetU32(h, NVAFX_PARAM_NUM_INPUT_SAMPLES_PER_FRAME, &spfIn);
        NvAFX_GetU32(h, NVAFX_PARAM_INPUT_SAMPLE_RATE,           &inSr);
        if (NvAFX_GetU32(h, NVAFX_PARAM_OUTPUT_SAMPLE_RATE, &outSr) != NVAFX_STATUS_SUCCESS)
            outSr = inSr;
        if (NvAFX_GetU32(h, NVAFX_PARAM_NUM_OUTPUT_SAMPLES_PER_FRAME, &spfOut) != NVAFX_STATUS_SUCCESS
            || spfOut == 0) {
            // Most effects are 1:1; scale by the rate ratio as a fallback.
            spfOut = (inSr > 0) ? (unsigned)((uint64_t)spfIn * outSr / inSr) : spfIn;
        }
        if (spfIn == 0) spfIn = kPassthroughFrame;
        if (spfOut == 0) spfOut = spfIn;

        handle_             = h;
        curMode_            = mode;
        inputSampleRate_    = inSr;
        outputSampleRate_   = outSr;
        inSamplesPerFrame_  = spfIn;
        outSamplesPerFrame_ = spfOut;

        std::cout << "AudioFX: ready — in " << inSr << "Hz/" << spfIn
                  << " smp, out " << outSr << "Hz/" << spfOut << " smp" << std::endl;
        return true;
    }

    // Process exactly inSamplesPerFrame() input samples into outSamplesPerFrame()
    // output samples. Passthrough / any failure copies input → output (mirrors
    // every frame.copyTo(result) fallback on the video side).
    void process(const float *in, float *out) {
        if (curMode_ == AMODE_NONE || !handle_) {
            std::memcpy(out, in, inSamplesPerFrame_ * sizeof(float));
            return;
        }
        const float *inPtr[1]  = {in};
        float       *outPtr[1] = {out};
        if (NvAFX_Run(handle_, inPtr, outPtr, inSamplesPerFrame_, 1) != NVAFX_STATUS_SUCCESS) {
            std::memcpy(out, in,
                        (inSamplesPerFrame_ < outSamplesPerFrame_ ? inSamplesPerFrame_
                                                                  : outSamplesPerFrame_)
                            * sizeof(float));
        }
    }

    unsigned inputSampleRate()    const { return inputSampleRate_; }
    unsigned outputSampleRate()   const { return outputSampleRate_; }
    unsigned inSamplesPerFrame()  const { return inSamplesPerFrame_; }
    unsigned outSamplesPerFrame() const { return outSamplesPerFrame_; }
    int      mode()               const { return curMode_; }

private:
    static constexpr unsigned kPassthroughFrame = 480;  // 10 ms @ 48 kHz

    static float clampUnit(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

    static const char *selectorFor(int mode) {
        switch (mode) {
        case AMODE_DENOISE:  return NVAFX_EFFECT_DENOISER;
        case AMODE_DEREVERB: return NVAFX_EFFECT_DEREVERB;
        case AMODE_STUDIO:   return NVAFX_EFFECT_STUDIO_VOICE_HIGH_QUALITY;
        default:             return nullptr;
        }
    }

    // Denoiser/dereverb run natively at 48 kHz → no resampling for the common
    // mic path. Studio Voice also runs at 48 kHz here. (If you swap in SuperRes,
    // return 16000 and the I/O layer's rate negotiation handles the rest.)
    static unsigned preferredInputRate(int /*mode*/) { return 48000; }

    std::string modelPathFor(int mode, unsigned rate) const {
        std::string name;
        switch (mode) {
        case AMODE_DENOISE:  name = "denoiser_";    break;
        case AMODE_DEREVERB: name = "dereverb_";    break;
        case AMODE_STUDIO:   name = "studiovoice_"; break;
        default:             name = "denoiser_";    break;
        }
        name += (rate >= 48000 ? "48k" : "16k");
        name += ".trtpkg";
        return modelDir_ + "/" + name;
    }

    void destroy() {
        if (handle_) {
            NvAFX_DestroyEffect(handle_);
            handle_ = nullptr;
        }
    }

    NvAFX_Handle handle_   = nullptr;
    int          curMode_  = AMODE_NONE;
    float        intensity_ = 1.0f;
    std::string  modelDir_;

    unsigned inputSampleRate_    = 48000;
    unsigned outputSampleRate_   = 48000;
    unsigned inSamplesPerFrame_  = kPassthroughFrame;
    unsigned outSamplesPerFrame_ = kPassthroughFrame;
};
