// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <codec/state.h>

#define DEBUG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

#ifndef VITA3K_PLATFORM_IOS
// Stock FFmpeg installs (vcpkg iOS) do not ship this private header; the iOS
// build uses the public send/receive API below instead.
#include <libavcodec/codec_internal.h>
#endif
}

#include <util/log.h>

#include <cstring>
#include <iterator>

#ifdef VITA3K_PLATFORM_IOS
namespace {

// FFmpeg's public send/receive API accepts a complete AVPacket but does not
// report how many bytes the decoder consumed. SceAudiodec requires that exact
// count because maxEsSize is only the capacity of the guest buffer. Vita3K's
// desktop path gets the count from FFCodec::cb.decode; vcpkg deliberately does
// not install codec_internal.h, so mirror the prefix of FFmpeg 8.1's FFCodec
// (the version pinned by vcpkg.json) through that callback.
struct FFCodec81Prefix {
    AVCodec public_codec;
    unsigned caps_internal : 24;
    unsigned is_decoder : 1;
    unsigned color_ranges : 2;
    unsigned alpha_modes : 2;
    unsigned cb_type : 3;
    int priv_data_size;
    int (*update_thread_context)(AVCodecContext *, const AVCodecContext *);
    int (*update_thread_context_for_user)(AVCodecContext *, const AVCodecContext *);
    const void *defaults;
    int (*init)(AVCodecContext *);
    union {
        int (*decode)(AVCodecContext *, AVFrame *, int *, AVPacket *);
        void *other;
    } cb;
};

constexpr unsigned FF_CODEC_CB_TYPE_DECODE = 0;

const FFCodec81Prefix *ffmpeg81_private_decoder(const AVCodec *public_codec) {
    if (!public_codec || std::strncmp(av_version_info(), "8.1", 3) != 0)
        return nullptr;
    const auto *private_codec = reinterpret_cast<const FFCodec81Prefix *>(public_codec);
    if (!private_codec->is_decoder || private_codec->cb_type != FF_CODEC_CB_TYPE_DECODE
        || !private_codec->cb.decode)
        return nullptr;
    return private_codec;
}

} // namespace
#endif

#include <algorithm>

AacDecoderState::AacDecoderState(uint32_t sample_rate, uint32_t channels, bool sbr) {
    // SBR doubles the core rate the guest gives
    output_sample_rate = sbr ? sample_rate * 2 : sample_rate;
    output_channels = channels;
    is_sbr = sbr;

    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    assert(codec);

    context = avcodec_alloc_context3(codec);
    assert(context);

    frame = av_frame_alloc();

    context->codec_type = AVMEDIA_TYPE_AUDIO;
    av_channel_layout_default(&context->ch_layout, channels);
    context->sample_rate = sample_rate;

#ifdef VITA3K_PLATFORM_IOS
    // SceAudiodec provides the stream configuration out of band (sample rate +
    // channels), so synthesise the 2-byte AudioSpecificConfig expected by
    // FFmpeg's raw-AAC decoder, matching what an MP4 'esds' box would carry.
    static const int aac_freq_table[] = { 96000, 88200, 64000, 48000, 44100,
        32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };
    int freq_index = -1;
    for (int i = 0; i < static_cast<int>(std::size(aac_freq_table)); ++i) {
        if (aac_freq_table[i] == static_cast<int>(sample_rate)) {
            freq_index = i;
            break;
        }
    }
    if (freq_index >= 0 && channels >= 1 && channels <= 7) {
        constexpr int aac_lc_object_type = 2;
        uint8_t asc[2];
        asc[0] = static_cast<uint8_t>((aac_lc_object_type << 3) | (freq_index >> 1));
        asc[1] = static_cast<uint8_t>(((freq_index & 1) << 7) | (channels << 3));
        context->extradata = static_cast<uint8_t *>(
            av_mallocz(sizeof(asc) + AV_INPUT_BUFFER_PADDING_SIZE));
        if (context->extradata) {
            memcpy(context->extradata, asc, sizeof(asc));
            context->extradata_size = sizeof(asc);
        }
    } else {
        LOG_WARN("AAC: no AudioSpecificConfig for sample_rate={} channels={}; "
                 "raw access units may fail to decode.",
            sample_rate, channels);
    }
#endif

    int err = avcodec_open2(context, codec, nullptr);
    assert(err == 0);
}

AacDecoderState::~AacDecoderState() {
    av_frame_free(&frame);
    swr_free(&swr);
}

uint32_t AacDecoderState::get(DecoderQuery query) {
    switch (query) {
    case DecoderQuery::CHANNELS: return output_channels;
    case DecoderQuery::BIT_RATE: return context->bit_rate;
    case DecoderQuery::SAMPLE_RATE: return output_sample_rate;
    default:
        return 0;
    }
}

bool AacDecoderState::send(const uint8_t *data, uint32_t size) {
    AVPacket *packet = av_packet_alloc();
    packet->data = const_cast<uint8_t *>(data);
    packet->size = static_cast<int>(size);

    av_frame_unref(frame);

#ifdef VITA3K_PLATFORM_IOS
    const FFCodec81Prefix *private_codec = ffmpeg81_private_decoder(codec);
    int got_frame = 0;
    const int consumed = private_codec
        ? private_codec->cb.decode(context, frame, &got_frame, packet)
        : AVERROR(ENOSYS);
    av_packet_free(&packet);

    if (consumed >= 0 && got_frame && consumed <= static_cast<int>(size)) {
        es_size_used = static_cast<uint32_t>(consumed);
        LOG_INFO_ONCE("AAC iOS exact decoder bridge: ffmpeg='{}' input={} consumed={} samples={}.",
            av_version_info(), size, es_size_used, frame->nb_samples);
        return true;
    }

    // Keep a bad AAC access unit from permanently wedging the movie clock.
    // This is a last-resort path; the log makes a future FFmpeg private-ABI
    // mismatch or genuinely invalid packet explicit.
    const auto emit_silence = [this]() {
        av_frame_unref(frame);
        frame->format = AV_SAMPLE_FMT_FLTP;
        av_channel_layout_copy(&frame->ch_layout, &context->ch_layout);
        frame->sample_rate = context->sample_rate;
        // AAC-LC produces 1024 samples per frame; SBR/HE-AAC would be 2048, but
        // a single short silent frame is a benign clock nudge either way.
        frame->nb_samples = 1024;
        if (av_frame_get_buffer(frame, 0) < 0)
            return false;
        av_samples_set_silence(frame->extended_data, 0, frame->nb_samples,
            frame->ch_layout.nb_channels, AV_SAMPLE_FMT_FLTP);
        return true;
    };
    LOG_WARN_ONCE("AAC iOS exact decoder bridge failed: ffmpeg='{}' bridge={} result={} got_frame={} "
                  "input={} extradata={} head={:02X} {:02X} {:02X} {:02X}; emitting silence.",
        av_version_info(), private_codec != nullptr, codec_error_name(consumed), got_frame,
        size, context->extradata_size, size > 0 ? data[0] : 0, size > 1 ? data[1] : 0,
        size > 2 ? data[2] : 0, size > 3 ? data[3] : 0);
    if (!emit_silence())
        return false;
    es_size_used = size;
    return true;
#else
    const FFCodec *ff_codec = ffcodec(codec);
    int got_frame;
    int len = ff_codec->cb.decode(context, frame, &got_frame, packet);
    assert(got_frame);

    av_packet_free(&packet);
    if (len < 0) {
        LOG_WARN("Error sending Aac packet: {}.", codec_error_name(len));
        return false;
    }

    es_size_used = static_cast<uint32_t>(len);

    return true;
#endif
}

bool AacDecoderState::receive(uint8_t *data, DecoderSize *size) {
    if (frame->nb_samples <= 0) {
        if (size)
            size->samples = 0;
        return true;
    }

    // FFmpeg decodes implicit SBR in mono as stereo the guest gets the layout it asked for
    const int input_sample_rate = frame->sample_rate > 0 ? frame->sample_rate : context->sample_rate;
    const int input_channels = frame->ch_layout.nb_channels > 0 ? frame->ch_layout.nb_channels : context->ch_layout.nb_channels;
    if (input_sample_rate <= 0 || input_channels <= 0) {
        LOG_WARN("AAC frame has an invalid format: {} Hz, {} channels.", input_sample_rate, input_channels);
        return false;
    }

    if (!swr || configured_input_sample_rate != input_sample_rate || configured_input_channels != input_channels
        || configured_input_format != frame->format) {
        AVChannelLayout input_layout{};
        if (frame->ch_layout.nb_channels > 0)
            av_channel_layout_copy(&input_layout, &frame->ch_layout);
        else
            av_channel_layout_default(&input_layout, input_channels);
        AVChannelLayout output_layout{};
        av_channel_layout_default(&output_layout, static_cast<int>(output_channels));

        SwrContext *new_swr = nullptr;
        const int ret = swr_alloc_set_opts2(&new_swr,
            &output_layout, AV_SAMPLE_FMT_S16, static_cast<int>(output_sample_rate),
            &input_layout, static_cast<AVSampleFormat>(frame->format), input_sample_rate,
            0, nullptr);
        av_channel_layout_uninit(&output_layout);
        av_channel_layout_uninit(&input_layout);
        if (ret < 0 || !new_swr || swr_init(new_swr) < 0) {
            swr_free(&new_swr);
            LOG_WARN("Failed to configure the AAC converter: {} Hz {} ch -> {} Hz {} ch.", input_sample_rate, input_channels, output_sample_rate, output_channels);
            return false;
        }

        swr_free(&swr);
        swr = new_swr;
        configured_input_sample_rate = input_sample_rate;
        configured_input_channels = input_channels;
        configured_input_format = frame->format;
    }

    const int max_samples = is_sbr ? 2048 : 1024;
    int converted_samples;
    if (data) {
        const int capacity = std::min(swr_get_out_samples(swr, frame->nb_samples), max_samples);
        converted_samples = swr_convert(swr, &data, capacity, const_cast<const uint8_t **>(frame->extended_data), frame->nb_samples);
        if (converted_samples < 0) {
            LOG_WARN("AAC conversion failed: {}.", codec_error_name(converted_samples));
            return false;
        }
    } else {
        converted_samples = std::min(swr_get_out_samples(swr, frame->nb_samples), max_samples);
    }

    if (size) {
        size->samples = static_cast<uint32_t>(converted_samples);
    }

    return true;
}

uint32_t AacDecoderState::get_es_size() {
    return es_size_used;
}
