// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created      : Sat Apr  7 23:41:07 MDT 2012
// Copyright    : Pavel Koshevoy
// License      : MIT -- http://www.opensource.org/licenses/mit-license.php

// system includes:
#include <iostream>
#include <algorithm>

// Qt includes:
#include <QCursor>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>

// yae includes:
#include <yaePlaylistWidget.h>
#include <yaeUtils.h>


namespace yae
{
  
  //----------------------------------------------------------------
  // kGroupNameHeight
  // 
  static const int kGroupNameHeight = 24;
  
  //----------------------------------------------------------------
  // kGroupItemHeight
  // 
  static const int kGroupItemHeight = 20;

  //----------------------------------------------------------------
  // kPlayPauseIconWidth
  // 
  static const int kPlayPauseIconWidth = 14;
  
  //----------------------------------------------------------------
  // shortenTextToFit
  // 
  static bool
  shortenTextToFit(QPainter & painter,
                   const QRect & bbox,
                   int textAlignment,
                   const QString & text,
                   QString & textLeft,
                   QString & textRight)
  {
    static const QString ellipsis("...");
    
    // in case style sheet is used, get fontmetrics from painter:
    QFontMetrics fm = painter.fontMetrics();
    
    const int bboxWidth = bbox.width();
    
    textLeft.clear();
    textRight.clear();
    
    QSize sz = fm.size(Qt::TextSingleLine, text);
    int textWidth = sz.width();
    if (textWidth <= bboxWidth || bboxWidth <= 0)
    {
      // text fits, nothing to do:
      if (textAlignment & Qt::AlignLeft)
      {
        textLeft = text;
      }
      else
      {
        textRight = text;
      }
      
      return false;
    }
    
    // scale back the estimate to avoid cutting out too much of text,
    // because not all characters have the same width:
    const double stepScale = 0.78;
    const int textLen = text.size();
    
    int numToRemove = 0;
    int currLen = textLen - numToRemove;
    int aLen = currLen / 2;
    int bLen = currLen - aLen;
    
    while (currLen > 1)
    {
      // estimate (conservatively) how much text to remove:
      double excess = double(textWidth) / double(bboxWidth) - 1.0;
      if (excess <= 0.0)
      {
        break;
      }
      
      double excessLen =
        std::max<double>(1.0, 
                         stepScale * double(currLen) * 
                         excess / (excess + 1.0));
      
      numToRemove += int(excessLen);
      currLen = textLen - numToRemove;
      
      aLen = currLen / 2;
      bLen = currLen - aLen;
      QString tmp = text.left(aLen) + ellipsis + text.right(bLen);
      
      sz = fm.size(Qt::TextSingleLine, tmp);
      textWidth = sz.width();
    }
    
    if (currLen < 2)
    {
      // too short, give up:
      aLen = 0;
      bLen = 0;
    }
    
    if (textAlignment & Qt::AlignLeft)
    {
      textLeft = text.left(aLen) + ellipsis;
      textRight = text.right(bLen);
    }
    else
    {
      textLeft = text.left(aLen);
      textRight = ellipsis + text.right(bLen);
    }
    
    return true;
  }

  //----------------------------------------------------------------
  // drawTextToFit
  // 
  static void
  drawTextToFit(QPainter & painter,
                const QRect & bbox,
                int textAlignment,
                const QString & text,
                QRect * bboxText = NULL)
  {
    QString textLeft;
    QString textRight;
    
    if (!shortenTextToFit(painter,
                          bbox,
                          textAlignment,
                          text,
                          textLeft,
                          textRight))
    {
      // text fits:
      painter.drawText(bbox, textAlignment, text, bboxText);
      return;
    }
    
    // one part will have ... added to it
    int vertAlignment = textAlignment & Qt::AlignVertical_Mask;
    
    QRect bboxLeft;
    painter.drawText(bbox,
                     vertAlignment | Qt::AlignLeft,
                     textLeft,
                     &bboxLeft);
    
    QRect bboxRight;
    painter.drawText(bbox,
                     vertAlignment | Qt::AlignRight,
                     textRight,
                     &bboxRight);
    
    if (bboxText)
    {
      *bboxText = bboxRight;
      *bboxText |= bboxLeft;
    }
  }
  
  //----------------------------------------------------------------
  // PlaylistItem::PlaylistItem
  // 
  PlaylistItem::PlaylistItem():
    selected_(false)
  {}
  
  //----------------------------------------------------------------
  // PlaylistWidget::PlaylistWidget
  // 
  PlaylistWidget::PlaylistWidget(QWidget * parent, Qt::WindowFlags):
    QAbstractScrollArea(parent),
    rubberBand_(QRubberBand::Rectangle, this),
    playing_(0),
    current_(0)
  {}
  
  //----------------------------------------------------------------
  // PlaylistWidget::setPlaylist
  // 
  void
  PlaylistWidget::setPlaylist(const std::list<QString> & playlist)
  {
    for (std::list<QString>::const_iterator i = playlist.begin();
         i != playlist.end(); ++i)
    {
      QString path = *i;
      
      QFileInfo fi(path);
      if (fi.exists())
      {
        path = fi.absoluteFilePath();
      }
      
      // tokenize it, convert into a tree key path:
      std::list<QString> keys;
      while (true)
      {
        QString key = fi.fileName();
        if (key.isEmpty())
        {
          break;
        }
        
        keys.push_front(key);
        
        QString dir = fi.path();
        fi = QFileInfo(dir);
      }
      
      tree_.set(keys, path);
    }
    
    // flatten the tree into a list of play groups:
    typedef TPlaylistTree::FringeGroup TFringeGroup;
    std::list<TFringeGroup> fringeGroups;
    tree_.get(fringeGroups);
    groups_.clear();
    
    for (std::list<TFringeGroup>::const_iterator i = fringeGroups.begin();
         i != fringeGroups.end(); ++i)
    {
      // shortcut:
      const TFringeGroup & fringeGroup = *i;
      
      groups_.push_back(PlaylistGroup());
      PlaylistGroup & group = groups_.back();
      group.keyPath_ = fringeGroup.fullPath_;
      group.name_ = toWords(fringeGroup.abbreviatedPath_);
      
      // shortcuts:
      typedef std::map<QString, QString> TSiblings;
      const TSiblings & siblings = fringeGroup.siblings_;
      
      for (TSiblings::const_iterator j = siblings.begin();
           j != siblings.end(); ++j)
      {
        const QString & key = j->first;
        const QString & value = j->second;
        
        group.items_.push_back(PlaylistItem());
        PlaylistItem & playlistItem = group.items_.back();
        
        playlistItem.path_ = value;
        
        QFileInfo fi(key);
        playlistItem.name_ = toWords(fi.baseName());
        playlistItem.ext_ = fi.completeSuffix();
      }
    }
    
    updateGeometries();
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::currentGroup
  // 
  const PlaylistGroup *
  PlaylistWidget::currentGroup() const
  {
    return NULL;
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::skipToNext
  // 
  void
  PlaylistWidget::skipToNext()
  {}
  
  //----------------------------------------------------------------
  // PlaylistWidget::backToPrev
  // 
  void
  PlaylistWidget::backToPrev()
  {}
  
  //----------------------------------------------------------------
  // PlaylistWidget::event
  // 
  bool
  PlaylistWidget::event(QEvent * e)
  {
    return QAbstractScrollArea::event(e);
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::paintEvent
  // 
  void
  PlaylistWidget::paintEvent(QPaintEvent * e)
  {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPalette palette = this->palette();
    QBrush background = palette.base();
    painter.fillRect(e->rect(), background);
    
    QPoint viewOffset = getViewOffset();
    QRect localRegion = e->rect().translated(viewOffset);
    painter.translate(-viewOffset);
    
    draw(painter, localRegion);
    
    painter.end();
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::mousePressEvent
  // 
  void
  PlaylistWidget::mousePressEvent(QMouseEvent * e)
  {
    if (e->buttons() & Qt::LeftButton)
    {
      QPoint viewOffset = getViewOffset();
      anchor_ = e->pos() + viewOffset;
      
      rubberBand_.setGeometry(QRect(anchor_ - viewOffset, QSize()));
      rubberBand_.show();
      
      updateSelection(e->pos());
    }
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::mouseReleaseEvent
  // 
  void
  PlaylistWidget::mouseReleaseEvent(QMouseEvent * e)
  {
    rubberBand_.hide();
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::mouseMoveEvent
  // 
  void
  PlaylistWidget::mouseMoveEvent(QMouseEvent * e)
  {
    if (e->buttons() & Qt::LeftButton)
    {
      updateSelection(e->pos(), true);
    }
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::mouseDoubleClickEvent
  // 
  void
  PlaylistWidget::mouseDoubleClickEvent(QMouseEvent * e)
  {
    QAbstractScrollArea::mouseDoubleClickEvent(e);
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::wheelEvent
  // 
  void
  PlaylistWidget::wheelEvent(QWheelEvent * e)
  {
    QScrollBar * sb = verticalScrollBar();
    int val = sb->value();
    int min = sb->minimum();
    int max = sb->maximum();
    int delta = -(e->delta());
    
    if (val == min && delta < 0 ||
        val == max && delta > 0)
    {
      // prevent wheel event from propagating to the parent widget:
      e->accept();
      return;
    }
    
    QAbstractScrollArea::wheelEvent(e);
    
    if (e->buttons() & Qt::LeftButton)
    {
      updateSelection(e->pos());
    }
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::keyPressEvent
  // 
  void
  PlaylistWidget::keyPressEvent(QKeyEvent * e)
  {
    QAbstractScrollArea::keyPressEvent(e);
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::resizeEvent
  // 
  void
  PlaylistWidget::resizeEvent(QResizeEvent * e)
  {
    (void) e;
    updateGeometries();
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::updateGeometries
  // 
  void
  PlaylistWidget::updateGeometries()
  {
    int offset = 0;
    int width = viewport()->width();
    
    for (std::vector<PlaylistGroup>::iterator i = groups_.begin();
         i != groups_.end(); ++i)
    {
      PlaylistGroup & group = *i;
      group.bbox_.setX(0);
      group.bbox_.setY(offset);
      group.bbox_.setWidth(width);
      group.bbox_.setHeight(kGroupNameHeight);
      offset += kGroupNameHeight;
      
      group.bboxItems_.setX(0);
      group.bboxItems_.setY(offset);
      group.bboxItems_.setWidth(width);
      
      for (std::vector<PlaylistItem>::iterator j = group.items_.begin();
           j != group.items_.end(); ++j)
      {
        PlaylistItem & item = *j;
        item.bbox_.setX(0);
        item.bbox_.setY(offset);
        item.bbox_.setWidth(width);
        item.bbox_.setHeight(kGroupItemHeight);
        offset += kGroupItemHeight;
      }
      
      group.bboxItems_.setHeight(offset - group.bboxItems_.y());
    }
    
    updateScrollBars();
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::updateScrollBars
  // 
  void
  PlaylistWidget::updateScrollBars()
  {
    QSize viewportSize = viewport()->size();
    if (!viewportSize.isValid())
    {
      viewportSize = QSize(0, 0);
    }
    
    int viewportHeight = viewportSize.height();
    int viewportWidth = viewportSize.width();
    
    int contentHeight = 0;
    int contentWidth = 0;

    if (!groups_.empty())
    {
      PlaylistGroup & group = groups_.back();
      contentWidth = group.bbox_.width();
      
      PlaylistItem & item = group.items_.back();
      contentHeight = item.bbox_.y() + item.bbox_.height();
    }
    
    verticalScrollBar()->setSingleStep(kGroupItemHeight);
    verticalScrollBar()->setPageStep(viewportHeight);
    verticalScrollBar()->setRange(0, qMax(0, contentHeight - viewportHeight));
    
    horizontalScrollBar()->setSingleStep(kGroupItemHeight);
    horizontalScrollBar()->setPageStep(viewportSize.width());
    horizontalScrollBar()->setRange(0, qMax(0, contentWidth - viewportWidth));
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::draw
  // 
  void
  PlaylistWidget::draw(QPainter & painter, const QRect & region)
  {
    static QPixmap iconPlay = QPixmap(":/images/iconPlay.png");
    static QPixmap iconPause = QPixmap(":/images/iconPause.png");
    
    static const QColor headerColorBg(0x40, 0x80, 0xff);
    static const QColor activeColorBg(0xff, 0x80, 0x40);
    static const QColor brightColorFg(0xff, 0xff, 0xff);
    static const QColor zebraBg[] = {
      QColor(0, 0, 0, 0),
      QColor(0xf4, 0xf4, 0xf4)
    };
    
    QPalette palette = this->palette();
    
    QColor selectedColorBg = palette.color(QPalette::Highlight);
    QColor selectedColorFg = palette.color(QPalette::HighlightedText);
    QColor foregroundColor = palette.color(QPalette::WindowText);
    QFont textFont = painter.font();
    
    std::size_t index = 0;
    for (std::vector<PlaylistGroup>::iterator i = groups_.begin();
         i != groups_.end(); ++i)
    {
      PlaylistGroup & group = *i;
      
      if (group.bbox_.intersects(region))
      {
        painter.fillRect(group.bbox_, headerColorBg);
        painter.setPen(brightColorFg);
        
        drawTextToFit(painter,
                      group.bbox_,
                      Qt::AlignVCenter | Qt::AlignCenter,
                      group.name_);
      }
      
      for (std::vector<PlaylistItem>::iterator j = group.items_.begin();
           j != group.items_.end(); ++j, index++)
      {
        PlaylistItem & item = *j;
        std::size_t zebraIndex = index % 2;
        
        if (!item.bbox_.intersects(region))
        {
          continue;
        }
        
        QColor bg = zebraBg[zebraIndex];
        QColor fg = foregroundColor;
        const QPixmap * icon = NULL;
        bool underline = false;
        
        if (index == playing_)
        {
          // FIXME: check whether playback is paused:
          icon = &iconPlay;
          // icon = &iconPause;
        }
        
        if (index == current_)
        {
          underline = true;
        }
        
        if (item.selected_)
        {
          bg = selectedColorBg;
          fg = selectedColorFg;
        }
        
        painter.setPen(fg);
        painter.fillRect(item.bbox_, bg);
        QString text = tr("%1, %2").arg(item.name_).arg(item.ext_);
        
        QRect bboxText = item.bbox_;
        if (icon)
        {
          int yoffset = (item.bbox_.height() - icon->height()) / 2;
          QPoint pt(item.bbox_.x(), item.bbox_.y() + yoffset);
          painter.drawPixmap(pt, *icon);
        }
        
        int xoffset = kPlayPauseIconWidth;
        bboxText.setX(item.bbox_.x() + xoffset);
        bboxText.setWidth(item.bbox_.width() - xoffset);
        
        QRect bboxTextOut;
        drawTextToFit(painter,
                      bboxText,
                      Qt::AlignVCenter | Qt::AlignLeft,
                      text,
                      &bboxTextOut);
        
        if (underline)
        {
          QColor fg = zebraBg[1].darker(150);
          QPoint p0 = bboxTextOut.bottomLeft() + QPoint(0, 1);
          QPoint p1 = bboxTextOut.bottomRight() + QPoint(0, 1);
          
          painter.setRenderHint(QPainter::Antialiasing, false);
          painter.setPen(fg);
          painter.drawLine(p0, p1);
          painter.setRenderHint(QPainter::Antialiasing);
        }
      }
    }
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::updateSelection
  // 
  void
  PlaylistWidget::updateSelection(const QPoint & mousePos,
                                  bool scrollToItem)
  {
    QPoint viewOffset = getViewOffset();
    QPoint p1 = mousePos + viewOffset;
    
    rubberBand_.setGeometry(QRect(anchor_ - viewOffset,
                                  p1 - viewOffset).normalized());
    
    QRect bboxSel = QRect(anchor_, p1).normalized();
    selectItems(bboxSel);

    if (!scrollToItem)
    {
      update();
      return;
    }
    
    PlaylistGroup * group = lookupGroup(p1);
    PlaylistItem * item = lookup(group, p1);
    scrollTo(group, item);
    
    QPoint viewOffsetNew = getViewOffset();
    int dy = viewOffsetNew.y() - viewOffset.y();
    int dx = viewOffsetNew.x() - viewOffset.x();
    if (dy)
    {
      // move the cursor:
      QPoint pt = this->mapToGlobal(mousePos);
      pt -= QPoint(dx, dy);
      QCursor::setPos(pt);
    }
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::selectItems
  // 
  void
  PlaylistWidget::selectItems(const QRect & bboxSel)
  {
    for (std::vector<PlaylistGroup>::iterator i = groups_.begin();
         i != groups_.end(); ++i)
    {
      PlaylistGroup & group = *i;
      for (std::vector<PlaylistItem>::iterator j = group.items_.begin();
           j != group.items_.end(); ++j)
      {
        PlaylistItem & item = *j;
        item.selected_ = item.bbox_.intersects(bboxSel);
      }
    }
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::scrollTo
  // 
  void
  PlaylistWidget::scrollTo(const PlaylistGroup * group,
                           const PlaylistItem * item)
  {
    if (!group && !item)
    {
      return;
    }
    
    QPoint viewOffset = getViewOffset();
    QRect area = viewport()->rect().translated(viewOffset);
    QRect rect = item ? item->bbox_ : group->bbox_;
    
    QScrollBar * hsb = horizontalScrollBar();
    QScrollBar * vsb = verticalScrollBar();
    
    if (rect.left() < area.left())
    {
      hsb->setValue(hsb->value() + rect.left() - area.left());
    }
    else if (rect.right() > area.right())
    {
      hsb->setValue(hsb->value() + qMin(rect.right() - area.right(),
                                        rect.left() - area.left()));
    }
    
    if (rect.top() < area.top())
    {
      vsb->setValue(vsb->value() + rect.top() - area.top());
    }
    else if (rect.bottom() > area.bottom())
    {
      vsb->setValue(vsb->value() + qMin(rect.bottom() - area.bottom(),
                                        rect.top() - area.top()));
    }
    
    update();
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::lookupGroup
  // 
  PlaylistGroup *
  PlaylistWidget::lookupGroup(const QPoint & pt)
  {
    for (std::vector<PlaylistGroup>::iterator i = groups_.begin();
         i != groups_.end(); ++i)
    {
      PlaylistGroup & group = *i;
      if (group.bboxItems_.contains(pt) ||
          group.bbox_.contains(pt))
      {
        return &group;
      }
    }
    
    if (!groups_.empty())
    {
      QRect bbox = groups_.front().bbox_;
      if (pt.y() <= bbox.y())
      {
        return &groups_.front();
      }
      
      bbox = groups_.back().bboxItems_;
      if (bbox.y() + bbox.height() < pt.y())
      {
        return &groups_.back();
      }
    }
    
    return NULL;
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::lookup
  // 
  PlaylistItem *
  PlaylistWidget::lookup(PlaylistGroup * group, const QPoint & pt)
  {
    if (group)
    {
      for (std::vector<PlaylistItem>::iterator j = group->items_.begin();
           j != group->items_.end(); ++j)
      {
        PlaylistItem & item = *j;
        if (item.bbox_.contains(pt))
        {
          return &item;
        }
      }
      
      if (!group->items_.empty())
      {
        QRect bbox = group->items_.back().bbox_;
        if (bbox.y() + bbox.height() < pt.y())
        {
          return &group->items_.back();
        }
      }
    }
    
    return NULL;
  }
  
  //----------------------------------------------------------------
  // PlaylistWidget::lookup
  // 
  PlaylistItem *
  PlaylistWidget::lookup(const QPoint & pt)
  {
    PlaylistGroup * group = lookupGroup(pt);
    PlaylistItem * item = lookup(group, pt);
    return item;
  }
  
}
