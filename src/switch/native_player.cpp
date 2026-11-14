#include "newpipe/ui_text.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
#include "newpipe/native_player.hpp"
#include "newpipe/resume_store.hpp"
#include "newpipe/audio_sink.hpp"
#include "newpipe/http_client.hpp"
#include "newpipe/log.hpp"
#include "newpipe/native_text.hpp"
#include <SDL2/SDL.h>
#ifdef __SWITCH__
#include <switch.h>
#include <switch2/video/decoder.h>
#endif
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace newpipe {
namespace {
using Wall = std::chrono::steady_clock;
using Packet = std::shared_ptr<AVPacket>;
using Frame = std::shared_ptr<AVFrame>;
static Packet packet_new() {
  return Packet(av_packet_alloc(), [](AVPacket *p) { av_packet_free(&p); });
}
static Frame frame_new() {
  return Frame(av_frame_alloc(), [](AVFrame *p) { av_frame_free(&p); });
}
static std::string av_error(int rc) {
  char s[AV_ERROR_MAX_STRING_SIZE];
  av_strerror(rc, s, sizeof(s));
  return s;
}
static std::string time_text(double seconds) {
  int s = std::max(0, static_cast<int>(seconds));
  char text[32];
  if (s >= 3600)
    std::snprintf(text, sizeof(text), "%d:%02d:%02d", s / 3600, s / 60 % 60,
                  s % 60);
  else
    std::snprintf(text, sizeof(text), "%d:%02d", s / 60, s % 60);
  return text;
}
template <class T> class Queue {
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<T> data_;
  size_t limit_;
  bool closed_ = false;

public:
  explicit Queue(size_t limit) : limit_(limit) {}
  bool push(T value, const std::atomic_bool &stop) {
    std::unique_lock<std::mutex> l(mutex_);
    cv_.wait(l,
             [&] { return data_.size() < limit_ || stop.load() || closed_; });
    if (stop || closed_)
      return false;
    data_.push_back(std::move(value));
    cv_.notify_all();
    return true;
  }
  bool pop(T &value, const std::atomic_bool &stop) {
    std::unique_lock<std::mutex> l(mutex_);
    cv_.wait(l, [&] { return !data_.empty() || closed_ || stop.load(); });
    if (stop || data_.empty())
      return false;
    value = std::move(data_.front());
    data_.pop_front();
    cv_.notify_all();
    return true;
  }
  bool take(T &value) {
    std::lock_guard<std::mutex> l(mutex_);
    if (data_.empty())
      return false;
    value = std::move(data_.front());
    data_.pop_front();
    cv_.notify_all();
    return true;
  }
  bool front(T &value) {
    std::lock_guard<std::mutex> l(mutex_);
    if (data_.empty())
      return false;
    value = data_.front();
    return true;
  }
  size_t size() {
    std::lock_guard<std::mutex> l(mutex_);
    return data_.size();
  }
  void reset() {
    std::lock_guard<std::mutex> l(mutex_); data_.clear();closed_=false;cv_.notify_all();
  }
  void close() {
    std::lock_guard<std::mutex> l(mutex_);
    closed_ = true;
    cv_.notify_all();
  }
};
struct Video {
  Frame frame;
  double pts = 0;
};
struct PCM {
  std::vector<int16_t> samples;
  double pts = 0;
};
struct Input {
  NativeMediaInput source;
  AVFormatContext *format = nullptr;
  AVIOContext *io = nullptr;
  std::atomic_bool *stop = nullptr;
  int video = -1, audio = -1;
  ~Input() {
    avformat_close_input(&format);
    if (io) {
      av_freep(&io->buffer);
      avio_context_free(&io);
    }
  }
  static int read(void *p, uint8_t *data, int size) {
    auto &i = *static_cast<Input *>(p);
    if (*i.stop)
      return AVERROR_EXIT;
    try {
      int64_t n =
          i.source.read(i.source.cookie, reinterpret_cast<char *>(data), size);
      return n > 0     ? static_cast<int>(n)
             : n == -2 ? AVERROR_EXIT
             : n < 0   ? AVERROR(EIO)
                       : AVERROR_EOF;
    } catch (...) {
      return AVERROR(EIO);
    }
  }
  static int64_t seek(void *p, int64_t off, int whence) {
    auto &i = *static_cast<Input *>(p);
    if (*i.stop)
      return AVERROR_EXIT;
    return i.source.seek(i.source.cookie, off, whence);
  }
  static int interrupt(void *p) {
    return static_cast<Input *>(p)->stop->load() ? 1 : 0;
  }
  bool open(std::string &error) {
    format = avformat_alloc_context();
    if (!format) {
      error = "No demux memory";
      return false;
    }
    format->interrupt_callback = {interrupt, this};
    format->probesize = 1024 * 1024;
    format->max_analyze_duration = 2 * AV_TIME_BASE;
    if (source.read) {
      auto *memory = static_cast<uint8_t *>(av_malloc(65536));
      if (!memory) {
        error = "No IO memory";
        return false;
      }
      io = avio_alloc_context(memory, 65536, 0, this, read, nullptr,
                              source.seek ? seek : nullptr);
      if (!io) {
        av_free(memory);
        error = "No IO context";
        return false;
      }
      // Skip eager MOV fragment-index scans over a growing cache, while
      // retaining the seek callback for positions already downloaded.
      format->flags |= AVFMT_FLAG_IGNIDX;
      format->pb = io;
      format->flags |= AVFMT_FLAG_CUSTOM_IO;
    }
    AVDictionary *options = nullptr;
    av_dict_set(&options, "tls_verify", "1", 0);
    av_dict_set(&options, "probesize", "1048576", 0);
    av_dict_set(&options, "analyzeduration", "1500000", 0);
    if (source.read)
      av_dict_set(&options, "use_mfra_for", "0", 0);
#ifdef __SWITCH__
    av_dict_set(&options, "ca_file", "romfs:/certs/cacert.pem", 0);
#endif
    av_dict_set(&options, "rw_timeout", "10000000", 0);
    if (!source.user_agent.empty())
      av_dict_set(&options, "user_agent", source.user_agent.c_str(), 0);
    if (!source.referer.empty())
      av_dict_set(&options, "referer", source.referer.c_str(), 0);
    int rc = avformat_open_input(
        &format, source.read ? nullptr : source.url.c_str(), nullptr, &options);
    av_dict_free(&options);
    if (rc >= 0)
      rc = avformat_find_stream_info(format, nullptr);
    if(rc>=0) {
      bool incomplete=format->nb_streams==0;
      for(unsigned i=0;i<format->nb_streams;++i) {
        auto* par=format->streams[i]->codecpar;
        incomplete=incomplete || par->codec_id==AV_CODEC_ID_NONE ||
          (par->codec_type==AVMEDIA_TYPE_VIDEO && (par->width<=0 || par->height<=0)) ||
          (par->codec_type==AVMEDIA_TYPE_AUDIO && par->sample_rate<=0);
      }
      if(incomplete && !source.cancelled->load()) {
        format->probesize=5*1024*1024;format->max_analyze_duration=5*AV_TIME_BASE;
        rc=avformat_find_stream_info(format,nullptr);
      }
    }
    if (rc < 0) {
      error = "Open media: " + av_error(rc);
      return false;
    }
    video = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    return true;
  }
};
struct CaptionsState {
  std::mutex mutex;
  std::vector<CaptionCue> cues;
  std::string status;
  std::atomic<int> generation{0};
};
class Player {
  SDL_Window *window_;
  NativeMediaInput source_;
  NativePlaybackState local_, &state_;
  SDL_Renderer *renderer_ = nullptr;
  SDL_Texture *texture_ = nullptr;
  std::unique_ptr<NativeText> text_;
  int texture_w_ = 0, texture_h_ = 0;
  bool texture_planar_ = false;
  SwsContext *scaler_ = nullptr;
  AVCodecContext *video_ = nullptr, *audio_ = nullptr;
  Input media_, external_;
  Input *audio_input_ = nullptr;
  Queue<Packet> video_packets_{96}, audio_packets_{192};
  Queue<Video> pictures_{8};
  Queue<std::shared_ptr<PCM>> sounds_{48};
  std::atomic_bool stop_{false}, video_done_{false}, audio_done_{false};
  std::mutex error_mutex_;
  std::string error_;
  std::thread demux_, extra_demux_, video_thread_, audio_thread_;
  AudioSink sink_;
  bool has_audio_ = false, started_ = false, paused_ = false, quit_ = false,
       restarting_ = false;
  std::shared_ptr<PCM> pending_audio_;
  size_t audio_offset_ = 0;
  Frame displayed_;
  double displayed_pts_ = 0, duration_ = 0, clock_base_ = 0, position_ = 0;
  double origin_ = 0;
  Wall::time_point wall_base_, pause_start_,
      hud_until_ = Wall::now() + std::chrono::seconds(4);
  const double rate_;
  const bool software_only_;
  float volume_ = 1;
  bool menu_ = false, menu_tabs_=true;
  std::atomic<int> decoder_mode_{0};
  std::atomic<uint64_t> decoded_frames_{0}, video_read_ms_{0}, video_read_slow_{0};
  uint64_t upload_us_=0;
  Uint32 texture_format_=0;
  int menu_row_ = 0, quality_choice_ = 0, speed_choice_ = 2,
      caption_choice_ = -1;
  bool decoder_choice_ = false;
  std::thread caption_thread_;
  std::mutex caption_job_mutex_;
  std::condition_variable caption_job_cv_;
  bool caption_stop_ = false;
  int caption_pending_ = -2, caption_pending_generation_ = 0;
  inline static constexpr int qualities_[3] = {360, 720, 1080};
  inline static constexpr double speeds_[7] = {0.5, 0.75, 1, 1.25,
                                               1.5, 1.75, 2};
  std::string notice_;
  uint64_t frames_ = 0, dropped_ = 0;
  bool first_audio_ = false, audio_tail_ = false;
  bool scrubbing_ = false, dragging_seek_ = false;
  double seek_preview_ = 0, pending_seek_=-1;
  bool seeking_=false, seek_frame_pending_=false, controls_hidden_=false;
  double tail_clock_ = 0;
  Wall::time_point tail_wall_;
  std::shared_ptr<CaptionsState> captions_ = std::make_shared<CaptionsState>();
#ifdef __SWITCH__
  PadState pad_{};
  bool a_released_ = false;
#endif
  bool fail(const std::string &message) {
    std::lock_guard<std::mutex> l(error_mutex_);
    if (error_.empty())
      error_ = message;
    stop_ = true;
    source_.cancelled->store(true);
    if (source_.audio)
      source_.audio->cancelled->store(true);
    wake();
    logf("player3: error %s", message.c_str());
    return false;
  }
  void wake() {
    video_packets_.close();
    audio_packets_.close();
    pictures_.close();
    sounds_.close();
  }
  bool codec(Input &input, int index, AVCodecContext **out) {
    const auto *parameters = input.format->streams[index]->codecpar;
    const auto *c = avcodec_find_decoder(parameters->codec_id);
    if (!c)
      return fail("Codec unavailable");
    *out = avcodec_alloc_context3(c);
    if (!*out)
      return fail("No codec memory");
    int rc = avcodec_parameters_to_context(*out, parameters);
    (*out)->thread_count = 2;
    if (rc >= 0)
      rc = avcodec_open2(*out, c, nullptr);
    logf("player3: codec=%s rc=%d", c->name, rc);
    return rc >= 0 || fail(av_error(rc));
  }
  void demux(Input *input, bool video, bool audio) {
    while (!stop_) {
      auto p = packet_new();
      if (!p) {
        fail("No packet memory");
        break;
      }
      auto read_start=Wall::now();
      int rc = av_read_frame(input->format, p.get());
      if(video){auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(Wall::now()-read_start).count();video_read_ms_+=ms;if(ms>100)++video_read_slow_;}
      if (rc == AVERROR_EOF)
        break;
      if (rc < 0) {
        if (!stop_)
          fail("Read media: " + av_error(rc));
        break;
      }
      if (video && p->stream_index == input->video) {
        if (!video_packets_.push(p, stop_))
          break;
      } else if (audio && p->stream_index == input->audio) {
        if (!audio_packets_.push(p, stop_))
          break;
      }
    }
    if (video)
      video_packets_.close();
    if (audio)
      audio_packets_.close();
  }
  double pts(AVFrame *f, Input &input, int index) {
    if (f->best_effort_timestamp == AV_NOPTS_VALUE)
      return state_.start;
    return f->best_effort_timestamp *
               av_q2d(input.format->streams[index]->time_base) -
           origin_;
  }
  bool push_picture(Frame f, double timestamp) {
    if (timestamp + 0.001 < state_.start)
      return true;
    ++decoded_frames_;
    return pictures_.push({std::move(f), timestamp}, stop_);
  }
  void video_loop() {
#ifdef __SWITCH__
    if (!software_only_ && video_->codec_id == AV_CODEC_ID_H264 &&
        video_->height > 360) {
      if (hardware_video()) {
        video_done_ = true;
        pictures_.close();
        return;
      }
      if (stop_) {
        video_done_ = true;
        pictures_.close();
        return;
      }
    }
#endif
    decoder_mode_=1;
    log_line("player3: software video decoder");
    Frame frame = frame_new();
    if (!frame) {
      fail("No video frame memory");
      video_done_ = true;
      pictures_.close();
      return;
    }
    Packet p;
    auto receive = [&]() {
      while (!stop_) {
        int rc = avcodec_receive_frame(video_, frame.get());
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
          return true;
        if (rc < 0)
          return fail("Video decode: " + av_error(rc));
        Frame copy(av_frame_clone(frame.get()),
                   [](AVFrame *f) { av_frame_free(&f); });
        double t = pts(frame.get(), media_, media_.video);
        av_frame_unref(frame.get());
        if (!copy)
          return fail("No video frame memory");
        if (!push_picture(std::move(copy), t))
          return false;
      }
      return false;
    };
    while (video_packets_.pop(p, stop_)) {
      int rc = avcodec_send_packet(video_, p.get());
      if (rc < 0) {
        fail("Video packet: " + av_error(rc));
        break;
      }
      if (!receive())
        break;
    }
    if (!stop_) {
      avcodec_send_packet(video_, nullptr);
      receive();
    }
    video_done_ = true;
    pictures_.close();
  }
#ifdef __SWITCH__
  bool hardware_video() {
    AVBSFContext *filter = nullptr;
    const AVBitStreamFilter *spec = av_bsf_get_by_name("h264_mp4toannexb");
    if (!spec || av_bsf_alloc(spec, &filter) < 0)
      return false;
    avcodec_parameters_copy(filter->par_in,
                            media_.format->streams[media_.video]->codecpar);
    filter->time_base_in = media_.format->streams[media_.video]->time_base;
    if (av_bsf_init(filter) < 0) {
      av_bsf_free(&filter);
      return false;
    }
    VideoDecoder decoder{};
    VideoDecoderConfig cfg{};
    cfg.codec = ViddecCodec_H264;
    cfg.width = video_->width;
    cfg.height = video_->height;
    double fps = av_q2d(av_guess_frame_rate(
        media_.format, media_.format->streams[media_.video], nullptr));
    cfg.frame_rate =
        static_cast<u32>(std::clamp(std::ceil(fps > 0 ? fps : 30), 1.0, 60.0));
    cfg.image_count = 4;
    cfg.codec_config = filter->par_out->extradata;
    cfg.codec_config_size = filter->par_out->extradata_size;
    cfg.max_sample_size = 16 * 1024 * 1024;
    logf("player3: VideoDecoder create begin %ux%u %ufps", cfg.width,
         cfg.height, cfg.frame_rate);
    Result rc = videoDecoderCreate(&decoder, &cfg);
    logf("player3: VideoDecoder create end rc=0x%08x", rc);
    if (R_FAILED(rc)) {
      av_bsf_free(&filter);
      log_line("player3: hardware unavailable, using software");
      return false;
    }
    decoder_mode_=2;
    bool eos = false;
    size_t count = 0;

    auto drain = [&]() {
      while (!stop_) {
        VideoFrame raw{};
        Result r = videoDecoderGetFrame(&decoder, &raw, 0);
        if (R_FAILED(r)) {
          if (r == KERNELRESULT(TimedOut))
            break;
          return fail("Hardware get frame: " + std::to_string(r));
        }
        Frame f = frame_new();
        if (!f) {
          videoDecoderReleaseFrame(&decoder, &raw);
          return fail("No hardware frame memory");
        }
        f->format = AV_PIX_FMT_NV12;
        f->color_range=video_->color_range; f->colorspace=video_->colorspace;
        f->width = raw.width;
        f->height = raw.height;
        f->sample_aspect_ratio = video_->sample_aspect_ratio;
        if (av_frame_get_buffer(f.get(), 64) < 0) {
          videoDecoderReleaseFrame(&decoder, &raw);
          return fail("Hardware frame memory");
        }
        r = videoDecoderCopyFrame(&decoder, &raw, f->data[0], f->linesize[0],
                                  f->data[1], f->linesize[1]);
        double t = raw.timestamp_ns / 1e9 - origin_;
        eos = raw.end_of_stream;
        videoDecoderReleaseFrame(&decoder, &raw);
        if (R_FAILED(r))
          return fail("Hardware frame copy: " + std::to_string(r));
        ++count;
        if (!push_picture(std::move(f), t))
          return false;
      }
      return !stop_;
    };
    auto submit = [&](AVPacket *p, bool last) {
      int64_t timestamp = p->pts == AV_NOPTS_VALUE ? p->dts : p->pts;
      if (timestamp == AV_NOPTS_VALUE)
        timestamp = 0;
      int64_t ns = av_rescale_q(timestamp,
                                media_.format->streams[media_.video]->time_base,
                                AVRational{1, 1000000000});
      while (!stop_) {
        if (!drain())
          return false;
        Result r =
            videoDecoderSubmit(&decoder, p->data, p->size, ns, last, 2000000);
        if (R_SUCCEEDED(r))
          return drain();
        if (r != KERNELRESULT(TimedOut) && r != VIDDEC_RESULT_SUBMIT_BUSY)
          return fail("Hardware submit: " + std::to_string(r));
        SDL_Delay(1);
      }
      return false;
    };
    Packet p, held;
    bool ok = true;
    auto filtered = [&]() {
      while (!stop_) {
        auto out = packet_new();
        if (!out)
          return fail("No filtered packet memory");
        int r = av_bsf_receive_packet(filter, out.get());
        if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
          return true;
        if (r < 0)
          return fail("AVC bitstream filter");
        if (held && !submit(held.get(), false))
          return false;
        held = std::move(out);
      }
      return false;
    };
    while (video_packets_.pop(p, stop_)) {
      if (av_bsf_send_packet(filter, p.get()) < 0 || !filtered()) {
        ok = false;
        break;
      }
    }
    if (ok && !stop_) {
      av_bsf_send_packet(filter, nullptr);
      ok = filtered();
      if (ok && held)
        ok = submit(held.get(), true);
      auto deadline = Wall::now() + std::chrono::seconds(2);
      while (ok && !stop_ && !eos && Wall::now() < deadline) {
        ok = drain();
        SDL_Delay(1);
      }
    }
    videoDecoderClose(&decoder);
    av_bsf_free(&filter);
    logf("player3: hardware video exit frames=%zu", count);
    if (ok && !count && !stop_)
      fail("Hardware decoder produced no frames");
    return true;
  }
#endif
  void audio_loop() {
    AVFilterGraph *graph = nullptr;
    AVFilterContext *source = nullptr, *sink = nullptr;
    double cursor = 0;
    bool cursor_set = false;
    Frame decoded = frame_new(), filtered = frame_new();
    if (!decoded || !filtered) {
      fail("No audio frame memory");
      audio_done_ = true;
      sounds_.close();
      return;
    }
    Packet p;
    auto receive = [&]() {
      while (!stop_) {
        int rc = avcodec_receive_frame(audio_, decoded.get());
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
          return true;
        if (rc < 0)
          return fail("Audio decode: " + av_error(rc));
        if (!graph) {
          graph = avfilter_graph_alloc();
          if (!graph)
            return fail("No audio graph memory");
          char layout[128];
          av_channel_layout_describe(&decoded->ch_layout, layout,
                                     sizeof(layout));
          std::string args =
              "time_base=1/" + std::to_string(decoded->sample_rate) +
              ":sample_rate=" + std::to_string(decoded->sample_rate) +
              ":sample_fmt=" +
              av_get_sample_fmt_name(
                  static_cast<AVSampleFormat>(decoded->format)) +
              ":channel_layout=" + layout;
          rc = avfilter_graph_create_filter(
              &source, avfilter_get_by_name("abuffer"), "source", args.c_str(),
              nullptr, graph);
          if (rc >= 0)
            rc = avfilter_graph_create_filter(
                &sink, avfilter_get_by_name("abuffersink"), "sink", nullptr,
                nullptr, graph);
          AVFilterInOut *in = avfilter_inout_alloc(),
                        *out = avfilter_inout_alloc();
          if (!in || !out)
            return fail("Audio filter memory");
          out->name = av_strdup("in");
          out->filter_ctx = source;
          out->pad_idx = 0;
          in->name = av_strdup("out");
          in->filter_ctx = sink;
          in->pad_idx = 0;
          std::string filters =
              "atempo=" + std::to_string(rate_) +
              ",aresample=48000,aformat=sample_fmts=s16:channel_layouts=stereo";
          if (rc >= 0)
            rc = avfilter_graph_parse_ptr(graph, filters.c_str(), &in, &out,
                                          nullptr);
          avfilter_inout_free(&in);
          avfilter_inout_free(&out);
          if (rc >= 0)
            rc = avfilter_graph_config(graph, nullptr);
          if (rc < 0)
            return fail("Audio filters: " + av_error(rc));
          cursor = pts(decoded.get(), *audio_input_, audio_input_->audio);
          cursor_set = true;
        }
        // The filter uses sample-based timestamps; the media cursor below
        // maps its output samples back to the selected playback rate.
        decoded->pts = decoded->best_effort_timestamp == AV_NOPTS_VALUE
                           ? AV_NOPTS_VALUE
                           : av_rescale_q(decoded->best_effort_timestamp,
                                          audio_input_->format
                                              ->streams[audio_input_->audio]
                                              ->time_base,
                                          AVRational{1, decoded->sample_rate});
        rc = av_buffersrc_add_frame_flags(source, decoded.get(),
                                          AV_BUFFERSRC_FLAG_KEEP_REF);
        av_frame_unref(decoded.get());
        if (rc < 0)
          return fail("Audio filter input: " + av_error(rc));
        if (!drain_audio_filter(sink, filtered, cursor))
          return false;
      }
      return false;
    };
    while (audio_packets_.pop(p, stop_)) {
      int rc = avcodec_send_packet(audio_, p.get());
      if (rc < 0) {
        fail("Audio packet: " + av_error(rc));
        break;
      }
      if (!receive())
        break;
    }
    if (!stop_) {
      avcodec_send_packet(audio_, nullptr);
      receive();
      if (source && cursor_set) {
        av_buffersrc_add_frame_flags(source, nullptr, 0);
        drain_audio_filter(sink, filtered, cursor);
      }
    }
    avfilter_graph_free(&graph);
    audio_done_ = true;
    sounds_.close();
  }
  bool drain_audio_filter(AVFilterContext *sink, Frame &f, double &cursor) {
    while (!stop_) {
      int rc = av_buffersink_get_frame(sink, f.get());
      if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
        return true;
      if (rc < 0)
        return fail("Audio filter output: " + av_error(rc));
      double start = cursor;
      cursor += f->nb_samples / 48000.0 * rate_;
      int skip = start < state_.start
                     ? std::min(f->nb_samples,
                                static_cast<int>(std::ceil(
                                    (state_.start - start) / rate_ * 48000)))
                     : 0;
      if (skip < f->nb_samples) {
        auto pcm = std::make_shared<PCM>();
        pcm->pts = start + skip / 48000.0 * rate_;
        const int16_t *samples =
            reinterpret_cast<const int16_t *>(f->data[0]) + skip * 2;
        pcm->samples.assign(samples, samples + (f->nb_samples - skip) * 2);
        if (!sounds_.push(std::move(pcm), stop_)) {
          av_frame_unref(f.get());
          return false;
        }
      }
      av_frame_unref(f.get());
    }
    return false;
  }
  double media_clock() {
    if (!started_)
      return state_.start;
    if (paused_)
      return position_;
    if (has_audio_) {
      double audio_clock = clock_base_ + sink_.played() / 48000.0 * rate_;
      if (audio_done_ && sounds_.size() == 0 && !pending_audio_ &&
          sink_.queued() == 0) {
        if (!audio_tail_) {
          audio_tail_ = true;
          tail_clock_ = audio_clock;
          tail_wall_ = Wall::now();
        }
        return tail_clock_ +
               std::chrono::duration<double>(Wall::now() - tail_wall_).count() *
                   rate_;
      }
      return audio_clock;
    }
    return clock_base_ +
           std::chrono::duration<double>(Wall::now() - wall_base_).count() *
               rate_;
  }
  void restart(double time, int quality = -1) {
    double target = std::max(0.0, time);
    if (duration_ > 0)
      target = std::min(target, std::max(0.0, duration_ - 0.1));
    if (quality < 0 && source_.buffered_ratio && duration_ > 0) {
      double limit = source_.buffered_ratio() * duration_;
      if (limit < duration_ - 0.1 && target > std::max(0.0, limit - 1)) {
        notice_ = newpipe::ui("Posizione non ancora nel buffer");
        hud_until_ = Wall::now() + std::chrono::seconds(3);
        return;
      }
    }
    if(quality<0 && std::abs(state_.speed-rate_)<.01 && state_.software_only==software_only_) {
      pending_seek_=target;scrubbing_=false;dragging_seek_=false;menu_=false;return;
    }
    state_.restart_at = target;
    state_.requested_height = quality;
    restarting_ = true;
    quit_ = true;
  }
  void caption(int index) {
    state_.caption_index = index;
    auto state = captions_;
    int generation = ++state->generation;
    {
      std::lock_guard<std::mutex> l(state->mutex);
      state->cues.clear();
      state->status = index < 0 ? "" : newpipe::ui("Caricamento sottotitoli...");
    }
    if (index < 0 || index >= static_cast<int>(source_.captions.size()))
      return;
    {
      std::lock_guard<std::mutex> lock(caption_job_mutex_);
      caption_pending_ = index;
      caption_pending_generation_ = generation;
    }
    if (!caption_thread_.joinable())
      caption_thread_ = std::thread([this] {
        for (;;) {
          int index, generation;
          {
            std::unique_lock<std::mutex> lock(caption_job_mutex_);
            caption_job_cv_.wait(lock, [this] {
              return caption_stop_ || caption_pending_ != -2;
            });
            if (caption_stop_)
              return;
            index = caption_pending_;
            generation = caption_pending_generation_;
            caption_pending_ = -2;
          }
          auto state = captions_;

          std::vector<CaptionCue> cues;
          std::string error;
          try {
            log_line("captions: fetch begin");
            HttpsHttpClient client;
            auto body = client.get(caption_url_with_format(source_.captions[index].url,"json3"));
            if (body)
              cues = parse_captions(*body);
            if(cues.empty()&&state->generation==generation) {
              auto fallback=client.get(caption_url_with_format(source_.captions[index].url,"vtt"));
              if(fallback)cues=parse_captions(*fallback);
            }
            if (cues.empty()) error = newpipe::ui("Caricamento sottotitoli non riuscito");
            logf("captions: fetch end cues=%zu", cues.size());
          } catch (const std::exception &) {
            error = newpipe::ui("Errore nel caricamento dei sottotitoli");
            log_line("captions: parse/request exception");
          }
          std::lock_guard<std::mutex> lock(state->mutex);
          if (state->generation == generation) {
            state->cues = std::move(cues);
            state->status = error;
          }
        }
      });
    caption_job_cv_.notify_one();
  }
  void begin_options() {
    quality_choice_ = source_.quality.find("1080") != std::string::npos  ? 2
                      : source_.quality.find("720") != std::string::npos ? 1
                                                                         : 0;
    speed_choice_ = 2;
    for (int i = 0; i < 7; ++i)
      if (std::abs(speeds_[i] - state_.speed) < .01)
        speed_choice_ = i;
    caption_choice_ = state_.caption_index;
    decoder_choice_ = state_.software_only;
  }
  void adjust(int direction) {
    if (menu_row_ == 0)
      quality_choice_ = std::clamp(quality_choice_ + direction, 0, 2);
    if (menu_row_ == 1)
      speed_choice_ = std::clamp(speed_choice_ + direction, 0, 6);
    if (menu_row_ == 2)
      caption_choice_ =
          std::clamp(caption_choice_ + direction, -1,
                     static_cast<int>(source_.captions.size()) - 1);
    if (menu_row_ == 3)
      decoder_choice_ = !decoder_choice_;
  }
  void confirm_option() {
    if (menu_row_ == 0) {
      int active = source_.quality.find("1080") != std::string::npos  ? 2
                   : source_.quality.find("720") != std::string::npos ? 1
                                                                      : 0;
      if (quality_choice_ != active)
        restart(position_, qualities_[quality_choice_]);
    }
    if (menu_row_ == 1 &&
        std::abs(speeds_[speed_choice_] - state_.speed) > .01) {
      state_.speed = speeds_[speed_choice_];
      restart(position_);
    }
    if (menu_row_ == 2)
      caption(caption_choice_);
    if (menu_row_ == 3 && decoder_choice_ != state_.software_only) {
      state_.software_only = decoder_choice_;
      restart(position_);
    }
    menu_ = false;
  }
  bool controls_visible() const {
    return menu_ || scrubbing_ || (!controls_hidden_ && (paused_ || Wall::now()<hud_until_));
  }
  bool pump() {
    const bool visible_before_input=controls_visible();
    bool toggle = false, back = false, options = false, timeline = false;
    int horizontal = 0, vertical = 0, seek_jump=0;
    bool activity=false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if(e.type==SDL_FINGERDOWN || e.type==SDL_MOUSEBUTTONDOWN || e.type==SDL_KEYDOWN || e.type==SDL_CONTROLLERBUTTONDOWN)activity=true;
      if (e.type == SDL_QUIT)
        quit_ = true;
      const bool controls_were_visible=controls_visible();
      bool press =
          e.type == SDL_FINGERDOWN ||
          (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT);
      bool move = e.type == SDL_FINGERMOTION || e.type == SDL_MOUSEMOTION;
      bool release =
          e.type == SDL_FINGERUP ||
          (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT);
      if(press) {activity=true;hud_until_=Wall::now()+std::chrono::seconds(5);}
      if ((press || move || release) && duration_ > 0) {
        int ww = 1, hh = 1;
        SDL_GetWindowSize(window_, &ww, &hh);
        bool finger = e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION ||
                      e.type == SDL_FINGERUP;
        float x = finger ? e.tfinger.x
                  : e.type == SDL_MOUSEMOTION
                      ? e.motion.x / static_cast<float>(ww)
                      : e.button.x / static_cast<float>(ww);
        float y = finger ? e.tfinger.y
                  : e.type == SDL_MOUSEMOTION
                      ? e.motion.y / static_cast<float>(hh)
                      : e.button.y / static_cast<float>(hh);
        const float px=x*1280,py=y*720;
        if(press && !controls_were_visible)continue;
        if(press && menu_ && px>=28 && px<=1252 && py>=360 && py<=580) {
          if(py>=380 && py<=428){menu_row_=std::clamp(int((px-44)/298),0,3);menu_tabs_=true;}
          else if(py>=450 && py<=502) {
            menu_tabs_=false;
            int total=menu_row_==0?3:menu_row_==1?7:menu_row_==2?int(source_.captions.size())+1:2;
            int selected=menu_row_==0?quality_choice_:menu_row_==1?speed_choice_:menu_row_==2?caption_choice_+1:int(decoder_choice_);
            int first=std::clamp(selected-3,0,std::max(0,total-7)),count=std::min(7,total);
            int choice=first+std::clamp(int((px-44)/1192*count),0,count-1);
            if(menu_row_==0)quality_choice_=choice;else if(menu_row_==1)speed_choice_=choice;
            else if(menu_row_==2)caption_choice_=choice-1;else decoder_choice_=choice!=0;
            confirm_option();
          }
          continue;
        }
        if(press && menu_)menu_=false;
        if(press && y>.82f && y<.94f) {
          if(x<.18f)toggle=true;
          else if(px>=790) {
            menu_row_=px<940?0:px<1050?1:px<1140?2:3;
            begin_options();options=true;
          } else {timeline=true;}
        }
        if (press && y >= 1.0f - 52.0f / 720.0f) {
          dragging_seek_ = true;
          scrubbing_ = true;
        }
        if (dragging_seek_) {
          seek_preview_ =
              std::clamp((x - 28.0 / 1280.0) / (1 - 56.0 / 1280.0), 0.0, 1.0) *
              std::max(0.0, duration_ - .1);
          if (release) {
            dragging_seek_ = false;
            restart(seek_preview_);
            if (restarting_)
              scrubbing_ = false;
          }
        }
      }
#ifndef __SWITCH__
      if(e.type==SDL_CONTROLLERAXISMOTION && (e.caxis.axis==SDL_CONTROLLER_AXIS_LEFTX || e.caxis.axis==SDL_CONTROLLER_AXIS_RIGHTX) && std::abs(e.caxis.value)>10000) {horizontal=e.caxis.value>0?1:-1;activity=true;}
      if (e.type == SDL_KEYDOWN) {
        auto k = e.key.keysym.sym;
        seek_jump=k==SDLK_e?30:k==SDLK_q?-30:k==SDLK_l?10:k==SDLK_j?-10:0;
        back = k == SDLK_ESCAPE;
        toggle = k == SDLK_SPACE;
        options = k == SDLK_x;
        timeline = k == SDLK_y;
        horizontal = k == SDLK_RIGHT ? 1 : k == SDLK_LEFT ? -1 : 0;
        vertical = k == SDLK_DOWN ? 1 : k == SDLK_UP ? -1 : 0;
      }
#endif
    }
#ifdef __SWITCH__
    if (!appletMainLoop())
      quit_ = true;
    padUpdate(&pad_);
    u64 down = padGetButtonsDown(&pad_);
    if (!(padGetButtons(&pad_) & HidNpadButton_A))
      a_released_ = true;
    toggle = a_released_ && (down & HidNpadButton_A);
    back = down & HidNpadButton_B;
    options = down & HidNpadButton_X;
    timeline = down & HidNpadButton_Y;
    horizontal = down & HidNpadButton_Right  ? 1
                 : down & HidNpadButton_Left ? -1
                                             : 0;
    vertical = down & HidNpadButton_Down ? 1 : down & HidNpadButton_Up ? -1 : 0;
    activity=activity || down!=0;
    auto leftStick=padGetStickPos(&pad_,0),rightStick=padGetStickPos(&pad_,1);
    int axis=std::abs(leftStick.x)>=10000 ? leftStick.x : std::abs(rightStick.x)>=10000 ? rightStick.x : 0;
    if(axis && Wall::now()>=stick_repeat_) {
      horizontal=axis>0?1:-1;activity=true;
      stick_repeat_=Wall::now()+std::chrono::milliseconds(130);
    }
    if(!axis)stick_repeat_=Wall::time_point{};
    if(std::abs(leftStick.y)>10000 || std::abs(rightStick.y)>10000)activity=true;
    seek_jump=(down&HidNpadButton_R?10:0)-(down&HidNpadButton_L?10:0)
           +(down&HidNpadButton_ZR?30:0)-(down&HidNpadButton_ZL?30:0);

#endif
    if(menu_ && seek_jump) {menu_row_=(menu_row_+(seek_jump>0?1:3))%4;menu_tabs_=true;seek_jump=0;}
    if(seek_jump) {
      if(!scrubbing_){scrubbing_=true;seek_preview_=position_;}
      seek_preview_=std::clamp(seek_preview_+seek_jump,0.0,std::max(0.0,duration_-.1));
      menu_=false;
    }

    if(activity && !back){controls_hidden_=false;hud_until_=Wall::now()+std::chrono::seconds(4);}
    if (back) {
      if(visible_before_input) {
        menu_=false;scrubbing_=false;dragging_seek_=false;
        controls_hidden_=true;hud_until_=Wall::time_point{};
      } else quit_=true;
    }
    if (timeline && duration_ > 0) {
      scrubbing_ = !scrubbing_;
      seek_preview_ = position_;
      menu_ = false;
    }
    if (options || (vertical>0 && !menu_ && !scrubbing_)) {
      if(!options)vertical=0;
      menu_tabs_=true;
      menu_ = !menu_;
      scrubbing_ = false;
      dragging_seek_ = false;
      if (menu_)
        begin_options();
      hud_until_ = Wall::now() + std::chrono::seconds(5);
    }
    if (menu_) {
      if(vertical<0)menu_tabs_=true;
      if(vertical>0)menu_tabs_=false;
      if(horizontal){if(menu_tabs_)menu_row_=(menu_row_+horizontal+4)%4;else adjust(horizontal);}
      if(toggle){if(menu_tabs_)menu_tabs_=false;else confirm_option();}
      toggle=false;
    } else if (scrubbing_) {
      if (horizontal)
        seek_preview_ = std::clamp(seek_preview_ + horizontal * 10, 0.0,
                                   std::max(0.0, duration_ - .1));
      if (toggle) {
        restart(seek_preview_);
        if (restarting_)
          scrubbing_ = false;
      }
      toggle = false;
    } else {
      if (horizontal && duration_>0) {scrubbing_=true;seek_preview_=std::clamp(position_+horizontal*10,0.0,std::max(0.0,duration_-.1));}
      if (vertical) {
        volume_ = std::clamp(volume_ - vertical * 0.05f, 0.0f, 1.0f);
        sink_.volume(volume_);
        notice_ =
            newpipe::ui("Volume ") + std::to_string(static_cast<int>(volume_ * 100)) + "%";
      }
    }
    if (toggle && started_) {
      paused_ = !paused_;
      if (has_audio_)
        sink_.pause(paused_);
      if (paused_) {controls_hidden_=false;pause_start_ = Wall::now();}
      else {
        auto paused_time = Wall::now() - pause_start_;
        controls_hidden_=true;hud_until_=Wall::time_point{};
        wall_base_ += paused_time;
        if (audio_tail_)
          tail_wall_ += paused_time;
      }
    }
    if ((toggle && paused_) || horizontal || vertical)
      hud_until_ = Wall::now() + std::chrono::seconds(4);
    return !quit_ && !stop_;
  }
  int texture_mode_=-1;
  bool upload(Frame f) {
      SDL_YUV_CONVERSION_MODE mode = f->color_range==AVCOL_RANGE_JPEG || f->format==AV_PIX_FMT_YUVJ420P
          ? SDL_YUV_CONVERSION_JPEG : f->colorspace==AVCOL_SPC_BT709
          ? SDL_YUV_CONVERSION_BT709 : f->colorspace==AVCOL_SPC_SMPTE170M || f->colorspace==AVCOL_SPC_BT470BG
          ? SDL_YUV_CONVERSION_BT601 : f->height>=720 ? SDL_YUV_CONVERSION_BT709 : SDL_YUV_CONVERSION_BT601;
      SDL_SetYUVConversionMode(mode);

    const auto upload_start=Wall::now();
    bool nv12=f->format==AV_PIX_FMT_NV12;
    bool planar =
        f->format == AV_PIX_FMT_YUV420P || f->format == AV_PIX_FMT_YUVJ420P;
    Uint32 pixel_format=nv12?SDL_PIXELFORMAT_NV12:planar?SDL_PIXELFORMAT_IYUV:SDL_PIXELFORMAT_RGBA32;
    if (!texture_ || texture_format_!=pixel_format || texture_w_ != f->width || texture_h_ != f->height ||
        texture_planar_ != planar || texture_mode_!=mode) {
      if (texture_)
        SDL_DestroyTexture(texture_);
      texture_ = SDL_CreateTexture(
          renderer_, pixel_format,
          SDL_TEXTUREACCESS_STREAMING, f->width, f->height);
      texture_format_=pixel_format;
      texture_mode_=mode;
      texture_w_ = f->width;
      texture_h_ = f->height;
      texture_planar_ = planar;
      if (!texture_)
        return fail(SDL_GetError());
    }
    if(nv12) {
      if(SDL_UpdateNVTexture(texture_,nullptr,f->data[0],f->linesize[0],f->data[1],f->linesize[1])<0)return fail(SDL_GetError());
    } else if (planar) {
      if(frames_==0)logf("video: size=%dx%d range=%d matrix=%d sdl_mode=%d linear=1",f->width,f->height,f->color_range,f->colorspace,mode);
      if (SDL_UpdateYUVTexture(texture_, nullptr, f->data[0], f->linesize[0],
                               f->data[1], f->linesize[1], f->data[2],
                               f->linesize[2]) < 0)
        return fail(SDL_GetError());
    } else {
      scaler_ = sws_getCachedContext(scaler_, f->width, f->height,
                                     static_cast<AVPixelFormat>(f->format),
                                     f->width, f->height, AV_PIX_FMT_RGBA,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
      if (!scaler_)
        return fail("Video conversion");
      const int cs=f->colorspace==AVCOL_SPC_BT709?SWS_CS_ITU709:SWS_CS_ITU601;
      sws_setColorspaceDetails(scaler_,sws_getCoefficients(cs),f->color_range==AVCOL_RANGE_JPEG,
                              sws_getCoefficients(cs),1,0,1<<16,1<<16);
      std::vector<uint8_t> rgba(static_cast<size_t>(f->width) * f->height * 4);
      uint8_t *dst[] = {rgba.data(), nullptr, nullptr, nullptr};
      int stride[] = {f->width * 4, 0, 0, 0};
      sws_scale(scaler_, f->data, f->linesize, 0, f->height, dst, stride);
      if (SDL_UpdateTexture(texture_, nullptr, rgba.data(), stride[0]) < 0)
        return fail(SDL_GetError());
    }
    upload_us_+=std::chrono::duration_cast<std::chrono::microseconds>(Wall::now()-upload_start).count();
    displayed_ = std::move(f);
    return true;
  }
  void overlay(int w, int h) {
    float scale = h / 720.0f;
    auto unit = [&](int n) { return static_cast<int>(n * scale); };
    auto label = [&](const std::string &s, int x, int y, int size,
                     SDL_Color color = SDL_Color{240, 242, 248, 255}) {
      text_->draw(s, unit(x), unit(y), unit(size), color, w - unit(x + 24));
    };
    std::string cue, status;
    {
      std::lock_guard<std::mutex> l(captions_->mutex);
      cue = caption_at(captions_->cues, position_);
      status = captions_->status;
    }
    if (!cue.empty()) {
      std::vector<std::string> lines;
      std::istringstream words(cue);
      std::string word, line;
      while (words >> word) {
        std::string next = line.empty() ? word : line + " " + word;
        if (text_->width(next, unit(30)) > w - unit(140) && !line.empty()) {
          lines.push_back(line);
          line = word;
        } else
          line = next;
      }
      if (!line.empty())
        lines.push_back(line);
      if (lines.size() > 3)
        lines.erase(lines.begin(), lines.end() - 3);
      int y =
          h - unit((menu_ ? 370 : controls_visible() ? 145 : 35) + static_cast<int>(lines.size()) * 38);
      for (const auto &s : lines) {
        int sw = text_->width(s, unit(30));
        SDL_Rect background{(w - sw) / 2 - unit(12), y, sw + unit(24),
                            unit(42)};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 210);
        SDL_RenderFillRect(renderer_, &background);
        text_->draw(s, (w - sw) / 2, y, unit(30), {255, 255, 255, 255});
        y += unit(40);
      }
    }
    if (menu_) {
      SDL_Rect panel{unit(28),unit(360),w-unit(56),unit(220)};
      SDL_SetRenderDrawColor(renderer_,15,19,27,245);SDL_RenderFillRect(renderer_,&panel);
      std::vector<std::string> tabs={newpipe::ui("Qualita: "),newpipe::ui("Velocita: "),newpipe::ui("Sottotitoli: "),newpipe::ui("Video: ")};
      for(int i=0;i<4;++i){
        SDL_Rect tab{unit(44+i*298),unit(380),unit(284),unit(48)};
        SDL_SetRenderDrawColor(renderer_,i==menu_row_?(menu_tabs_?180:95):30,i==menu_row_?34:35,i==menu_row_?45:46,255);SDL_RenderFillRect(renderer_,&tab);
        text_->draw(tabs[i],tab.x+unit(14),tab.y+unit(12),unit(20),{245,245,248,255},tab.w-unit(24));
      }
      std::vector<std::string> choices;int selected=0;
      if(menu_row_==0){for(int value:qualities_)choices.push_back(std::to_string(value)+"p");selected=quality_choice_;}
      if(menu_row_==1){for(double value:speeds_)choices.push_back(std::to_string(value).substr(0,4)+"x");selected=speed_choice_;}
      if(menu_row_==2){choices.push_back(newpipe::ui("Disattivati"));for(const auto& track:source_.captions)choices.push_back(track.label);selected=caption_choice_+1;}
      if(menu_row_==3){choices={newpipe::ui("Automatico"),newpipe::ui("Compatibile")};selected=decoder_choice_;}
      int count=std::min(7,int(choices.size())),first=std::clamp(selected-3,0,std::max(0,int(choices.size())-7));
      for(int j=0;j<count;++j){int index=first+j;int cw=unit(1192)/count;
        SDL_Rect chip{unit(44)+j*cw,unit(450),cw-unit(8),unit(52)};
        SDL_SetRenderDrawColor(renderer_,index==selected?230:40,index==selected?53:46,index==selected?76:60,255);SDL_RenderFillRect(renderer_,&chip);
        text_->draw(choices[index],chip.x+unit(10),chip.y+unit(16),unit(18),{255,255,255,255},chip.w-unit(20));
      }
      label(newpipe::ui("Sinistra/destra: categorie • Giù: valori • Su: categorie • A: conferma • B: chiudi"),44,536,18);
    }
    if (controls_visible()) {
      SDL_Rect panel{0, h - unit(126), w, unit(126)};
      SDL_SetRenderDrawColor(renderer_, 8, 11, 18, 230);
      SDL_RenderFillRect(renderer_, &panel);
      label(source_.title, 28, 602, 25);
      for(const auto& box:std::vector<SDL_Rect>{{unit(20),unit(630),unit(200),unit(40)},
          {unit(775),unit(630),unit(160),unit(40)},{unit(940),unit(630),unit(100),unit(40)},
          {unit(1045),unit(630),unit(90),unit(40)},{unit(1140),unit(630),unit(112),unit(40)}}) {
        SDL_SetRenderDrawColor(renderer_,42,48,62,245);SDL_RenderFillRect(renderer_,&box);
      }
      label(std::string("A  ")+(paused_?newpipe::ui("Riprendi"):newpipe::ui("Pausa")),32,641,18,{245,245,250,255});
      label(time_text(scrubbing_ ? seek_preview_ : position_)+" / "+time_text(duration_),260,644,19);
      label(source_.quality,790,644,18);
      label(std::to_string(state_.speed).substr(0,4)+"x",950,644,18);
      label("CC",1060,644,18);label(state_.software_only?newpipe::ui("Compatibile"):newpipe::ui("Automatico"),1148,644,14);
      int bar_y = h - unit(20);
      SDL_Rect track{unit(28), bar_y, w - unit(56), unit(4)};
      SDL_SetRenderDrawColor(renderer_, 50, 58, 72, 255);
      SDL_RenderFillRect(renderer_, &track);
      if (duration_ > 0) {
        SDL_Rect progress = track;
        progress.w = static_cast<int>(
            track.w * std::clamp(position_ / duration_, 0.0, 1.0));
        SDL_SetRenderDrawColor(renderer_, 255, 75, 100, 255);
        SDL_RenderFillRect(renderer_, &progress);
      }
      if (scrubbing_ && duration_ > 0) {
        int thumb =
            track.x +
            static_cast<int>(track.w *
                             std::clamp(seek_preview_ / duration_, 0.0, 1.0));
        SDL_Rect marker{thumb - unit(5), bar_y - unit(7), unit(10), unit(18)};
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer_, &marker);
        label(newpipe::ui("Scegli il punto: sinistra/destra 10s, poi A. Oppure trascina la barra."),
              28, 566, 18);
      }
      if (!notice_.empty())
        label(notice_, 28, 566, 18, {255, 208, 142, 255});
      if (!status.empty())
        label(status, 28, 540, 18);
    }
  }
  void render() {
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(renderer_, &w, &h);
    SDL_SetRenderDrawColor(renderer_, 4, 7, 12, 255);
    SDL_RenderClear(renderer_);
    if (displayed_ && texture_) {
      double sar = displayed_->sample_aspect_ratio.num > 0
                       ? av_q2d(displayed_->sample_aspect_ratio)
                       : 1;
      double ratio = displayed_->width * sar / displayed_->height;
      int rw = w, rh = static_cast<int>(w / ratio);
      if (rh > h) {
        rh = h;
        rw = static_cast<int>(h * ratio);
      }
      SDL_Rect dst{(w - rw) / 2, (h - rh) / 2, rw, rh};
      SDL_RenderCopy(renderer_, texture_, nullptr, &dst);
    }
    if (!displayed_) {
      loading_screen(newpipe::ui("Buffer iniziale...   B - Indietro")); return;
    }
    if(seeking_) {
      SDL_Rect busy{w/2-24,h/2-3,48,6};SDL_SetRenderDrawColor(renderer_,255,80,100,220);SDL_RenderFillRect(renderer_,&busy);
    }
    if(started_ && !paused_ && !video_done_ && Wall::now()-last_frame_at_>std::chrono::seconds(2))
      text_->draw(newpipe::ui("Buffering in corso..."),32,50,24,{255,255,255,255},w-64);
    overlay(w, h);
    SDL_RenderPresent(renderer_);
  }

public:
  Player(SDL_Window *window, const NativeMediaInput &input)
      : window_(window), source_(input),
        state_(input.state ? *input.state : local_), rate_(state_.speed),
        software_only_(state_.software_only) {}
  ~Player() {
    {
      std::lock_guard<std::mutex> lock(caption_job_mutex_);
      caption_stop_ = true;
    }
    caption_job_cv_.notify_one();
    ++captions_->generation;
    if (caption_thread_.joinable())
      caption_thread_.join();
    stop_ = true;
    source_.cancelled->store(true);
    if (source_.audio)
      source_.audio->cancelled->store(true);
    wake();
    if (demux_.joinable())
      demux_.join();
    if (extra_demux_.joinable())
      extra_demux_.join();
    if (video_thread_.joinable())
      video_thread_.join();
    if (audio_thread_.joinable())
      audio_thread_.join();
    ++captions_->generation;
    text_.reset();
    if (texture_)
      SDL_DestroyTexture(texture_);
    if(loading_thumbnail_)SDL_DestroyTexture(loading_thumbnail_);
    if (renderer_)
      SDL_DestroyRenderer(renderer_);
    if (scaler_)
      sws_freeContext(scaler_);
    avcodec_free_context(&video_);
    avcodec_free_context(&audio_);
  }
  bool run(std::string &error) {
    try {
      bool ok = run_inner();
      std::lock_guard<std::mutex> l(error_mutex_);
      error = error_;
      if(!ok && state_.start>0 && frames_==0 && !state_.resume_fallback && !quit_) {
        log_line("resume: first read failed after seek, retrying safely from beginning");
        state_.resume_fallback=true;state_.restart_at=0;error.clear();return true;
      }
      if(started_ && !source_.resume_key.empty() && !restarting_)
        if(!save_resume(source_.resume_key,position_,duration_)) log_line("resume: save failed");
      logf("player3: exit ok=%d frames=%llu dropped=%llu pos=%.3f", ok,
           (unsigned long long)frames_, (unsigned long long)dropped_,
           position_);
      return ok;
    } catch (const std::exception &e) {
      error = e.what();
      return false;
    }
  }
  void start_workers() {
    demux_ = std::thread([this] {
      try {
        demux(&media_, true, !source_.audio && has_audio_);
      } catch (const std::exception &e) {
        fail(e.what());
      }
    });
    if (source_.audio)
      extra_demux_ = std::thread([this] {
        try {
          demux(&external_, false, has_audio_);
        } catch (const std::exception &e) {
          fail(e.what());
        }
      });
    video_thread_ = std::thread([this] {
      try {
        video_loop();
      } catch (const std::exception &e) {
        fail(e.what());
        video_done_ = true;
      }
    });
    if (has_audio_)
      audio_thread_ = std::thread([this] {
        try {
          audio_loop();
        } catch (const std::exception &e) {
          fail(e.what());
          audio_done_ = true;
        }
      });
  }
  bool seek_in_place(double target) {
    const auto began=Wall::now();const double old_position=position_;
    seeking_=true;stop_=true;source_.cancelled->store(true);wake();
    if(has_audio_)sink_.pause(true);
    std::atomic_bool done{false},cancel{false};int result=0;
    std::thread worker([&]{
      if(demux_.joinable())demux_.join();if(extra_demux_.joinable())extra_demux_.join();
      if(video_thread_.joinable())video_thread_.join();if(audio_thread_.joinable())audio_thread_.join();
      if(!cancel.load()) {
        stop_=false;source_.cancelled->store(false);
        if(cancel.load()){stop_=true;source_.cancelled->store(true);}
        auto seek=[&](double at) {
          for(auto* input:{&media_,&external_})if(input->format && input->format->pb){input->format->pb->error=0;input->format->pb->eof_reached=0;}
          int rc=avformat_seek_file(media_.format,-1,INT64_MIN,(at+origin_)*AV_TIME_BASE,INT64_MAX,AVSEEK_FLAG_BACKWARD);
          if(rc>=0 && source_.audio)rc=avformat_seek_file(external_.format,-1,INT64_MIN,(at+origin_)*AV_TIME_BASE,INT64_MAX,AVSEEK_FLAG_BACKWARD);
          return rc;
        };
        result=seek(target);
        if(result<0 && !cancel.load()) {target=old_position;result=seek(target);}
      }
      done=true;
    });
    while(!done.load()) {
      auto back=[&]{if(controls_visible()){menu_=false;scrubbing_=false;controls_hidden_=true;hud_until_=Wall::time_point{};}else cancel=true;};
      SDL_Event e;
      while(SDL_PollEvent(&e)) {
        if(e.type==SDL_QUIT)cancel=true;
        if(e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE)back();
      }
#ifdef __SWITCH__
      padUpdate(&pad_);if(padGetButtonsDown(&pad_)&HidNpadButton_B)back();
      if(!appletMainLoop())cancel=true;
#endif
      if(cancel){stop_=true;source_.cancelled->store(true);}
      render();SDL_Delay(8);
    }
    worker.join();seeking_=false;
    if(cancel){quit_=true;error_.clear();return false;}
    if(result<0)return fail("Seek: "+av_error(result));
    stop_=false;source_.cancelled->store(false);
    {std::lock_guard<std::mutex> lock(error_mutex_);error_.clear();}
    video_packets_.reset();audio_packets_.reset();pictures_.reset();sounds_.reset();
    avcodec_flush_buffers(video_);if(audio_)avcodec_flush_buffers(audio_);
    video_done_=false;audio_done_=!has_audio_;pending_audio_.reset();audio_offset_=0;
    first_audio_=false;audio_tail_=false;started_=false;
    state_.start=target;position_=target;clock_base_=target;seek_frame_pending_=true;
    if(has_audio_){if(!sink_.reset(error_))return false;sink_.volume(volume_);sink_.pause(true);}
    ++state_.seek_count;state_.last_seek=target;
    logf("timing: seek_in_place_ms=%lld target=%.3f count=%u",(long long)std::chrono::duration_cast<std::chrono::milliseconds>(Wall::now()-began).count(),target,state_.seek_count);
    start_workers();return true;
  }
  SDL_Texture* loading_thumbnail_=nullptr;
  Wall::time_point last_frame_at_=Wall::now(),stick_repeat_{};
  void loading_screen(const std::string& status) {
    int w=0,h=0;SDL_GetRendererOutputSize(renderer_,&w,&h);
    SDL_SetRenderDrawColor(renderer_,8,11,18,255);SDL_RenderClear(renderer_);
    if(loading_thumbnail_) {
      SDL_Rect image{0,0,w,h};SDL_RenderCopy(renderer_,loading_thumbnail_,nullptr,&image);
    }
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_,0,0,0,105);SDL_Rect shade{0,0,w,h};SDL_RenderFillRect(renderer_,&shade);
    for(int i=0;i<8;++i){double angle=(i+SDL_GetTicks()/80%8)*.785398;
      SDL_Rect dot{w/2+int(std::cos(angle)*22)-3,h/2+int(std::sin(angle)*22)-3,6,6};
      SDL_SetRenderDrawColor(renderer_,255,255,255,60+i*24);SDL_RenderFillRect(renderer_,&dot);
    }
    text_->draw(source_.title,28,h-55,23,{255,255,255,255},w-56);
    SDL_RenderPresent(renderer_);
  }
  bool run_inner() {
    logf("player3: start rate=%.2f resume=%.3f", state_.speed, state_.start);
    state_.restart_at = -1;
    if(state_.resume_fallback){notice_=newpipe::ui("Ripresa non disponibile: video riavviato");hud_until_=Wall::now()+std::chrono::seconds(6);}
    state_.requested_height = -1;
#ifdef __SWITCH__
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad_);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
#endif
    Uint32 flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
#ifndef __SWITCH__
    if (SDL_GetCurrentVideoDriver() &&
        std::string(SDL_GetCurrentVideoDriver()) == "dummy")
      flags = SDL_RENDERER_SOFTWARE;
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
    renderer_ = SDL_CreateRenderer(window_, -1, flags);
    if (!renderer_)
      return fail(SDL_GetError());
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    text_ = std::make_unique<NativeText>(renderer_);
#ifdef __SWITCH__
    text_->load("romfs:/font/ui.ttf");
#else
    text_->load("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
#endif
    loading_thumbnail_=text_->thumbnail(source_.thumbnail_jpeg);
    render();
    std::atomic_int opening_phase{0};
    std::atomic_bool opened{false};
    bool open_ok=false;
    const auto opening_started=Wall::now();
    std::thread opener([&] {
      try { open_ok = [&]() -> bool {
    media_.source = source_;
    media_.stop = &stop_;
    if (!media_.open(error_))
      return false;
    if (media_.video < 0)
      return fail("No video stream");
    if (source_.audio) {
      opening_phase=1;
      external_.source = *source_.audio;
      external_.stop = &stop_;
      if (!external_.open(error_))
        return false;
      audio_input_ = &external_;
    } else
      audio_input_ = &media_;
    has_audio_ = audio_input_->audio >= 0;
    origin_ =
        media_.format->start_time == AV_NOPTS_VALUE
            ? 0
            : media_.format->start_time / static_cast<double>(AV_TIME_BASE);
    duration_ =
        media_.format->duration > 0
            ? media_.format->duration / static_cast<double>(AV_TIME_BASE)
            : 0;
    if (state_.start > 0.05) {
      opening_phase=2;
      if(source_.prepare_seek && !source_.prepare_seek(state_.start,duration_)) {
        if(stop_)return false;
        state_.start=0;notice_=newpipe::ui("Ripresa non disponibile: video riavviato");
        hud_until_=Wall::now()+std::chrono::seconds(6);
      }
    }
    if (state_.start > 0.05) {
      int rc = avformat_seek_file(media_.format, -1, INT64_MIN,
                                  (state_.start + origin_) * AV_TIME_BASE,
                                  INT64_MAX, AVSEEK_FLAG_BACKWARD);
      if (rc >= 0 && source_.audio)
        rc = avformat_seek_file(external_.format, -1, INT64_MIN,
                                (state_.start + origin_) * AV_TIME_BASE,
                                INT64_MAX, AVSEEK_FLAG_BACKWARD);
      if (rc < 0) {
        logf("player3: resume unavailable, reopening from beginning: %s",
             av_error(rc).c_str());
        avformat_close_input(&media_.format);
        if (media_.io) {
          av_freep(&media_.io->buffer);
          avio_context_free(&media_.io);
        }
        if (source_.seek)
          source_.seek(source_.cookie, 0, SEEK_SET);
        if (!media_.open(error_))
          return false;
        if (source_.audio) {
          avformat_close_input(&external_.format);
          if (external_.io) {
            av_freep(&external_.io->buffer);
            avio_context_free(&external_.io);
          }
          if (source_.audio->seek)
            source_.audio->seek(source_.audio->cookie, 0, SEEK_SET);
          if (!external_.open(error_))
            return false;
        }
        state_.start = 0;
        notice_ = newpipe::ui("Ripresa non disponibile: video riavviato");
        hud_until_ = Wall::now() + std::chrono::seconds(6);
      }
    }
        return true;
      }(); } catch(const std::exception& e) { error_=e.what(); }
      opened.store(true);
    });
    while(!opened.load()) {
      bool cancel=false; SDL_Event e;
      while(SDL_PollEvent(&e)) {
        if(e.type==SDL_QUIT || (e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE)) cancel=true;
      }
#ifdef __SWITCH__
      padUpdate(&pad_);
      cancel=cancel || (padGetButtonsDown(&pad_)&HidNpadButton_B) || !appletMainLoop();
#endif
      if(cancel) { quit_=true; stop_=true; source_.cancelled->store(true); }
      loading_screen(quit_ ? newpipe::ui("Annullamento...") :
        opening_phase==1 ? newpipe::ui("Preparazione audio...") :
        opening_phase==2 ? newpipe::ui("Ricerca del punto salvato...") :newpipe::ui("Preparazione video..."));
      SDL_Delay(16);
    }
    opener.join();
    logf("timing: open_ms=%lld", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(Wall::now()-opening_started).count());
    if(quit_) { error_.clear(); return true; }
    if(!open_ok) return false;
    if (!codec(media_, media_.video, &video_))
      return false;
    if (has_audio_) {
      if (!codec(*audio_input_, audio_input_->audio, &audio_) ||
          !sink_.open(error_))
        return false;
      sink_.pause(true);
    } else
      audio_done_ = true;
    start_workers();
    if (state_.caption_index >= static_cast<int>(source_.captions.size()))
      state_.caption_index = -1;
    caption(state_.caption_index);
    double last_report = -10, last_saved=-30;
    while (pump()) {
      if(pending_seek_>=0) {
        double at=pending_seek_;pending_seek_=-1;
        if(!seek_in_place(at))return quit_;
        continue;
      }
      if (has_audio_) {
        std::string sink_error;
        if (!sink_.update(sink_error))
          return fail(sink_error);
        for (int n = 0; n < 12; ++n) {
          if (!pending_audio_) {
            if (!sounds_.take(pending_audio_))
              break;
            audio_offset_ = 0;
          }
          if (!first_audio_) {
            clock_base_ = pending_audio_->pts;
            first_audio_ = true;
          }
          size_t left = pending_audio_->samples.size() / 2 - audio_offset_;
          size_t sent = sink_.enqueue(
              pending_audio_->samples.data() + audio_offset_ * 2, left);
          if (!sent)
            break;
          audio_offset_ += sent;
          if (audio_offset_ == pending_audio_->samples.size() / 2)
            pending_audio_.reset();
        }
      }
      if (!started_ && pictures_.size() > 0 &&
          (!has_audio_ || sink_.queued() >= 2048 || audio_done_)) {
        started_ = true;
        wall_base_ = Wall::now();if(paused_)pause_start_=wall_base_;
        if (has_audio_)
          sink_.pause(paused_);
        else {
          Video first;
          if (pictures_.front(first))
            clock_base_ = first.pts;
        }
        log_line("player3: playback clock started");
      }
      if (started_) {
        position_ = media_clock();
        if (!paused_ || seek_frame_pending_) {
          Video next, chosen;
          bool have = false;
          while (pictures_.front(next) && (next.pts <= position_ + 0.012 || (paused_ && seek_frame_pending_))) {
            pictures_.take(next);
            if (have)
              ++dropped_;
            chosen = std::move(next);
            have = true;
            if(paused_ && seek_frame_pending_)break;
          }
          if (have) {
            if (!upload(chosen.frame))
              return false;
            seek_frame_pending_=false;
            displayed_pts_ = chosen.pts; last_frame_at_=Wall::now();
            if (++frames_ == 1) {
              log_line("player3: FIRST VIDEO FRAME PRESENTED");
              logf("timing: first_frame_ms=%lld",(long long)std::chrono::duration_cast<std::chrono::milliseconds>(Wall::now()-opening_started).count());
            }
          }
        }
        if (position_ - last_report >= 5) {
          logf("player3: clock=%.3f video=%.3f delta_ms=%.1f audio_queued=%llu "
               "frames=%llu dropped=%llu",
               position_, displayed_pts_, (displayed_pts_ - position_) * 1000,
               (unsigned long long)sink_.queued(), (unsigned long long)frames_,
               (unsigned long long)dropped_);
          logf("perf: decoder=%s decoded=%llu vpackets=%zu pictures=%zu upload_avg_ms=%.2f video_read_ms=%llu slow_reads=%llu",
               decoder_mode_==2?"HW":"SW",(unsigned long long)decoded_frames_.load(),video_packets_.size(),pictures_.size(),
               frames_?upload_us_/1000.0/frames_:0.,(unsigned long long)video_read_ms_.load(),(unsigned long long)video_read_slow_.load());
          last_report = position_;
          if(!source_.resume_key.empty() && std::abs(position_-last_saved)>=30) {save_resume(source_.resume_key,position_,duration_);last_saved=position_;}
        }
      }
      render();
      if (video_done_ && audio_done_ && pictures_.size() == 0 &&
          sounds_.size() == 0 && !pending_audio_ &&
          (!has_audio_ || sink_.queued() == 0))
        break;
      SDL_Delay(1);
    }
    return !stop_ || quit_ || restarting_;
  }
};
} // namespace
} // namespace newpipe
namespace newpipe {
bool run_native_media(SDL_Window *window, const NativeMediaInput &input,
                      std::string &error) {
  Player player(window, input);
  return player.run(error);
}
} // namespace newpipe
