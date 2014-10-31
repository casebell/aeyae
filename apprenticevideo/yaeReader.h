// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created   : Wed May 26 22:17:43 MDT 2010
// Copyright : Pavel Koshevoy
// License   : MIT -- http://www.opensource.org/licenses/mit-license.php

#ifndef YAE_READER_H_
#define YAE_READER_H_

// system includes:
#include <string>
#include <list>

// yae includes:
#include <yaeAPI.h>
#include <yaeQueue.h>


namespace yae
{

  //----------------------------------------------------------------
  // IReader
  //
  struct YAE_API IReader
  {
  protected:
    IReader() {}
    virtual ~IReader() {}

  public:

    //! The de/structor is intentionally hidden, use destroy() method instead.
    //! This is necessary in order to avoid conflicting memory manager
    //! problems that arise on windows when various libs are linked to
    //! different versions of runtime library.  Each library uses its own
    //! memory manager, so allocating in one library call and deallocating
    //! in another library will not work.  This can be avoided by hiding
    //! the standard constructor/destructor and providing an explicit
    //! interface for de/allocating an object instance, thus ensuring that
    //! the same memory manager will perform de/allocation.
    virtual void destroy() = 0;

    //! return a human readable name for this reader (preferably unique):
    virtual const char * getName() const = 0;

    //! assemble a list of supported URL protocols:
    virtual bool getUrlProtocols(std::list<std::string> & protocols) const = 0;

    //! open a resource specified by the resourcePath such as filepath or URL:
    virtual bool open(const char * resourcePathUTF8) = 0;

    //! close currently open resource:
    virtual void close() = 0;

    virtual std::size_t getNumberOfVideoTracks() const = 0;
    virtual std::size_t getNumberOfAudioTracks() const = 0;

    virtual std::size_t getSelectedVideoTrackIndex() const = 0;
    virtual std::size_t getSelectedAudioTrackIndex() const = 0;

    virtual bool selectVideoTrack(std::size_t i) = 0;
    virtual bool selectAudioTrack(std::size_t i) = 0;

    virtual void getSelectedVideoTrackInfo(TTrackInfo & info) const = 0;
    virtual void getSelectedAudioTrackInfo(TTrackInfo & info) const = 0;

    virtual bool getVideoDuration(TTime & start, TTime & duration) const = 0;
    virtual bool getAudioDuration(TTime & start, TTime & duration) const = 0;

    virtual bool getAudioTraits(AudioTraits & traits) const = 0;
    virtual bool getVideoTraits(VideoTraits & traits) const = 0;

    //! force decoded audio/video frames to be in a particular format:
    virtual bool setAudioTraitsOverride(const AudioTraits & override) = 0;
    virtual bool setVideoTraitsOverride(const VideoTraits & override) = 0;

    virtual bool getAudioTraitsOverride(AudioTraits & override) const = 0;
    virtual bool getVideoTraitsOverride(VideoTraits & override) const = 0;

    //! check whether it is possible to set the current position:
    virtual bool isSeekable() const = 0;

    //! set current position to a given value (or an earlier value nearby):
    virtual bool seek(double t) = 0;

    //! By default readAudio and readVideo will block indefinitely
    //! until a frame arrives or the frame queue is closed.
    //! Use QueueWaitMgr if you need to break out of the waiting
    //! loop for some reason (such as in order to avoid a deadlock).
    virtual bool readVideo(TVideoFramePtr & frame, QueueWaitMgr * mgr) = 0;
    virtual bool readAudio(TAudioFramePtr & frame, QueueWaitMgr * mgr) = 0;

    virtual bool threadStart() = 0;
    virtual bool threadStop() = 0;

    virtual void getPlaybackInterval(double & tIn, double & tOut) const = 0;
    virtual void setPlaybackIntervalStart(double timeIn) = 0;
    virtual void setPlaybackIntervalEnd(double timeOut) = 0;
    virtual void setPlaybackEnabled(bool enabled) = 0;
    virtual void setPlaybackLooping(bool enabled) = 0;

    // these are used to speed up video decoding:
    virtual void skipLoopFilter(bool skip) = 0;
    virtual void skipNonReferenceFrames(bool skip) = 0;

    // this can be used to slow down audio if video decoding is too slow,
    // or it can be used to speed up audio to watch the movie faster:
    virtual bool setTempo(double tempo) = 0;

    // enable/disable video deinterlacing:
    virtual bool setDeinterlacing(bool enabled) = 0;

    // query subtitles, enable/disable rendering...
    virtual std::size_t subsCount() const = 0;
    virtual TSubsFormat subsInfo(std::size_t i, TTrackInfo & info) const = 0;
    virtual void setSubsRender(std::size_t i, bool render) = 0;
    virtual bool getSubsRender(std::size_t i) const = 0;

    // chapter navigation:
    virtual std::size_t countChapters() const = 0;
    virtual bool getChapterInfo(std::size_t i, TChapter & c) const = 0;

    // attachments (fonts, thumbnails, etc...)
    virtual std::size_t getNumberOfAttachments() const = 0;
    virtual const TAttachment * getAttachmentInfo(std::size_t i) const = 0;

    // frames produced by this reader will be tagged with a reader ID,
    // so that renderers can disambiguate between frames produced
    // by different readers:
    virtual void setReaderId(unsigned int readerId) = 0;
  };

}


#endif // YAE_READER_H_
