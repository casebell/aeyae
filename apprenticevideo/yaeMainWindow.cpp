// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created      : Sat Dec 18 17:55:21 MST 2010
// Copyright    : Pavel Koshevoy
// License      : MIT -- http://www.opensource.org/licenses/mit-license.php

// system includes:
#include <iostream>
#include <sstream>
#include <assert.h>

// Qt includes:
#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMimeData>
#include <QUrl>

// yae includes:
#include <yaeReaderFFMPEG.h>
#include <yaePixelFormats.h>
#include <yaePixelFormatTraits.h>
#include <yaeAudioRendererPortaudio.h>
#include <yaeVideoRenderer.h>

// local includes:
#include <yaeMainWindow.h>


namespace yae
{

  //----------------------------------------------------------------
  // MainWindow::MainWindow
  // 
  MainWindow::MainWindow():
    QMainWindow(NULL, 0),
    reader_(NULL),
    canvas_(NULL),
    audioRenderer_(NULL),
    videoRenderer_(NULL)
  {
    setupUi(this);
    setAcceptDrops(true);

    // request vsync if available:
    QGLFormat contextFormat;
    contextFormat.setSwapInterval(1);
  
    canvas_ = new Canvas(contextFormat);
    reader_ = ReaderFFMPEG::create();
    
    audioRenderer_ = AudioRendererPortaudio::create();
    videoRenderer_ = VideoRenderer::create();
      
    delete centralwidget->layout();
    QVBoxLayout * layout = new QVBoxLayout(centralwidget);
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(canvas_);
    
    bool ok = true;
    ok = connect(actionOpen, SIGNAL(triggered()),
                 this, SLOT(fileOpen()));
    assert(ok);
    
    ok = connect(actionExit, SIGNAL(triggered()),
                 this, SLOT(fileExit()));
    assert(ok);
  }

  //----------------------------------------------------------------
  // MainWindow::~MainWindow
  // 
  MainWindow::~MainWindow()
  {
    audioRenderer_->close();
    audioRenderer_->destroy();
    
    videoRenderer_->close();
    videoRenderer_->destroy();
    
    reader_->destroy();
    delete canvas_;
  }

  //----------------------------------------------------------------
  // MainWindow::canvas
  // 
  Canvas *
  MainWindow::canvas() const
  {
    return canvas_;
  }
  
  //----------------------------------------------------------------
  // MainWindow::load
  // 
  bool
  MainWindow::load(const QString & path)
  {
    std::ostringstream os;
    os << fileUtf8::kProtocolName << "://" << path.toUtf8().constData();
    
    std::string url(os.str());
    
    ReaderFFMPEG * reader = ReaderFFMPEG::create();
    if (!reader->open(url.c_str()))
    {
      std::cerr << "ERROR: could not open movie: " << url << std::endl;
      return false;
    }
    
    std::size_t numVideoTracks = reader->getNumberOfVideoTracks();
    std::size_t numAudioTracks = reader->getNumberOfAudioTracks();
    
    reader->threadStop();
    
    if (numVideoTracks)
    {
      reader->selectVideoTrack(0);
      VideoTraits vtts;
      if (reader->getVideoTraits(vtts))
      {
        // pixel format shortcut:
        const pixelFormat::Traits * ptts =
          pixelFormat::getTraits(vtts.pixelFormat_);

        std::cout << "yae: native format: ";
        if (ptts)
        {
          std::cout << ptts->name_;
        }
        else
        {
          std::cout << "unsupported" << std::endl;
        }
        std::cout << std::endl;
        
#if 0
        bool unsupported = ptts == NULL;

        if (!unsupported)
        {
          unsupported = (ptts->flags_ & pixelFormat::kPaletted) != 0;
        }

        if (!unsupported)
        {
          GLint internalFormatGL;
          GLenum pixelFormatGL;
          GLenum dataTypeGL;
          unsigned int supportedChannels = yae_to_opengl(vtts.pixelFormat_,
                                                         internalFormatGL,
                                                         pixelFormatGL,
                                                         dataTypeGL);
          unsupported = (supportedChannels != ptts->channels_);
        }
        
        if (unsupported)
        {
          vtts.pixelFormat_ = kPixelFormatGRAY8;
          
          if (ptts)
          {
            if ((ptts->flags_ & pixelFormat::kAlpha) &&
                (ptts->flags_ & pixelFormat::kColor))
            {
              vtts.pixelFormat_ = kPixelFormatBGRA;
            }
            else if ((ptts->flags_ & pixelFormat::kColor) ||
                     (ptts->flags_ & pixelFormat::kPaletted))
            {
              vtts.pixelFormat_ = kPixelFormatBGR24;
            }
          }
          
          reader->setVideoTraitsOverride(vtts);
        }
#elif 0
        vtts.pixelFormat_ = kPixelFormatY400A;
        reader->setVideoTraitsOverride(vtts);
#endif
      }
    }
    
    if (numAudioTracks)
    {
      reader->selectAudioTrack(0);
#if 0
      // FIXME: just testing:
      AudioTraits atts;
      if (reader->getAudioTraits(atts))
      {
        atts.channelLayout_ = kAudioStereo;
        reader->setVideoTraitsOverride(atts);
      }
#endif
    }
    
    reader->threadStart();
    
    // setup renderer shared reference clock:
    videoRenderer_->close();
    audioRenderer_->close();
    
    if (numAudioTracks)
    {
      audioRenderer_->takeThisClock(SharedClock());
      audioRenderer_->obeyThisClock(audioRenderer_->clock());
      
      if (numVideoTracks)
      {
        videoRenderer_->obeyThisClock(audioRenderer_->clock());
      }
    }
    else if (numVideoTracks)
    {
      videoRenderer_->takeThisClock(SharedClock());
      videoRenderer_->obeyThisClock(videoRenderer_->clock());
    }
    
    // update the renderers:
    reader_->close();
    audioRenderer_->open(audioRenderer_->getDefaultDeviceIndex(), reader);
    videoRenderer_->open(canvas_, reader);
    
    // replace the previous reader:
    reader_->destroy();
    reader_ = reader;
    
    return true;
  }
  
  //----------------------------------------------------------------
  // MainWindow::fileOpen
  // 
  void
  MainWindow::fileOpen()
  {
    QString filename =
      QFileDialog::getOpenFileName(this,
				   "Open file",
				   QString(),
				   "movies ("
                                   "*.avi "
                                   "*.asf "
                                   "*.divx "
                                   "*.flv "
                                   "*.f4v "
                                   "*.m2t "
                                   "*.m2ts "
                                   "*.m4v "
                                   "*.mkv "
                                   "*.mod "
                                   "*.mov "
                                   "*.mpg "
                                   "*.mp4 "
                                   "*.mpeg "
                                   "*.mpts "
                                   "*.ogm "
                                   "*.ogv "
                                   "*.ts "
                                   "*.wmv "
                                   "*.webm "
                                   ")");
    load(filename);
  }
  
  //----------------------------------------------------------------
  // MainWindow::fileExit
  // 
  void
  MainWindow::fileExit()
  {
    reader_->close();
    MainWindow::close();
    qApp->quit();
  }
  
  //----------------------------------------------------------------
  // MainWindow::closeEvent
  // 
  void
  MainWindow::closeEvent(QCloseEvent * e)
  {
    e->accept();
    fileExit();
  }
  
  //----------------------------------------------------------------
  // MainWindow::dragEnterEvent
  // 
  void
  MainWindow::dragEnterEvent(QDragEnterEvent * e)
  {
    if (!e->mimeData()->hasUrls())
    {
      e->ignore();
      return;
    }
    
    e->acceptProposedAction();
  }
  
  //----------------------------------------------------------------
  // MainWindow::dropEvent
  // 
  void
  MainWindow::dropEvent(QDropEvent * e)
  {
    if (!e->mimeData()->hasUrls())
    {
      e->ignore();
      return;
    }
    
    e->acceptProposedAction();
    
    QString filename = e->mimeData()->urls().front().toLocalFile();
    load(filename);
  }
  
};
