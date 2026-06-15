// ══════════════════════════════════════════════════════════════════════════
// Audio I/O (PulseAudio client API)
//
// Mirrors VirtualCamera (server.cpp). Two thin wrappers over pa_simple:
//   AudioCapture  — records mono float32 from a real mic source.
//   VirtualMic    — plays mono float32 into the BluCast null sink, which a
//                   module-remap-source then re-exposes as "BluCast Virtual
//                   Microphone" for other apps.
//
// Why libpulse and not native PipeWire: PipeWire ships a full PulseAudio
// protocol implementation (pipewire-pulse), so this one code path works on both
// PipeWire and PulseAudio — i.e. essentially every modern Linux desktop.
//
// Sample-rate handling: we open the streams at the model's exact in/out rates
// and let the sound server resample the physical mic / the sink transparently —
// no hand-rolled resampler needed.
// ══════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <pulse/error.h>
#include <pulse/simple.h>

// ── Capture from a real microphone ────────────────────────────────────────
class AudioCapture {
public:
    AudioCapture() = default;
    ~AudioCapture() { close(); }

    // Reopen if the source/rate changed (same guard pattern as VirtualCamera::open).
    bool open(const std::string &source, uint32_t rate, uint8_t channels = 1) {
        if (s_ && (source_ != source || rate_ != rate || channels_ != channels)) close();
        if (s_) return true;

        pa_sample_spec ss;
        ss.format   = PA_SAMPLE_FLOAT32NE;
        ss.rate     = rate;
        ss.channels = channels;

        // Bound capture latency to a couple of frames' worth of audio.
        pa_buffer_attr ba;
        std::memset(&ba, 0xff, sizeof(ba));  // -1 = let the server pick defaults
        ba.fragsize = pa_usec_to_bytes(20 * 1000, &ss);  // ~20 ms fragments

        int err = 0;
        s_ = pa_simple_new(nullptr, "BluCast", PA_STREAM_RECORD,
                           source.empty() ? nullptr : source.c_str(),
                           "microphone", &ss, nullptr, &ba, &err);
        if (!s_) {
            std::cerr << "AudioCapture: cannot open source '"
                      << (source.empty() ? "<default>" : source)
                      << "': " << pa_strerror(err) << std::endl;
            return false;
        }
        source_   = source;
        rate_     = rate;
        channels_ = channels;
        std::cout << "AudioCapture: " << (source.empty() ? "<default>" : source)
                  << " @ " << rate << "Hz x" << (int)channels << std::endl;
        return true;
    }

    // Read `frames` mono samples (blocking). Returns false on error.
    bool read(float *buf, size_t frames) {
        if (!s_) return false;
        int err = 0;
        if (pa_simple_read(s_, buf, frames * channels_ * sizeof(float), &err) < 0) {
            std::cerr << "AudioCapture: read error: " << pa_strerror(err) << std::endl;
            return false;
        }
        return true;
    }

    bool isOpen() const { return s_ != nullptr; }

    void close() {
        if (s_) { pa_simple_free(s_); s_ = nullptr; }
    }

private:
    pa_simple  *s_       = nullptr;
    std::string source_;
    uint32_t    rate_     = 0;
    uint8_t     channels_ = 1;
};

// ── Playback into the BluCast null sink (the virtual mic) ──────────────────
class VirtualMic {
public:
    VirtualMic() = default;
    ~VirtualMic() { close(); }

    bool open(const std::string &sink, uint32_t rate, uint8_t channels = 1) {
        if (s_ && (sink_ != sink || rate_ != rate || channels_ != channels)) close();
        if (s_) return true;

        pa_sample_spec ss;
        ss.format   = PA_SAMPLE_FLOAT32NE;
        ss.rate     = rate;
        ss.channels = channels;

        int err = 0;
        s_ = pa_simple_new(nullptr, "BluCast", PA_STREAM_PLAYBACK,
                           sink.empty() ? nullptr : sink.c_str(),
                           "virtual-microphone", &ss, nullptr, nullptr, &err);
        if (!s_) {
            std::cerr << "VirtualMic: cannot open sink '"
                      << (sink.empty() ? "<default>" : sink)
                      << "': " << pa_strerror(err) << std::endl;
            return false;
        }
        sink_     = sink;
        rate_     = rate;
        channels_ = channels;
        std::cout << "VirtualMic: -> " << (sink.empty() ? "<default>" : sink)
                  << " @ " << rate << "Hz x" << (int)channels << std::endl;
        return true;
    }

    bool writeFrame(const float *buf, size_t frames) {
        if (!s_) return false;
        int err = 0;
        if (pa_simple_write(s_, buf, frames * channels_ * sizeof(float), &err) < 0) {
            std::cerr << "VirtualMic: write error: " << pa_strerror(err) << std::endl;
            return false;
        }
        return true;
    }

    // Analog of VirtualCamera::writeIdleFrame — keep the sink fed with silence
    // so downstream consumers see a live-but-quiet mic instead of an xrun.
    void writeSilence(size_t frames) {
        if (!s_) return;
        if (silence_.size() < frames * channels_) silence_.assign(frames * channels_, 0.0f);
        writeFrame(silence_.data(), frames);
    }

    bool isOpen() const { return s_ != nullptr; }

    void close() {
        if (s_) { pa_simple_free(s_); s_ = nullptr; }
    }

private:
    pa_simple         *s_       = nullptr;
    std::string        sink_;
    uint32_t           rate_     = 0;
    uint8_t            channels_ = 1;
    std::vector<float> silence_;
};
