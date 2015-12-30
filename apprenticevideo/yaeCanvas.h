// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created   : Sun Feb 13 21:37:20 MST 2011
// Copyright : Pavel Koshevoy
// License   : MIT -- http://www.opensource.org/licenses/mit-license.php

#ifndef YAE_CANVAS_H_
#define YAE_CANVAS_H_

// boost includes:
#ifndef Q_MOC_RUN
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#endif

// Qt includes:
#include <QEvent>
#include <QObject>
#include <QString>

// yae includes:
#include "yae/video/yae_video.h"
#include "yae/video/yae_auto_crop.h"
#include "yae/video/yae_video_canvas.h"
#include "yae/video/yae_synchronous.h"
#include "yae/thread/yae_threading.h"

// local includes:
#include "yaeCanvasRenderer.h"
#include "yaeThumbnailProvider.h"


namespace yae
{
  // forward declarations:
  class TLibass;
  struct Canvas;

  //----------------------------------------------------------------
  // BufferedEvent
  //
  template <int EventId>
  struct BufferedEvent : public QEvent
  {
    //----------------------------------------------------------------
    // kId
    //
    enum { kId = EventId };

    //----------------------------------------------------------------
    // TPayload
    //
    struct TPayload
    {
      TPayload():
        delivered_(true)
      {}

      bool setDelivered(bool delivered)
      {
        boost::lock_guard<boost::mutex> lock(mutex_);
        bool postThePayload = delivered_;
        delivered_ = delivered;
        return postThePayload;
      }

    private:
      mutable boost::mutex mutex_;
      bool delivered_;
    };

    BufferedEvent(TPayload & payload):
      QEvent(QEvent::User),
      payload_(payload)
    {}

    TPayload & payload_;
  };


  //----------------------------------------------------------------
  // TFontAttachment
  //
  struct YAE_API TFontAttachment
  {
    TFontAttachment(const char * filename = NULL,
                    const unsigned char * data = NULL,
                    std::size_t size = 0);

    const char * filename_;
    const unsigned char * data_;
    std::size_t size_;
  };

  //----------------------------------------------------------------
  // Canvas
  //
  struct YAE_API Canvas : public IVideoCanvas
  {
    //----------------------------------------------------------------
    // IDelegate
    //
    struct YAE_API IDelegate
    {
      virtual ~IDelegate() {}

      virtual bool isVisible() = 0;
      virtual void repaint() = 0;
      virtual void requestRepaint() = 0;
      virtual void inhibitScreenSaver() = 0;
    };

    //----------------------------------------------------------------
    // ILayer
    //
    struct YAE_API ILayer
    {
      ILayer(): enabled_(true) {}
      virtual ~ILayer() {}

      inline void setContext(const boost::shared_ptr<IOpenGLContext> & context)
      { context_ = context; }

      inline const boost::shared_ptr<IOpenGLContext> & context() const
      { return context_; }

      inline void setDelegate(const boost::shared_ptr<IDelegate> & delegate)
      { delegate_ = delegate; }

      inline const boost::shared_ptr<IDelegate> & delegate() const
      { return delegate_; }

      inline bool isEnabled() const
      { return enabled_; }

      virtual void setEnabled(bool enable)
      {
        enabled_ = enable;

        if (delegate_)
        {
          delegate_->requestRepaint();
        }
      }

      virtual void requestRepaint() = 0;
      virtual void resizeTo(const Canvas * canvas) = 0;
      virtual void paint(Canvas * canvas) = 0;
      virtual bool processEvent(Canvas * canvas, QEvent * event) = 0;

      // lookup image provider for a given resource URL:
      virtual TImageProviderPtr
      getImageProvider(const QString & imageUrl, QString & imageId) const = 0;

    protected:
      boost::shared_ptr<IOpenGLContext> context_;
      boost::shared_ptr<IDelegate> delegate_;
      bool enabled_;
    };

    Canvas(const boost::shared_ptr<IOpenGLContext> & context);
    ~Canvas();

    inline int canvasWidth() const
    { return w_; }

    inline int canvasHeight() const
    { return h_; }

    inline IOpenGLContext & context()
    { return *context_; }

    void setDelegate(const boost::shared_ptr<IDelegate> & delegate);

    inline const boost::shared_ptr<IDelegate> & delegate() const
    { return delegate_; }

    // initialize private backend rendering object,
    // should not be called prior to initializing GLEW:
    void initializePrivateBackend();

    // generic mechanism for delegating canvas painting and event processing:
    // NOTE: 1. the last appended layer is front-most
    //       2. painting -- back-to-front, for all layers
    //       3. event handling -- front-to-back, until handled
    void append(ILayer * layer);

    // lookup a fragment shader for a given pixel format, if one exits:
    const TFragmentShader * fragmentShaderFor(const VideoTraits & vtts) const;

    // specify reader ID tag so that the Canvas can discard
    // frames originating from any other reader:
    void acceptFramesWithReaderId(unsigned int readerId);

    // this is called to add custom fonts that may have been
    // embedded in the video file:
    void libassAddFont(const char * filename,
                       const unsigned char * data,
                       const std::size_t size);

    // discard currently stored image data, repaint the canvas:
    void clear();
    void clearOverlay();

    // helper:
    void refresh();

    // NOTE: thread safe, will post a PaintCanvasEvent if there isn't one
    // already posted-but-not-delivered (to avoid flooding the event queue):
    void requestRepaint();

    // virtual: this will be called from a secondary thread:
    bool render(const TVideoFramePtr & frame);

    // helpers:
    bool loadFrame(const TVideoFramePtr & frame);
    TVideoFramePtr currentFrame() const;

    // helpers:
    void setSubs(const std::list<TSubsFrame> & subs);
    bool updateOverlay(bool reparse);

    // helpers:
    void setGreeting(const QString & greeting);
    bool updateGreeting();

    // accessor to the current greeting message:
    inline const QString & greeting() const
    { return greeting_; }

    // if enabled, skip using a fragment shader (even if one is available)
    // for non-native pixel formats:
    void skipColorConverter(bool enable);

    // NOTE: In order to avoid blurring interlaced frames vertical scaling
    // is disabled by default.  However, if the video is not interlaced
    // and display aspect ratio is less than encoded frame aspect ratio
    // scaling down frame width would result in loss of information,
    // therefore scaling up frame height would be preferred.
    //
    // use this to enable/disable frame height scaling:
    void enableVerticalScaling(bool enable);

    // use this to override auto-detected aspect ratio:
    void overrideDisplayAspectRatio(double dar);

    // use this to crop letterbox pillars and bars:
    void cropFrame(double darCropped);

    // use this to zoom/crop a portion of the frame
    // to eliminate letterbox pillars and/or bars;
    void cropFrame(const TCropFrame & crop);

    // start crop frame detection thread and deliver the results
    // asynchronously via a callback:
    void cropAutoDetect(void * callbackContext, TAutoCropCallback callback);
    void cropAutoDetectStop();

    // accessors to full resolution frame dimensions
    // after overriding display aspect ratio and cropping:
    double imageWidth() const;
    double imageHeight() const;

    // return width/height image aspect ration
    // and pass back image width and height
    //
    // NOTE: width and height are preprocessed according to current
    // frame crop and aspect ratio settings.
    //
    double imageAspectRatio(double & w, double & h) const;

    enum TRenderMode
    {
      // this will result in letterbox bars or pillars rendering, the
      // image will be scaled up or down to fit within canvas bounding box:
      kScaleToFit = 0,

      // this will avoid letterbox bars and pillars rendering, the
      // image will be scaled up and cropped by the canvas bounding box:
      kCropToFill = 1
    };

    // crop-to-fill may be useful in a full screen canvas:
    void setRenderMode(TRenderMode renderMode);

    // this will be called from a helper thread
    // once it is done updating fontconfig cache for libass:
    static void libassInitDoneCallback(void * canvas, TLibass * libass);

    TLibass * asyncInitLibass(const unsigned char * header = NULL,
                              const std::size_t headerSize = 0);

    // helper:
    void resize(int w, int h);
    void paintCanvas();

    //----------------------------------------------------------------
    // PaintCanvasEvent
    //
    enum { kPaintCanvasEvent };
    typedef BufferedEvent<kPaintCanvasEvent> PaintCanvasEvent;

    //----------------------------------------------------------------
    // RenderFrameEvent
    //
    struct RenderFrameEvent : public QEvent
    {
      //----------------------------------------------------------------
      // TPayload
      //
      struct TPayload
      {
        TPayload():
          expectedReaderId_((unsigned int)~0)
        {}

        bool set(const TVideoFramePtr & frame)
        {
          boost::lock_guard<boost::mutex> lock(mutex_);
          bool postThePayload = !frame_;
          frame_ = frame;
          return postThePayload;
        }

        void get(TVideoFramePtr & frame)
        {
          boost::lock_guard<boost::mutex> lock(mutex_);
          if (frame_ && frame_->readerId_ == expectedReaderId_)
          {
            frame = frame_;
          }
          frame_ = TVideoFramePtr();
        }

        void setExpectedReaderId(unsigned int readerId)
        {
          boost::lock_guard<boost::mutex> lock(mutex_);
          expectedReaderId_ = readerId;
        }

      private:
        mutable boost::mutex mutex_;
        TVideoFramePtr frame_;

        // frames with matching reader ID tag will be rendered,
        // mismatching frames will be ignored:
        unsigned int expectedReaderId_;
      };

      RenderFrameEvent(TPayload & payload):
        QEvent(QEvent::User),
        payload_(payload)
      {}

      TPayload & payload_;
    };

  protected:
    // NOTE: events will be delivered on the main thread:
    bool processEvent(QEvent * event);

    //----------------------------------------------------------------
    // TEventReceiver
    //
    struct TEventReceiver : public QObject
    {
      TEventReceiver(Canvas & canvas):
        canvas_(canvas)
      {}

      // virtual:
      bool event(QEvent * event)
      {
        if (canvas_.processEvent(event))
        {
          return true;
        }

        return QObject::event(event);
      }

      Canvas & canvas_;
    };

    friend struct TEventReceiver;
    TEventReceiver eventReceiver_;

    boost::shared_ptr<IOpenGLContext> context_;
    boost::shared_ptr<IDelegate> delegate_;
    PaintCanvasEvent::TPayload paintCanvasEvent_;
    RenderFrameEvent::TPayload renderFrameEvent_;
    CanvasRenderer * private_;
    CanvasRenderer * overlay_;
    TLibass * libass_;
    bool showTheGreeting_;
    bool subsInOverlay_;
    TRenderMode renderMode_;

    // canvas size:
    int w_;
    int h_;

    // keep track of previously displayed subtitles
    // in order to avoid re-rendering the same subtitles with every frame:
    std::list<TSubsFrame> subs_;

    // the greeting message shown to the user
    QString greeting_;

    // automatic frame margin detection:
    TAutoCropDetect autoCrop_;
    Thread<TAutoCropDetect> autoCropThread_;

    // extra fonts embedded in the video file, will be passed along to libass:
    std::list<TFontAttachment> customFonts_;

    // a list of painting and event handling delegates,
    // traversed front-to-back for painting
    // and back-to-front for event processing:
    std::list<ILayer *> layers_;
  };
}


#endif // YAE_CANVAS_H_
