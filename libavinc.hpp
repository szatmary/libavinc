#pragma once

extern "C" {
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace libav {
///////////////////////////////////////////////////////////////////////////////
static const int64_t FLICKS_TIMESCALE = 705600000;
static const ::AVRational FLICKS_TIMESCALE_Q = { 1, FLICKS_TIMESCALE };
using flicks = std::chrono::duration<int64_t, std::ratio<1, FLICKS_TIMESCALE>>;
inline flicks av_rescale(int64_t time, ::AVRational scale)
{
    return flicks(::av_rescale_q(time, scale, FLICKS_TIMESCALE_Q));
}

inline int64_t av_rescale(flicks time, ::AVRational scale)
{
    return ::av_rescale_q(time.count(), FLICKS_TIMESCALE_Q, scale);
}
///////////////////////////////////////////////////////////////////////////////
using AVDictionary = std::multimap<std::string, std::string>;
::AVDictionary* av_dictionary(const AVDictionary& dict)
{
    ::AVDictionary* d = nullptr;
    for (const auto& entry : dict) {
        av_dict_set(&d, entry.first.c_str(), entry.second.c_str(), 0);
    }
    return d;
}

AVDictionary av_dictionary(const ::AVDictionary* dict)
{
    AVDictionary d;
    AVDictionaryEntry* entry = nullptr;
    while ((entry = av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX))) {
        d.emplace(entry->key, entry->value);
    }
    return d;
}

void av_dict_free(::AVDictionary* d)
{
    if (d) {
        av_dict_free(&d);
    }
}
///////////////////////////////////////////////////////////////////////////////
using AVIOContext = std::unique_ptr<::AVIOContext, void (*)(::AVIOContext*)>;
inline AVIOContext avio_alloc_context(std::function<int(uint8_t*, int)> read,
    std::function<int(uint8_t*, int)> write,
    std::function<int64_t(int64_t, int)> seek,
    int buffer_size = 4096, int write_flag = 1)
{
    struct opaque_t {
        std::function<int(uint8_t*, int)> read;
        std::function<int(uint8_t*, int)> write;
        std::function<int64_t(int64_t, int)> seek;
    };

    // create a copy of the functions to enable captures
    auto buffer = (unsigned char*)::av_malloc(buffer_size);
    auto opaque = new opaque_t { read, write, seek };
    return AVIOContext(
        ::avio_alloc_context(
            buffer, buffer_size, write_flag, opaque,
            [](void* opaque, uint8_t* buf, int buf_size) {
                auto o = reinterpret_cast<opaque_t*>(opaque);
                return o->read(buf, buf_size);
            },
            [](void* opaque, uint8_t* buf, int buf_size) {
                auto o = reinterpret_cast<opaque_t*>(opaque);
                return o->write(buf, buf_size);
            },
            [](void* opaque, int64_t offset, int whence) {
                auto o = reinterpret_cast<opaque_t*>(opaque);
                return o->seek(offset, whence);
            }),
        [](::AVIOContext* c) {
            delete reinterpret_cast<opaque_t*>(c->opaque);
            av_free(c->buffer), c->buffer = nullptr;
            ::avio_context_free(&c);
        });
}

inline AVIOContext avio_alloc_context(std::function<int(uint8_t*, int)> read,
    std::function<int64_t(int64_t, int)> seek, int buffer_size = 4096)
{
    return libav::avio_alloc_context(
        read, [](uint8_t*, int) { return 0; }, seek, 0, buffer_size);
}
///////////////////////////////////////////////////////////////////////////////
int av_read_frame(::AVFormatContext* ctx, ::AVPacket* pkt)
{
    if (!ctx || !pkt) {
        return AVERROR(1);
    }

    auto err = ::av_read_frame(ctx, pkt);
    if (0 <= err) {
        auto& track = ctx->streams[pkt->stream_index];
        ::av_packet_rescale_ts(pkt, track->time_base, FLICKS_TIMESCALE_Q);
        // TODO check for pkt->size == 0 but not EOF
    } else {
        ::av_init_packet(pkt);
        pkt->size = 0;
        pkt->data = nullptr;
    }

    return err;
}

// AVPacket is extended to enable ranged for loop
using AVPacketBase = std::unique_ptr<::AVPacket, void (*)(::AVPacket*)>;
class AVPacket : public AVPacketBase {
private:
    ::AVFormatContext* fmtCtx = nullptr;

public:
    AVPacket()
        : AVPacketBase(nullptr, [](::AVPacket*) {})
        , fmtCtx(nullptr)
    {
    }

    AVPacket(::AVFormatContext* fmtCtx)
        : AVPacketBase(::av_packet_alloc(), [](::AVPacket* p) { ::av_packet_free(&p); })
        , fmtCtx(fmtCtx)
    {
        libav::av_read_frame(fmtCtx, get());
    }

    AVPacket& operator++()
    {
        if (!(*this)) {
            return *this;
        }

        if (0 >= get()->size) {
            reset();
        } else {
            libav::av_read_frame(fmtCtx, get());
        }

        return *this;
    }
};

inline AVPacket av_packet_alloc()
{
    return AVPacket(nullptr);
}

inline AVPacket av_packet_clone(const ::AVPacket* src)
{
    auto newPkt = AVPacket(nullptr);
    if (::av_packet_ref(newPkt.get(), src) < 0) {
        newPkt.reset();
    }
    return newPkt;
}

inline AVPacket av_packet_clone(const AVPacket& src)
{
    return libav::av_packet_clone(src.get());
}
///////////////////////////////////////////////////////////////////////////////
using AVFormatContextBase = std::unique_ptr<::AVFormatContext, void (*)(::AVFormatContext*)>;
using AVCodecContext = std::unique_ptr<::AVCodecContext, void (*)(::AVCodecContext*)>;
class AVFormatContext : public AVFormatContextBase {
public:
    AVFormatContext()
        : AVFormatContextBase(nullptr, [](::AVFormatContext*) {})
    {
    }

    template <class T>
    AVFormatContext(::AVFormatContext* fmtCtx, T deleter)
        : AVFormatContextBase(fmtCtx, deleter)
    {
    }

    AVPacket end() { return AVPacket(); }
    AVPacket begin() { return AVPacket(get()); }
    std::map<int, AVCodecContext> open_streams;
};

inline AVFormatContext avformat_open_input(const std::string& url, const AVDictionary& options = AVDictionary())
{
    ::AVFormatContext* fmtCtx = nullptr;
    auto avdict = av_dictionary(options);
    auto err = ::avformat_open_input(&fmtCtx, url.c_str(), nullptr, &avdict);
    av_dict_free(avdict);
    if (err < 0 || !fmtCtx) {
        return AVFormatContext();
    }

    err = ::avformat_find_stream_info(fmtCtx, nullptr);
    if (err < 0) {
        ::avformat_close_input(&fmtCtx);
        return AVFormatContext();
    }

    return AVFormatContext(fmtCtx, [](::AVFormatContext* ctx) {
        auto p_ctx = &ctx;
        ::avformat_close_input(p_ctx);
    });
}

inline int av_seek_frame(AVFormatContext& ctx, flicks time, int idx = -1, int flags = 0)
{
    auto time_base = 0 <= idx ? ctx->streams[idx]->time_base : AV_TIME_BASE_Q;
    return ::av_seek_frame(ctx.get(), idx, av_rescale(time, time_base), flags);
}

inline AVFormatContext avformat_open_output(const std::string& url, std::string format_name = std::string())
{
    ::AVFormatContext* fmtCtx = nullptr;
    auto err = ::avformat_alloc_output_context2(&fmtCtx, nullptr, format_name.empty() ? nullptr : format_name.c_str(), url.c_str());
    if (0 != err || !fmtCtx) {
        return AVFormatContext();
    }

    if (0 > ::avio_open(&fmtCtx->pb, url.c_str(), AVIO_FLAG_WRITE)) {
        ::avformat_free_context(fmtCtx);
        return AVFormatContext();
    }

    return AVFormatContext(fmtCtx, [](::AVFormatContext* fmtCtx) {
        ::avio_close(fmtCtx->pb);
        ::avformat_free_context(fmtCtx);
    });
}
///////////////////////////////////////////////////////////////////////////////
inline int avformat_new_stream(AVFormatContext& fmtCtx, const ::AVCodecParameters* par)
{
    auto out_stream = ::avformat_new_stream(fmtCtx.get(), nullptr);
    ::avcodec_parameters_copy(out_stream->codecpar, par);
    return out_stream->index;
}

inline int av_interleaved_write_frame(AVFormatContext& fmtCtx, int stream_index, const ::AVPacket& pkt)
{
    int err = 0;
    // this is a flush packet, ignore and return.
    if (!pkt.data || !pkt.size) {
        return AVERROR_EOF;
    }
    ::AVPacket dup;
    ::av_packet_ref(&dup, &pkt);
    dup.stream_index = stream_index;
    auto& track = fmtCtx->streams[stream_index];
    ::av_packet_rescale_ts(&dup, FLICKS_TIMESCALE_Q, track->time_base);
    err = ::av_interleaved_write_frame(fmtCtx.get(), &dup);
    ::av_packet_unref(&dup);
    return err;
}
///////////////////////////////////////////////////////////////////////////////
using AVFrame = std::unique_ptr<::AVFrame, void (*)(::AVFrame*)>;
inline AVFrame av_frame_alloc()
{
    return AVFrame(::av_frame_alloc(), [](::AVFrame* f) {
        av_frame_free(&f);
    });
}

inline AVFrame av_frame_clone(const ::AVFrame* frame)
{
    auto newFrame = av_frame_alloc();
    if (::av_frame_ref(newFrame.get(), frame) < 0) {
        newFrame.reset();
    }
    return newFrame;
}

inline AVFrame av_frame_clone(const AVFrame& frame)
{
    return libav::av_frame_clone(frame.get());
}
///////////////////////////////////////////////////////////////////////////////
inline int av_open_best_stream(AVFormatContext& fmtCtx, AVMediaType type, int related_stream = -1)
{
    int idx = -1;
    ::AVCodec* codec = nullptr;
    if ((idx = ::av_find_best_stream(fmtCtx.get(), type, -1, related_stream, &codec, 0)) < 0) {
        return -1;
    }
    auto codecCtx = AVCodecContext(::avcodec_alloc_context3(codec),
        [](::AVCodecContext* c) {
            ::avcodec_free_context(&c);
        });

    if (::avcodec_parameters_to_context(codecCtx.get(), fmtCtx->streams[idx]->codecpar) < 0) {
        return -1;
    }
    if (::avcodec_open2(codecCtx.get(), codec, nullptr) < 0) {
        return -1;
    }

    fmtCtx.open_streams.emplace(idx, std::move(codecCtx));
    return idx;
}

inline int av_open_best_streams(AVFormatContext& fmtCtx)
{
    auto v = av_open_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO);
    auto a = av_open_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, v);
    auto s = av_open_best_stream(fmtCtx, AVMEDIA_TYPE_SUBTITLE, 0 <= v ? v : a);
    (void)v, (void)a, (void)s;
    return fmtCtx.open_streams.size();
}

inline AVCodecContext& find_open_audio_stream(AVFormatContext& fmtCtx)
{
    for (auto& stream : fmtCtx.open_streams) {
        if (stream.second->codec_type == AVMEDIA_TYPE_AUDIO) {
            return stream.second;
        }
    }

    static auto err = AVCodecContext(nullptr, [](::AVCodecContext*) {});
    return err;
}

inline AVCodecContext& find_open_video_stream(AVFormatContext& fmtCtx)
{
    for (auto& stream : fmtCtx.open_streams) {
        if (stream.second->codec_type == AVMEDIA_TYPE_VIDEO) {
            return stream.second;
        }
    }

    static auto err = AVCodecContext(nullptr, [](::AVCodecContext*) {});
    return err;
}

inline int avcodec_send_packet(AVFormatContext& fmtCtx, const ::AVPacket* pkt,
    std::function<void(AVFrame&)> onFrame)
{
    int err = AVERROR(1);
    auto codecCtx = fmtCtx.open_streams.find(pkt->stream_index);
    if (codecCtx != fmtCtx.open_streams.end()) {
        ::avcodec_send_packet(codecCtx->second.get(), pkt);
        for (;;) {
            auto frame = av_frame_alloc();
            err = ::avcodec_receive_frame(codecCtx->second.get(), frame.get());
            if (err < 0) {
                break;
            }

            onFrame(frame);
        };
    }
    return err == AVERROR(EAGAIN) ? 0 : err;
}

inline int avcodec_send_packet(AVFormatContext& fmtCtx, AVPacket& pkt,
    std::function<void(AVFrame&)> onFrame)
{
    return libav::avcodec_send_packet(fmtCtx, pkt.get(), onFrame);
}
///////////////////////////////////////////////////////////////////////////////
using AVFilterGraphBase = std::unique_ptr<::AVFilterGraph, void (*)(::AVFilterGraph*)>;
class AVFilterGraph : public AVFilterGraphBase {
public:
    class AVFilterArgs : public std::string {
    public:
        AVFilterArgs(const char* fmt, ...)
            : std::string(1024, 0)
        {
            va_list vargs;
            va_start(vargs, fmt);
            resize(vsnprintf((char*)data(), size(), fmt, vargs));
            va_end(vargs);
        }
    };

    class AVFilterChain {
    public:
        std::string name;
        std::string args;
        AVFilterChain() = default;
        AVFilterChain(std::string name, AVFilterArgs args)
            : name(name)
            , args(args)
        {
        }

        ::AVFilterContext* create_filter(::AVFilterContext* source, ::AVFilterGraph* graph) const
        {
            ::AVFilterContext* sink = nullptr;
            auto err = ::avfilter_graph_create_filter(&sink, avfilter_get_by_name(name.c_str()), nullptr, args.c_str(), nullptr, graph);
            err = ::avfilter_link(source, 0, sink, 0);
            return sink;
        }
    };

    ::AVFilterContext* sink = nullptr;
    ::AVFilterContext* source = nullptr;
    AVFilterGraph()
        : AVFilterGraphBase(nullptr, [](::AVFilterGraph*) {})
    {
    }

    AVFilterGraph(AVFormatContext& fmtCtx, int stream, const std::vector<AVFilterChain>& chain)
        : AVFilterGraphBase(::avfilter_graph_alloc(), [](::AVFilterGraph* p) { avfilter_graph_free(&p); })
    {
        auto& codecpar = fmtCtx->streams[stream]->codecpar;
        AVFilterChain buffer("buffer",
            { "video_size=%dx%d:pix_fmt=%d:time_base=%ld/%ld:pixel_aspect=%d/%d",
                codecpar->width, codecpar->height, codecpar->format,
                flicks::period::num, flicks::period::den,
                codecpar->sample_aspect_ratio.num, codecpar->sample_aspect_ratio.den });

        ::avfilter_graph_create_filter(&source, avfilter_get_by_name(buffer.name.c_str()), nullptr, buffer.args.c_str(), nullptr, get());
        auto _source = source;
        for (const auto& link : chain) {
            _source = link.create_filter(_source, get());
        }

        // Create sink
        enum AVPixelFormat pix_fmts[] = { (AVPixelFormat)codecpar->format, AV_PIX_FMT_NONE };
        AVBufferSinkParams* buffersink_params = av_buffersink_params_alloc();
        buffersink_params->pixel_fmts = pix_fmts;
        auto err = avfilter_graph_create_filter(&sink, avfilter_get_by_name("buffersink"), nullptr, nullptr, buffersink_params, get());
        err = ::avfilter_link(_source, 0, sink, 0);
        ::av_free(buffersink_params);
        err = ::avfilter_graph_config(get(), nullptr);
    }
};

inline int avfilter_graph_write_frame(AVFilterGraph& graph, AVFrame& frame, std::function<void(AVFrame&)> onFrame)
{
    int err = 0;
    if ((err = ::av_buffersrc_write_frame(graph.source, frame.get())) < 0) {
        return err;
    }

    for (;;) {
        auto newFrame = av_frame_alloc();
        if (::av_buffersink_get_frame(graph.sink, newFrame.get()) < 0) {
            break;
        }

        onFrame(newFrame);
    }

    return err == AVERROR(EAGAIN) ? 0 : err;
}
}
