// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created   : Thu May 27 21:03:47 MDT 2010
// Copyright : Pavel Koshevoy
// License   : MIT -- http://www.opensource.org/licenses/mit-license.php

// standard:
#include <map>
#include <set>

// boost:
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

// yae includes:
#include "yae/ffmpeg/yae_ffmpeg_utils.h"
#include "yae/ffmpeg/yae_track.h"

// namespace shortcuts:
namespace al = boost::algorithm;


namespace yae
{

  //----------------------------------------------------------------
  // AvPkt::AvPkt
  //
  AvPkt::AvPkt(const AVPacket * pkt):
    packet_(av_packet_alloc()),
    pbuffer_(NULL),
    demuxer_(NULL),
    program_(0)
  {
    if (pkt)
    {
      av_packet_ref(packet_, pkt);
    }
    else
    {
      av_init_packet(packet_);
    }
  }

  //----------------------------------------------------------------
  // AvPkt::AvPkt
  //
  AvPkt::AvPkt(const AvPkt & pkt):
    packet_(av_packet_alloc()),
    pbuffer_(pkt.pbuffer_),
    demuxer_(pkt.demuxer_),
    program_(pkt.program_),
    trackId_(pkt.trackId_)
  {
    av_packet_ref(packet_, pkt.packet_);
  }

  //----------------------------------------------------------------
  // AvPkt::~AvPkt
  //
  AvPkt::~AvPkt()
  {
    av_packet_free(&packet_);
  }

  //----------------------------------------------------------------
  // AvPkt::operator =
  //
  AvPkt &
  AvPkt::operator = (const AvPkt & pkt)
  {
    if (this != &pkt)
    {
      av_packet_unref(packet_);
      av_packet_ref(packet_, pkt.packet_);

      demuxer_ = pkt.demuxer_;
      trackId_ = pkt.trackId_;
      program_ = pkt.program_;
      pbuffer_ = pkt.pbuffer_;
    }

    return *this;
  }

  //----------------------------------------------------------------
  // clone
  //
  TPacketPtr
  clone(const TPacketPtr & packet_ptr)
  {
    return packet_ptr ? TPacketPtr(new AvPkt(*packet_ptr)) : TPacketPtr();
  }

  //----------------------------------------------------------------
  // AvFrm::AvFrm
  //
  AvFrm::AvFrm(const AVFrame * frame):
    frame_(av_frame_alloc())
  {
    if (frame)
    {
      av_frame_ref(frame_, frame);
    }
  }

  //----------------------------------------------------------------
  // AvFrm::AvFrm
  //
  AvFrm::AvFrm(const AvFrm & frame):
    frame_(av_frame_alloc())
  {
    av_frame_ref(frame_, frame.frame_);
  }

  //----------------------------------------------------------------
  // AvFrm::~AvFrm
  //
  AvFrm::~AvFrm()
  {
    av_frame_free(&frame_);
  }

  //----------------------------------------------------------------
  // AvFrm::operator =
  //
  AvFrm &
  AvFrm::operator = (const AvFrm & frame)
  {
    if (this != &frame)
    {
      av_frame_unref(frame_);
      av_frame_ref(frame_, frame.frame_);
    }

    return *this;
  }


  //----------------------------------------------------------------
  // AvCodecContextPtr::destroy
  //
  void
  AvCodecContextPtr::destroy(AVCodecContext * ctx)
  {
    avcodec_close(ctx);
    avcodec_free_context(&ctx);
  }


  //----------------------------------------------------------------
  // tryToOpen
  //
  AvCodecContextPtr
  tryToOpen(const AVCodec * c,
            const AVCodecParameters * params,
            AVDictionary * opts)
  {
    unsigned int nthreads = boost::thread::hardware_concurrency();
    nthreads = std::min<unsigned int>(16, nthreads);

    AvCodecContextPtr ctx(avcodec_alloc_context3(c));
    if (params)
    {
      avcodec_parameters_to_context(ctx.get(), params);
    }

    av_dict_set_int(&opts, "threads", nthreads, 0);

    int err = avcodec_open2(ctx.get(), c, &opts);
    if (err < 0)
    {
      return AvCodecContextPtr();
    }

    return ctx;
  }


  //----------------------------------------------------------------
  // verify_pts
  //
  // verify that presentation timestamps are monotonically increasing
  //
  bool
  verify_pts(bool hasPrevPTS,
             const TTime & prevPTS,
             const TTime & nextPTS,
             const AVStream * stream,
             const char * debugMessage)
  {
    bool ok = (int64_t(nextPTS.time_) != AV_NOPTS_VALUE &&
               int64_t(nextPTS.base_) != AV_NOPTS_VALUE &&
               nextPTS.base_ != 0 &&
               (!hasPrevPTS ||
                (prevPTS.base_ == nextPTS.base_ ?
                 prevPTS.time_ < nextPTS.time_ :
                 prevPTS.sec() < nextPTS.sec())));
#if 0
    if (ok && debugMessage)
    {
      std::cerr << "PTS OK: "
                << nextPTS.time_ << "/" << nextPTS.base_
                << " = " << nextPTS.to_hhmmss_frac(1000)
                << ", " << debugMessage << std::endl;
    }
#else
    (void)debugMessage;
#endif

    if (!ok || !stream)
    {
      return ok;
    }

    return true;
  }



  //----------------------------------------------------------------
  // Track::Track
  //
  Track::Track(AVFormatContext * context, AVStream * stream):
    thread_(this),
    context_(context),
    stream_(stream),
    preferSoftwareDecoder_(false),
    switchDecoderToRecommended_(false),
    sent_(0),
    received_(0),
    errors_(0),
    timeIn_(0.0),
    timeOut_(kMaxDouble),
    playbackEnabled_(false),
    startTime_(0),
    tempo_(1.0),
    discarded_(0),
    packetQueue_(kQueueSizeLarge)
  {
    if (context_ && stream_)
    {
      YAE_ASSERT(context_->streams[stream_->index] == stream_);
    }
  }

  //----------------------------------------------------------------
  // Track::Track
  //
  Track::Track(Track & track):
    thread_(this),
    context_(NULL),
    stream_(NULL),
    preferSoftwareDecoder_(track.preferSoftwareDecoder_),
    switchDecoderToRecommended_(track.switchDecoderToRecommended_),
    timeIn_(0.0),
    timeOut_(kMaxDouble),
    playbackEnabled_(false),
    startTime_(0),
    tempo_(1.0),
    discarded_(0),
    packetQueue_(kQueueSizeLarge)
  {
    std::swap(context_, track.context_);
    std::swap(stream_, track.stream_);
    std::swap(codecContext_, track.codecContext_);
  }

  //----------------------------------------------------------------
  // Track::~Track
  //
  Track::~Track()
  {
    threadStop();
    close();
  }

  //----------------------------------------------------------------
  // TDecoderMap
  //
  typedef std::map<AVCodecID, std::set<const AVCodec *> > TDecoderMap;

  //----------------------------------------------------------------
  // TDecoders
  //
  struct TDecoders : public TDecoderMap
  {

    //----------------------------------------------------------------
    // TDecoders
    //
    TDecoders()
    {
      ensure_ffmpeg_initialized();

      void * opaque = NULL;
      for (const AVCodec * c = av_codec_iterate(&opaque); c; c = av_codec_iterate(&opaque))
      {
        if (av_codec_is_decoder(c))
        {
          TDecoderMap::operator[](c->id).insert(c);
        }
      }
    }

    //----------------------------------------------------------------
    // find
    //
    // FIXME: perhaps should also include a list of acceptable
    // output pixel formats (or sample formats)
    //
    void
    find(const AVCodecParameters & params,
         std::list<AvCodecContextPtr> & decoders,
         bool preferSoftwareDecoder) const
    {
      decoders.clear();

      TDecoderMap::const_iterator found = TDecoderMap::find(params.codec_id);
      if (found == TDecoderMap::end())
      {
        return;
      }

      std::list<AvCodecContextPtr> hardware;
      std::list<AvCodecContextPtr> software;
      std::list<AvCodecContextPtr> experimental;

      typedef std::set<const AVCodec *> TCodecs;
      const TCodecs & codecs = found->second;
      for (TCodecs::const_iterator i = codecs.begin(); i != codecs.end(); ++i)
      {
        const AVCodec * c = *i;
        if (c->capabilities & AV_CODEC_CAP_EXPERIMENTAL)
        {
          AvCodecContextPtr ctx = tryToOpen(c, &params);
          if (ctx)
          {
            experimental.push_back(ctx);
          }
        }
        else if (al::ends_with(c->name, "_cuvid"))
        {
          if (params.format != AV_PIX_FMT_YUV420P &&
              params.codec_id != AV_CODEC_ID_MJPEG)
          {
            // 4:2:0 is the only one that works with h264_cuvud and mpeg2_cuvid
            // however, 4:2:2 and 4:4:4 work with mjpeg_cuvid
            continue;
          }

          // verify that the GPU can handle this stream:
          AvCodecContextPtr ctx = tryToOpen(c, &params);
          if (ctx)
          {
            hardware.push_front(ctx);
          }
        }
        else if (al::ends_with(c->name, "_qsv") ||
                 al::ends_with(c->name, "_v4l2m2m") ||
                 al::ends_with(c->name, "_vda") ||
                 al::ends_with(c->name, "_vdpau"))
        {
          if (al::ends_with(c->name, "_vda"))
          {
            // h264_vda decoder output is corrupted on 2010 macmini,
            // even though there are no decoder errors reported,
            // therefore it cannot be trusted and is explicitly disbaled:
            continue;
          }

          // verify that the GPU can handle this stream:
          AvCodecContextPtr ctx = tryToOpen(c, &params);
          if (ctx)
          {
            hardware.push_back(ctx);
          }
        }
        else
        {
          AvCodecContextPtr ctx = tryToOpen(c, &params);
          if (ctx)
          {
            software.push_back(ctx);
          }
        }
      }

      if (preferSoftwareDecoder)
      {
        decoders.splice(decoders.end(), software);
        decoders.splice(decoders.end(), hardware);
      }
      else
      {
        decoders.splice(decoders.end(), hardware);
        decoders.splice(decoders.end(), software);
      }

      decoders.splice(decoders.end(), experimental);
    }
  };

  //----------------------------------------------------------------
  // get_decoders
  //
  static const TDecoders & get_decoders()
  {
    static const TDecoders decoders;
    return decoders;
  }

  //----------------------------------------------------------------
  // find_best_decoder_for
  //
  AvCodecContextPtr
  find_best_decoder_for(const AVCodecParameters & params,
                        std::list<AvCodecContextPtr> & untried,
                        bool preferSoftwareDecoder)
  {
    const TDecoders & decoders = get_decoders();

    if (untried.empty())
    {
      decoders.find(params, untried, preferSoftwareDecoder);
    }

    if (untried.empty())
    {
      return AvCodecContextPtr();
    }

    AvCodecContextPtr ctx = untried.front();
    untried.pop_front();

#if 0 // ndef NDEBUG
    if (ctx)
    {
      av_log(NULL, AV_LOG_INFO,
             "\n\nUSING DECODER: %s\n\n\n",
             ctx->codec->name);
    }
#endif

    return ctx;
  }


  //----------------------------------------------------------------
  // Track::initTraits
  //
  bool
  Track::initTraits()
  {
    YAE_ASSERT(false);
    return true;
  }

  //----------------------------------------------------------------
  // Track::open
  //
  AVCodecContext *
  Track::open()
  {
    if (codecContext_)
    {
      return codecContext_.get();
    }

    if (!stream_)
    {
      return NULL;
    }

    const AVCodecParameters & codecParams = *(stream_->codecpar);

    codecContext_ = find_best_decoder_for(codecParams,
                                          candidates_,
                                          preferSoftwareDecoder_);

    AVCodecContext * ctx = codecContext_.get();
    if (!ctx && stream_->codecpar->codec_id != AV_CODEC_ID_TEXT)
    {
      // unsupported codec:
      return NULL;
    }

    if (ctx)
    {
      ctx->pkt_timebase = stream_->time_base;
    }

    sent_ = 0;
    received_ = 0;
    errors_ = 0;
    return ctx;
  }

  //----------------------------------------------------------------
  // Track::close
  //
  void
  Track::close()
  {
    if (stream_ && codecContext_)
    {
      codecContext_.reset();
    }
  }

  //----------------------------------------------------------------
  // Track::getCodecName
  //
  const char *
  Track::getCodecName() const
  {
    return stream_ ? avcodec_get_name(stream_->codecpar->codec_id) : NULL;
  }

  //----------------------------------------------------------------
  // Track::getName
  //
  const char *
  Track::getName() const
  {
    return stream_ ? getTrackName(stream_->metadata) : NULL;
  }

  //----------------------------------------------------------------
  // Track::getLang
  //
  const char *
  Track::getLang() const
  {
    return stream_ ? getTrackLang(stream_->metadata) : NULL;
  }

  //----------------------------------------------------------------
  // Track::getDuration
  //
  bool
  Track::getDuration(TTime & start, TTime & duration) const
  {
    if (!stream_)
    {
      YAE_ASSERT(false);
      return false;
    }

    bool got_start = false;
    bool got_duration = false;

    if (stream_->start_time != int64_t(AV_NOPTS_VALUE))
    {
      // return track duration:
      start.base_ = stream_->time_base.den;
      start.time_ =
        stream_->start_time != int64_t(AV_NOPTS_VALUE) ?
        stream_->time_base.num * stream_->start_time : 0;
      got_start = true;
    }

    if (stream_->duration != int64_t(AV_NOPTS_VALUE))
    {
      duration.time_ = stream_->time_base.num * stream_->duration;
      duration.base_ = stream_->time_base.den;
      got_duration = true;
    }

    if (got_start && got_duration)
    {
      return true;
    }

    if (!context_)
    {
      YAE_ASSERT(false);
      return false;
    }

    if (!got_start && context_->start_time != int64_t(AV_NOPTS_VALUE))
    {
      // track duration is unknown, return movie duration instead:
      start.base_ = AV_TIME_BASE;
      start.time_ =
        context_->start_time != int64_t(AV_NOPTS_VALUE) ?
        context_->start_time : 0;
      got_start = true;
    }

    if (!got_duration && context_->duration != int64_t(AV_NOPTS_VALUE))
    {
      duration.time_ = context_->duration;
      duration.base_ = AV_TIME_BASE;
      got_duration = true;
    }

    if (got_start && got_duration)
    {
      return true;
    }

    int64 fileSize = avio_size(context_->pb);
    int64 fileBits = fileSize * 8;

    start.base_ = AV_TIME_BASE;
    start.time_ = 0;

    if (context_->bit_rate)
    {
      double t =
        double(fileBits / context_->bit_rate) +
        double(fileBits % context_->bit_rate) /
        double(context_->bit_rate);

      duration.time_ = int64_t(0.5 + t * double(AV_TIME_BASE));
      duration.base_ = AV_TIME_BASE;
      return true;
    }

    const AVCodecParameters & codecParams = *(stream_->codecpar);
    if (context_->nb_streams == 1 && codecParams.bit_rate)
    {
      double t =
        double(fileBits / codecParams.bit_rate) +
        double(fileBits % codecParams.bit_rate) /
        double(codecParams.bit_rate);

      duration.time_ = int64_t(0.5 + t * double(AV_TIME_BASE));
      duration.base_ = AV_TIME_BASE;
      return true;
    }

    // unknown duration:
    duration.time_ = std::numeric_limits<int64>::max();
    duration.base_ = AV_TIME_BASE;
    return false;
  }

  //----------------------------------------------------------------
  // Track::threadStart
  //
  bool
  Track::threadStart()
  {
    terminator_.stopWaiting(false);
    packetQueue_.open();
    return thread_.run();
  }

  //----------------------------------------------------------------
  // Track::decoderPull
  //
  int
  Track::decoderPull(AVCodecContext * ctx)
  {
    int err = 0;
    while (true)
    {
      AvFrm frm;
      AVFrame & decodedFrame = frm.get();
      err = avcodec_receive_frame(ctx, &decodedFrame);
      if (err < 0)
      {
        if (err != AVERROR(EAGAIN) && err != AVERROR_EOF)
        {
          errors_++;
        }

        break;
      }

      // FIXME: perhaps it may be useful to keep track of the number
      // of frames decoded successfully?

      received_++;
      decodedFrame.pts = decodedFrame.best_effort_timestamp;
      handle(frm);
    }

    return err;
  }

  //----------------------------------------------------------------
  // Track::decode
  //
  int
  Track::decode(AVCodecContext * ctx, const AvPkt & pkt)
  {
    int errSend = AVERROR(EAGAIN);
    int errRecv = AVERROR(EAGAIN);

    while (errSend == AVERROR(EAGAIN))
    {
      boost::this_thread::interruption_point();

      const AVPacket & packet = pkt.get();
      errSend = avcodec_send_packet(ctx, &packet);

      if (errSend == AVERROR_EOF)
      {
        avcodec_flush_buffers(ctx);
        errSend = avcodec_send_packet(ctx, &packet);
      }

      if (errSend < 0 && errSend != AVERROR(EAGAIN) && errSend != AVERROR_EOF)
      {
#ifndef NDEBUG
        av_log(NULL, AV_LOG_WARNING,
               "[%s] Track::decode(%p), errSend: %i, %s\n",
               id_.c_str(),
               packet.data,
               errSend,
               av_strerr(errSend).c_str());
#endif
        errors_++;
        return errSend;
      }
      else if (errSend >= 0)
      {
        sent_++;
      }

      errRecv = decoderPull(ctx);
      if (errRecv < 0)
      {
#ifndef NDEBUG
        if (errRecv != AVERROR(EAGAIN) && errRecv != AVERROR_EOF)
        {
          av_log(NULL, AV_LOG_WARNING,
                 "[%s] Track::decode(%p), errRecv: %i, %s\n",
                 id_.c_str(),
                 packet.data,
                 errRecv,
                 av_strerr(errRecv).c_str());
        }
#endif
        break;
      }
    }

    return errRecv;
  }

  //----------------------------------------------------------------
  // Track::decode
  //
  void
  Track::decode(const TPacketPtr & packetPtr)
  {
    if (!packetPtr)
    {
      this->flush();
      return;
    }

    const AvPkt & pkt = *packetPtr;
    const AVPacket & packet = pkt.get();

    if (switchDecoderToRecommended_ && packet.flags & AV_PKT_FLAG_KEY)
    {
      candidates_.clear();
      candidates_.splice(candidates_.end(), recommended_);
      switchDecoderToRecommended_ = false;

      // flush any buffered frames:
      this->flush();

      // close the codec:
      codecContext_.reset();
    }

    AVCodecContext * ctx = this->open();
    if (!ctx)
    {
      // codec is not supported
      return;
    }

    if (sent_ > 60)
    {
      packets_.pop_front();
    }
    packets_.push_back(packetPtr);

    uint64_t receivedPrior = received_;
    int err = decode(ctx, pkt);

    if (received_ > receivedPrior)
    {
      packets_.clear();
      sent_ = 0;
      errors_ = 0;
    }
    else if (err < 0 && err != AVERROR(EAGAIN) &&
             (!received_ || errors_ >= 6))
    {
      this->switchDecoder();
    }
  }

  //----------------------------------------------------------------
  // Track::flush
  //
  void
  Track::flush()
  {
    AVCodecContext * ctx = codecContext_.get();
    if (ctx)
    {
      // flush out buffered frames with an empty packet:
      this->decode(ctx, AvPkt());
    }
  }

  //----------------------------------------------------------------
  // Track::switchDecoder
  //
  bool
  Track::switchDecoder()
  {
    // while (!candidates_.empty())
    {
      codecContext_.reset();
      AVCodecContext * ctx = open();

      int err = AVERROR(EAGAIN);
      for (std::list<TPacketPtr>::const_iterator
             i = packets_.begin(), end = packets_.end(); i != end; ++i)
      {
        const AvPkt & pkt = *(*i);
        int err = decode(ctx, pkt);
        if (err < 0 && err != AVERROR(EAGAIN))
        {
          break;
        }
      }

      if (err == 0)
      {
        return true;
      }

      if (err == AVERROR(EAGAIN))
      {
        return false;
      }
    }

    return false;
  }

  //----------------------------------------------------------------
  // Track::tryToSwitchDecoder
  //
  void
  Track::tryToSwitchDecoder(const std::string & name)
  {
    const TDecoders & decoders = get_decoders();
    const AVCodecParameters & params = *(stream_->codecpar);

    std::list<AvCodecContextPtr> candidates;
    decoders.find(params, candidates, preferSoftwareDecoder_);

    std::list<AvCodecContextPtr> a;
    std::list<AvCodecContextPtr> b;

    for (std::list<AvCodecContextPtr>::const_iterator i = candidates.begin();
         i != candidates.end(); ++i)
    {
      const AvCodecContextPtr & c = *i;
      const AVCodecContext * ctx = c.get();

      if (name == ctx->codec->name)
      {
        a.push_back(c);
      }
      else
      {
        b.push_back(c);
      }
    }

    a.splice(a.end(), b);

    recommended_ = a;
    switchDecoderToRecommended_ = true;
  }

  //----------------------------------------------------------------
  // VideoTrack::threadLoop
  //
  void
  Track::threadLoop()
  {
    decoderStartup();

    while (true)
    {
      try
      {
        boost::this_thread::interruption_point();

        TPacketPtr packetPtr;
        if (!packetQueue_.pop(packetPtr, &terminator_))
        {
          break;
        }

        decode(packetPtr);
      }
      catch (...)
      {
        break;
      }
    }

    decoderShutdown();
  }

  //----------------------------------------------------------------
  // Track::threadStop
  //
  bool
  Track::threadStop()
  {
    terminator_.stopWaiting(true);
    packetQueue_.close();
    thread_.stop();
    return thread_.wait();
  }

  //----------------------------------------------------------------
  // Track::setTempo
  //
  bool
  Track::setTempo(double tempo)
  {
    boost::lock_guard<boost::mutex> lock(tempoMutex_);
    tempo_ = tempo;
    return true;
  }


  //----------------------------------------------------------------
  // same_codec
  //
  bool
  same_codec(const TrackPtr & a, const TrackPtr & b)
  {
    if (a == b)
    {
      return true;
    }

    if (!(a && b))
    {
      return false;
    }

    const AVStream & sa = a->stream();
    const AVStream & sb = b->stream();
    bool same = (sa.codecpar->codec_id == sb.codecpar->codec_id);

    return same;
  }
}
