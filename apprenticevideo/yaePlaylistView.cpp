// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created      : Tue Oct 20 19:19:59 PDT 2015
// Copyright    : Pavel Koshevoy
// License      : MIT -- http://www.opensource.org/licenses/mit-license.php

// standard C++:
#include <cmath>
#include <iomanip>
#include <sstream>

// Qt library:
#include <QItemSelectionModel>

// local interfaces:
#include "yaeColor.h"
#include "yaeExpression.h"
#include "yaeFlickableArea.h"
#include "yaeGradient.h"
#include "yaeImage.h"
#include "yaeItemFocus.h"
#include "yaeItemRef.h"
#include "yaePlaylistView.h"
#include "yaeProperty.h"
#include "yaeRectangle.h"
#include "yaeRoundRect.h"
#include "yaeScrollview.h"
#include "yaeSegment.h"
#include "yaeText.h"
#include "yaeTextInput.h"
#include "yaeTexture.h"
#include "yaeTexturedRect.h"
#include "yaeTransform.h"
#include "yaeTriangle.h"
#include "yaeUtilsQt.h"


namespace yae
{

  //----------------------------------------------------------------
  // toString
  //
  static std::string
  toString(const QModelIndex & index)
  {
    std::string path;

    QModelIndex ix = index;
    do
    {
      int row = ix.row();

      std::ostringstream oss;
      oss << row;

#if 0
      if (ix.model())
      {
        QString v = ix.data().toString();
        oss << " (" << v.toUtf8().constData() << ")";
      }
#endif

      if (!path.empty())
      {
        oss << '.' << path;
      }

      path = oss.str().c_str();
      ix = ix.parent();
    }
    while (ix.isValid());

    return path;
  }

  //----------------------------------------------------------------
  // drand
  //
  inline static double
  drand()
  {
#ifdef _WIN32
    int r = rand();
    return double(r) / double(RAND_MAX);
#else
    return drand48();
#endif
  }

  //----------------------------------------------------------------
  // calcCellWidth
  //
  inline static double
  calcCellWidth(double rowWidth)
  {
    double n = std::min<double>(5.0, std::floor(rowWidth / 160.0));
    return (n < 1.0) ? rowWidth : (rowWidth / n);
  }

  //----------------------------------------------------------------
  // calcCellHeight
  //
  inline static double
  calcCellHeight(double cellWidth)
  {
    double h = std::floor(cellWidth * 9.0 / 16.0);
    return h;
  }

  //----------------------------------------------------------------
  // calcItemsPerRow
  //
  inline static unsigned int
  calcItemsPerRow(double rowWidth)
  {
    double c = calcCellWidth(rowWidth);
    double n = std::floor(rowWidth / c);
    return (unsigned int)n;
  }

  //----------------------------------------------------------------
  // calcRows
  //
  inline static unsigned int
  calcRows(double viewWidth, double cellWidth, unsigned int numItems)
  {
    double cellsPerRow = std::floor(viewWidth / cellWidth);
    double n = std::max(1.0, std::ceil(double(numItems) / cellsPerRow));
    return n;
  }

  //----------------------------------------------------------------
  // calcTitleHeight
  //
  inline static double
  calcTitleHeight(double minHeight, double w)
  {
    return std::max<double>(minHeight, 24.0 * w / 800.0);
  }

  //----------------------------------------------------------------
  // GroupTop
  //
  struct GroupTop : public TDoubleExpr
  {
    GroupTop(const TPlaylistModelItem & item):
      item_(item)
    {
      YAE_ASSERT(item_.modelIndex().isValid());
    }

    // virtual:
    void evaluate(double & result) const
    {
      Item & groups = item_.parent<Item>();
      unsigned int groupIndex = item_.modelIndex().row();

      if (groupIndex < 1)
      {
        result = groups.top();
      }
      else
      {
        Item & prevGroup = *(groups.children_[groupIndex - 1]);
        result = prevGroup.bottom();
      }
    }

    const TPlaylistModelItem & item_;
  };

  //----------------------------------------------------------------
  // GridCellLeft
  //
  struct GridCellLeft : public TDoubleExpr
  {
    GridCellLeft(const TPlaylistModelItem & item):
      item_(item)
    {}

    // virtual:
    void evaluate(double & result) const
    {
      Item & grid = item_.parent<Item>();
      double gridWidth = grid.width();
      unsigned int cellsPerRow = calcItemsPerRow(gridWidth);
      unsigned int cellIndex = item_.modelIndex().row();
      std::size_t cellCol = cellIndex % cellsPerRow;
      double ox = grid.left() + 2;
      result = ox + gridWidth * double(cellCol) / double(cellsPerRow);
    }

    const TPlaylistModelItem & item_;
  };

  //----------------------------------------------------------------
  // GridCellTop
  //
  struct GridCellTop : public TDoubleExpr
  {
    GridCellTop(const TPlaylistModelItem & item):
      item_(item)
    {}

    // virtual:
    void evaluate(double & result) const
    {
      Item & grid = item_.parent<Item>();
      std::size_t numCells = grid.children_.size();
      double gridWidth = grid.width();
      double cellWidth = calcCellWidth(gridWidth);
      double cellHeight = cellWidth; // calcCellHeight(cellWidth);
      unsigned int cellsPerRow = calcItemsPerRow(gridWidth);
      unsigned int rowsOfCells = calcRows(gridWidth, cellWidth, numCells);
      double gridHeight = cellHeight * double(rowsOfCells);
      unsigned int cellIndex = item_.modelIndex().row();
      std::size_t cellRow = cellIndex / cellsPerRow;
      double oy = grid.top() + 2;
      result = oy + gridHeight * double(cellRow) / double(rowsOfCells);
    }

    const TPlaylistModelItem & item_;
  };

  //----------------------------------------------------------------
  // GridCellWidth
  //
  struct GridCellWidth : public TDoubleExpr
  {
    GridCellWidth(const Item & grid):
      grid_(grid)
    {}

    // virtual:
    void evaluate(double & result) const
    {
      double gridWidth = grid_.width();
      result = calcCellWidth(gridWidth) - 2;
    }

    const Item & grid_;
  };

  //----------------------------------------------------------------
  // GridCellHeight
  //
  struct GridCellHeight : public TDoubleExpr
  {
    GridCellHeight(const Item & grid):
      grid_(grid)
    {}

    // virtual:
    void evaluate(double & result) const
    {
      double gridWidth = grid_.width();
      double cellWidth = calcCellWidth(gridWidth);
      result = cellWidth - 2; // calcCellHeight(cellWidth) - 2;
    }

    const Item & grid_;
  };


  //----------------------------------------------------------------
  // CalcTitleHeight::CalcTitleHeight
  //
  CalcTitleHeight::CalcTitleHeight(const Item & titleContainer,
                                   double minHeight):
    titleContainer_(titleContainer),
    minHeight_(minHeight)
  {}

  //----------------------------------------------------------------
  // CalcTitleHeight::evaluate
  //
  void
  CalcTitleHeight::evaluate(double & result) const
  {
    double titleContainerWidth = titleContainer_.width();
    result = calcTitleHeight(minHeight_, titleContainerWidth);
  }


  //----------------------------------------------------------------
  // GetFontSize
  //
  struct GetFontSize : public TDoubleExpr
  {
    GetFontSize(const Item & titleHeight, double titleHeightScale,
                const Item & cellHeight, double cellHeightScale):
      titleHeight_(titleHeight),
      cellHeight_(cellHeight),
      titleHeightScale_(titleHeightScale),
      cellHeightScale_(cellHeightScale)
    {}

    // virtual:
    void evaluate(double & result) const
    {
      double t = 0.0;
      titleHeight_.get(kPropertyHeight, t);
      t *= titleHeightScale_;

      double c = 0.0;
      cellHeight_.get(kPropertyHeight, c);
      c *= cellHeightScale_;

      result = std::min(t, c);
    }

    const Item & titleHeight_;
    const Item & cellHeight_;

    double titleHeightScale_;
    double cellHeightScale_;
  };


  //----------------------------------------------------------------
  // PlaylistFooter
  //
  struct PlaylistFooter : public TVarExpr
  {
    PlaylistFooter(const PlaylistModelProxy & model):
      model_(model)
    {}

    // virtual:
    void evaluate(TVar & result) const
    {
      quint64 n = model_.itemCount();
      QString t = (n == 1) ?
        QObject::tr("1 item, end of playlist") :
        QObject::tr("%1 items, end of playlist").arg(n);

      result = QVariant(t);
    }

    const PlaylistModelProxy & model_;
  };

  //----------------------------------------------------------------
  // ModelQuery
  //
  struct ModelQuery : public TVarExpr
  {
    ModelQuery(const PlaylistModelProxy & model,
               const QModelIndex & index,
               int role):
      model_(model),
      index_(index),
      role_(role)
    {}

    // virtual:
    void evaluate(TVar & result) const
    {
      static_cast<QVariant &>(result) = model_.data(index_, role_);
    }

    const PlaylistModelProxy & model_;
    QPersistentModelIndex index_;
    int role_;
  };

  //----------------------------------------------------------------
  // TModelQuery
  //
  template <typename TData>
  struct TModelQuery : public Expression<TData>
  {
    TModelQuery(const PlaylistModelProxy & model,
                const QModelIndex & index,
                int role):
      model_(model),
      index_(index),
      role_(role)
    {}

    // virtual:
    void evaluate(TData & result) const
    {
      QVariant v = model_.data(index_, role_);

      if (!v.canConvert<TData>())
      {
        YAE_ASSERT(false);
        throw std::runtime_error("unexpected model data type");
      }

      result = v.value<TData>();
    }

    const PlaylistModelProxy & model_;
    QPersistentModelIndex index_;
    int role_;
  };

  //----------------------------------------------------------------
  // TQueryBool
  //
  typedef TModelQuery<bool> TQueryBool;

  //----------------------------------------------------------------
  // QueryBoolInverse
  //
  struct QueryBoolInverse : public TQueryBool
  {
    QueryBoolInverse(const PlaylistModelProxy & model,
                     const QModelIndex & index,
                     int role):
      TQueryBool(model, index, role)
    {}

    // virtual:
    void evaluate(bool & result) const
    {
      bool inverseResult = false;
      TQueryBool::evaluate(inverseResult);
      result = !inverseResult;
    }
  };

  //----------------------------------------------------------------
  // IsModelSortedBy
  //
  struct IsModelSortedBy : public TBoolExpr
  {
    IsModelSortedBy(const PlaylistModelProxy & model,
                    PlaylistModelProxy::SortBy sortBy):
      model_(model),
      sortBy_(sortBy)
    {}

    // virtual:
    void evaluate(bool & result) const
    {
      PlaylistModelProxy::SortBy modelSortedBy = model_.sortBy();
      result = (modelSortedBy == sortBy_);
    }

    const PlaylistModelProxy & model_;
    PlaylistModelProxy::SortBy sortBy_;
  };

  //----------------------------------------------------------------
  // IsModelSortOrder
  //
  struct IsModelSortOrder : public TBoolExpr
  {
    IsModelSortOrder(const PlaylistModelProxy & model,
                     Qt::SortOrder sortOrder):
      model_(model),
      sortOrder_(sortOrder)
    {}

    // virtual:
    void evaluate(bool & result) const
    {
      Qt::SortOrder modelSortOrder = model_.sortOrder();
      result = (modelSortOrder == sortOrder_);
    }

    const PlaylistModelProxy & model_;
    Qt::SortOrder sortOrder_;
  };

  //----------------------------------------------------------------
  // ItemHighlightColor
  //
  struct ItemHighlightColor : public TColorExpr
  {
    ItemHighlightColor(const PlaylistModelProxy & model,
                       const QModelIndex & index,
                       const Color & colorDefault,
                       const Color & colorSelected,
                       const Color & colorPlaying):
      model_(model),
      index_(index),
      colorDefault_(colorDefault),
      colorSelected_(colorSelected),
      colorPlaying_(colorPlaying)
    {}

    // virtual:
    void evaluate(Color & result) const
    {
      bool isSelected =
        model_.data(index_, PlaylistModel::kRoleSelected).value<bool>();

      if (isSelected)
      {
        result = colorSelected_;
        return;
      }

      bool isPlaying =
        model_.data(index_, PlaylistModel::kRolePlaying).value<bool>();

      if (isPlaying)
      {
        result = colorPlaying_;
        return;
      }

      result = colorDefault_;
    }

    const PlaylistModelProxy & model_;
    QPersistentModelIndex index_;
    Color colorDefault_;
    Color colorSelected_;
    Color colorPlaying_;
  };

  //----------------------------------------------------------------
  // ContrastColor
  //
  struct ContrastColor : public TColorExpr
  {
    ContrastColor(const Item & item, Property prop, double scaleAlpha):
      scaleAlpha_(scaleAlpha),
      item_(item),
      prop_(prop)
    {}

    // virtual:
    void evaluate(Color & result) const
    {
      Color c0;
      item_.get(prop_, c0);
      result = c0.bw_contrast();
      double a = scaleAlpha_ * double(result.a());
      result.set_a((unsigned char)(std::min(255.0, std::max(0.0, a))));
    }

    double scaleAlpha_;
    const Item & item_;
    Property prop_;
  };

  //----------------------------------------------------------------
  // xbuttonImage
  //
  static QImage
  xbuttonImage(unsigned int w, const Color & color)
  {
    QImage img(w, w, QImage::Format_ARGB32);

    // supersample each pixel:
    static const TVec2D sp[] = { TVec2D(0.25, 0.25), TVec2D(0.75, 0.25),
                                 TVec2D(0.25, 0.75), TVec2D(0.75, 0.75) };

    static const unsigned int supersample = sizeof(sp) / sizeof(TVec2D);

    int w2 = w / 2;
    double diameter = double(w);
    double center = diameter * 0.5;
    Segment sa(-center, diameter);
    Segment sb(-diameter * 0.1, diameter * 0.2);

    TVec2D origin(0.0, 0.0);
    TVec2D u_axis(0.707106781186548, 0.707106781186548);
    TVec2D v_axis(-0.707106781186548, 0.707106781186548);

    Vec<double, 4> outerColor(Color(0, 0.0));
    Vec<double, 4> innerColor(color);
    TVec2D samplePoint;

    for (int y = 0; y < int(w); y++)
    {
      unsigned char * dst = img.scanLine(y);
      samplePoint.set_y(double(y - w2));

      for (int x = 0; x < int(w); x++, dst += 4)
      {
        samplePoint.set_x(double(x - w2));

        double outer = 0.0;
        double inner = 0.0;

        for (unsigned int k = 0; k < supersample; k++)
        {
          TVec2D wcs_pt = samplePoint + sp[k];
          TVec2D pt = wcs_to_lcs(origin, u_axis, v_axis, wcs_pt);
          double oh = sa.pixelOverlap(pt.x()) * sb.pixelOverlap(pt.y());
          double ov = sb.pixelOverlap(pt.x()) * sa.pixelOverlap(pt.y());
          double innerOverlap = std::max<double>(oh, ov);
          double outerOverlap = 1.0 - innerOverlap;

          outer += outerOverlap;
          inner += innerOverlap;
        }

        double outerWeight = outer / double(supersample);
        double innerWeight = inner / double(supersample);
        Color c(outerColor * outerWeight + innerColor * innerWeight);
        memcpy(dst, &(c.argb_), sizeof(c.argb_));
      }
    }

    return img;
  }



  //----------------------------------------------------------------
  // TLayoutPtr
  //
  typedef PlaylistView::TLayoutPtr TLayoutPtr;

  //----------------------------------------------------------------
  // TLayoutHint
  //
  typedef PlaylistModel::LayoutHint TLayoutHint;

  //----------------------------------------------------------------
  // findLayoutDelegate
  //
  static TLayoutPtr
  findLayoutDelegate(const std::map<TLayoutHint, TLayoutPtr> & delegates,
                     TLayoutHint layoutHint)
  {
    std::map<TLayoutHint, TLayoutPtr>::const_iterator found =
      delegates.find(layoutHint);

    if (found != delegates.end())
    {
      return found->second;
    }

    YAE_ASSERT(false);
    return TLayoutPtr();
  }

  //----------------------------------------------------------------
  // findLayoutDelegate
  //
  static TLayoutPtr
  findLayoutDelegate(const std::map<TLayoutHint, TLayoutPtr> & delegates,
                     const PlaylistModelProxy & model,
                     const QModelIndex & modelIndex)
  {
    QVariant v = model.data(modelIndex, PlaylistModel::kRoleLayoutHint);

    if (v.canConvert<TLayoutHint>())
    {
      TLayoutHint layoutHint = v.value<TLayoutHint>();
      return findLayoutDelegate(delegates, layoutHint);
    }

    YAE_ASSERT(false);
    return TLayoutPtr();
  }

  //----------------------------------------------------------------
  // findLayoutDelegate
  //
  static TLayoutPtr
  findLayoutDelegate(const PlaylistView & view,
                     const PlaylistModelProxy & model,
                     const QModelIndex & modelIndex)
  {
    return findLayoutDelegate(view.layouts(), model, modelIndex);
  }

  //----------------------------------------------------------------
  // SetSortBy
  //
  struct SetSortBy : public InputArea
  {
    SetSortBy(const char * id,
              PlaylistView & view,
              PlaylistModelProxy & model,
              Item & filter,
              PlaylistModelProxy::SortBy sortBy):
      InputArea(id),
      view_(view),
      model_(model),
      filter_(filter),
      sortBy_(sortBy)
    {}

    // virtual:
    bool onPress(const TVec2D & itemCSysOrigin,
                 const TVec2D & rootCSysPoint)
    { return true; }

    // virtual:
    bool onClick(const TVec2D & itemCSysOrigin,
                 const TVec2D & rootCSysPoint)
    {
      view_.delegate()->requestRepaint();
      filter_.uncache();
      model_.setSortBy(sortBy_);
      return true;
    }

    PlaylistView & view_;
    PlaylistModelProxy & model_;
    Item & filter_;
    PlaylistModelProxy::SortBy sortBy_;
  };

  //----------------------------------------------------------------
  // SetSortOrder
  //
  struct SetSortOrder : public InputArea
  {
    SetSortOrder(const char * id,
                 PlaylistView & view,
                 PlaylistModelProxy & model,
                 Item & filter,
                 Qt::SortOrder sortOrder):
      InputArea(id),
      view_(view),
      model_(model),
      filter_(filter),
      sortOrder_(sortOrder)
    {}

    // virtual:
    bool onPress(const TVec2D & itemCSysOrigin,
                 const TVec2D & rootCSysPoint)
    { return true; }

    // virtual:
    bool onClick(const TVec2D & itemCSysOrigin,
                 const TVec2D & rootCSysPoint)
    {
      view_.delegate()->requestRepaint();
      filter_.uncache();
      model_.setSortOrder(sortOrder_);
      return true;
    }

    PlaylistView & view_;
    PlaylistModelProxy & model_;
    Item & filter_;
    Qt::SortOrder sortOrder_;
  };

  //----------------------------------------------------------------
  // ClearTextInput
  //
  struct ClearTextInput : public InputArea
  {
    ClearTextInput(const char * id, TextInput & edit, Text & view):
      InputArea(id),
      edit_(edit),
      view_(view)
    {}

    // virtual:
    bool onPress(const TVec2D & itemCSysOrigin,
                 const TVec2D & rootCSysPoint)
    { return true; }

    // virtual:
    bool onClick(const TVec2D & itemCSysOrigin,
                 const TVec2D & rootCSysPoint)
    {
      edit_.setText(QString());
      edit_.uncache();
      view_.uncache();
      return true;
    }

    TextInput & edit_;
    Text & view_;
  };

  //----------------------------------------------------------------
  // Uncache
  //
  struct Uncache : public Item::Observer
  {
    Uncache(Item & item):
      item_(item)
    {}

    // virtual:
    void observe(const Item & item, Item::Event e)
    {
      if (e == Item::kOnUncache)
      {
        item_.uncache();
      }
    }

    Item & item_;
  };

  //----------------------------------------------------------------
  // layoutPlaylistFilter
  //
  static void
  layoutPlaylistFilter(Item & item,
                       PlaylistView & view,
                       PlaylistModelProxy & model,
                       const QModelIndex & itemIndex,
                       const IPlaylistViewStyle & style)
  {
    // reuse pre-computed properties:
    const Item & playlist = *(view.root());
    const Item & fontSize = playlist["font_size"];
    const Item & scrollbar = playlist["scrollbar"];

    const Text & nowPlaying =
      dynamic_cast<const Text &>(playlist["now_playing"]);

    const Texture & xbuttonTexture =
      dynamic_cast<const Texture &>(playlist["xbutton_texture"]);

    ColorRef underlineColor = ColorRef::constant(style.cursor_);
    ColorRef sortColor = ColorRef::constant(style.fg_hint_);
    ColorRef colorTextBg = ColorRef::constant(style.bg_focus_.scale_a(0.5));
    ColorRef colorTextFg = ColorRef::constant(style.fg_focus_.scale_a(0.5));
    ColorRef colorEditBg = ColorRef::constant(style.bg_focus_.scale_a(0.0));
    ColorRef colorFocusBg = ColorRef::constant(style.bg_focus_);
    ColorRef colorFocusFg = ColorRef::constant(style.fg_focus_);
    ColorRef colorHighlightBg = ColorRef::constant(style.bg_edit_selected_);
    ColorRef colorHighlightFg = ColorRef::constant(style.fg_edit_selected_);

    Gradient & filterShadow = item.addNew<Gradient>("filterShadow");
    filterShadow.anchors_.fill(item);
    filterShadow.anchors_.bottom_.reset();
    filterShadow.height_ = ItemRef::reference(item, kPropertyHeight);
    filterShadow.color_ = style.filter_shadow_;

    RoundRect & filter = item.addNew<RoundRect>("bg");
    filter.anchors_.fill(item, 2);
    filter.anchors_.bottom_.reset();
    filter.height_ = ItemRef::scale(item, kPropertyHeight, 0.333);
    filter.radius_ = ItemRef::scale(filter, kPropertyHeight, 0.1);

    Item & icon = filter.addNew<Item>("filter_icon");
    {
      RoundRect & circle = icon.addNew<RoundRect>("circle");
      circle.anchors_.hcenter_ = ItemRef::scale(filter, kPropertyHeight, 0.5);
      circle.anchors_.vcenter_ = ItemRef::reference(filter, kPropertyVCenter);
      circle.width_ = ItemRef::scale(filter, kPropertyHeight, 0.5);
      circle.height_ = circle.width_;
      circle.radius_ = ItemRef::scale(circle, kPropertyWidth, 0.5);
      circle.border_ = ItemRef::scale(circle, kPropertyWidth, 0.1);
      circle.background_ = ColorRef::transparent(filter, kPropertyColor);
      circle.color_ = colorFocusBg;
      circle.colorBorder_ = ColorRef::constant(style.fg_focus_.scale_a(0.25));

      Transform & xform = icon.addNew<Transform>("xform");
      xform.anchors_.hcenter_ = circle.anchors_.hcenter_;
      xform.anchors_.vcenter_ = circle.anchors_.vcenter_;
      xform.rotation_ = ItemRef::constant(M_PI * 0.25);

      Rectangle & handle = xform.addNew<Rectangle>("handle");
      handle.anchors_.left_ = ItemRef::scale(filter, kPropertyHeight, 0.25);
      handle.anchors_.vcenter_ = ItemRef::constant(0.0);
      handle.width_ = handle.anchors_.left_;
      handle.height_ = ItemRef::scale(handle, kPropertyWidth, 0.25);
      handle.color_ = colorFocusBg;

      icon.anchors_.left_ = ItemRef::reference(circle, kPropertyLeft);
      icon.anchors_.top_ = ItemRef::reference(circle, kPropertyTop);
    }

    Text & text = filter.addNew<Text>("filter_text");
    text.font_ = style.font_;

    TextInput & edit = filter.addNew<TextInput>("filter_edit");
    edit.font_ = style.font_;

    Item & rm = filter.addNew<Item>("rm");

    TexturedRect & xbutton =
      rm.add<TexturedRect>(new TexturedRect("xbutton", xbuttonTexture));

    ItemRef fontDescentNowPlaying =
      xbutton.addExpr(new GetFontDescent(nowPlaying));

    ClearTextInput & maRmFilter =
      item.add(new ClearTextInput("ma_clear_filter", edit, text));
    maRmFilter.anchors_.fill(xbutton);

    TextInputProxy & editProxy =
      filter.add(new TextInputProxy("filter_focus", text, edit));
    ItemFocus::singleton().setFocusable(view, editProxy, 1);
    editProxy.anchors_.fill(filter);
    editProxy.placeholder_ =
      TVarRef::constant(TVar(QObject::tr("SEARCH AND FILTER")));

    filter.color_ = filter.addExpr(new ColorWhenFocused(editProxy,
                                                        colorTextBg,
                                                        colorFocusBg));

    text.anchors_.vcenter_ = ItemRef::reference(filter, kPropertyVCenter);
    text.anchors_.left_ = ItemRef::reference(icon, kPropertyRight);
    text.anchors_.right_ = ItemRef::offset(rm, kPropertyLeft, -3);
    text.margins_.left_ = ItemRef::scale(icon, kPropertyWidth, 0.5);
    text.margins_.bottom_ = text.addExpr(new GetFontDescent(text), -0.25);
    text.visible_ = edit.addExpr(new ShowWhenFocused(editProxy, false));
    text.elide_ = Qt::ElideLeft;
    text.color_ = colorTextFg;
    text.text_ = TVarRef::reference(editProxy, kPropertyText);
    text.fontSize_ =
      ItemRef::scale(fontSize, kPropertyHeight, 1.07 * kDpiScale);

    edit.anchors_.fill(text);
    edit.margins_.left_ = ItemRef::scale(edit, kPropertyCursorWidth, -1.0);
    edit.visible_ = edit.addExpr(new ShowWhenFocused(editProxy, true));
    edit.background_ = colorEditBg;
    edit.color_ = colorFocusFg;
    edit.cursorColor_ = underlineColor;
    edit.fontSize_ = text.fontSize_;
    edit.selectionBg_ = colorHighlightBg;
    edit.selectionFg_ = colorHighlightFg;
    edit.addObserver(Item::kOnUncache, Item::TObserverPtr(new Uncache(rm)));

    // remove filter [x] button:
    rm.width_ = ItemRef::reference(nowPlaying, kPropertyHeight);
    rm.height_ = ItemRef::reference(text, kPropertyHeight);
    rm.anchors_.top_ = ItemRef::reference(text, kPropertyTop);
    rm.anchors_.right_ = ItemRef::offset(scrollbar, kPropertyLeft, -5);
    rm.visible_ = BoolRef::reference(editProxy, kPropertyHasText);

    xbutton.anchors_.center(rm);
    xbutton.margins_.set(fontDescentNowPlaying);
    xbutton.width_ = xbutton.addExpr(new InscribedCircleDiameterFor(rm));
    xbutton.height_ = xbutton.width_;

    // layout sort-and-order:
    ItemRef smallFontSize = ItemRef::scale(fontSize,
                                           kPropertyHeight,
                                           0.7 * kDpiScale);
    QFont smallFont(style.font_small_);
    smallFont.setBold(true);

    Item & sortAndOrder = item.addNew<Item>("sort_and_order");
    sortAndOrder.anchors_.top_ = ItemRef::reference(filter, kPropertyBottom);
    sortAndOrder.anchors_.left_ = ItemRef::reference(filter, kPropertyLeft);
    sortAndOrder.margins_.left_ = ItemRef::scale(icon, kPropertyWidth, 0.25);

    Rectangle & ulName = sortAndOrder.addNew<Rectangle>("underline_name");
    Rectangle & ulTime = sortAndOrder.addNew<Rectangle>("underline_time");
    Rectangle & ulAsc = sortAndOrder.addNew<Rectangle>("underline_asc");
    Rectangle & ulDesc = sortAndOrder.addNew<Rectangle>("underline_desc");

    Text & sortBy = sortAndOrder.addNew<Text>("sort_by");
    sortBy.anchors_.left_ = ItemRef::reference(sortAndOrder, kPropertyLeft);
    sortBy.anchors_.top_ = ItemRef::reference(sortAndOrder, kPropertyTop);
    sortBy.text_ = TVarRef::constant(TVar(QObject::tr("sort by ")));
    sortBy.color_ = sortColor;
    sortBy.font_ = smallFont;
    sortBy.fontSize_ = smallFontSize;

    Text & byName = sortAndOrder.addNew<Text>("by_name");
    byName.anchors_.left_ = ItemRef::reference(sortBy, kPropertyRight);
    byName.anchors_.top_ = ItemRef::reference(sortAndOrder, kPropertyTop);
    byName.text_ = TVarRef::constant(TVar(QObject::tr("name")));
    byName.color_ = sortColor;
    byName.font_ = smallFont;
    byName.fontSize_ = smallFontSize;

    Text & nameOr = sortAndOrder.addNew<Text>("name_or");
    nameOr.anchors_.left_ = ItemRef::reference(byName, kPropertyRight);
    nameOr.anchors_.top_ = ItemRef::reference(sortAndOrder, kPropertyTop);
    nameOr.text_ = TVarRef::constant(TVar(QObject::tr(" or ")));
    nameOr.color_ = sortColor;
    nameOr.font_ = smallFont;
    nameOr.fontSize_ = smallFontSize;

    Text & orTime = sortAndOrder.addNew<Text>("or_time");
    orTime.anchors_.left_ = ItemRef::reference(nameOr, kPropertyRight);
    orTime.anchors_.top_ = ItemRef::reference(sortAndOrder, kPropertyTop);
    orTime.text_ = TVarRef::constant(TVar(QObject::tr("time")));
    orTime.color_ = sortColor;
    orTime.font_ = smallFont;
    orTime.fontSize_ = smallFontSize;

    Text & comma = sortAndOrder.addNew<Text>("comma");
    comma.anchors_.left_ = ItemRef::reference(orTime, kPropertyRight);
    comma.anchors_.top_ = ItemRef::reference(sortAndOrder, kPropertyTop);
    comma.text_ = TVarRef::constant(TVar(QObject::tr(", in ")));
    comma.color_ = sortColor;
    comma.font_ = smallFont;
    comma.fontSize_ = smallFontSize;

    Text & inAsc = sortAndOrder.addNew<Text>("in_asc");
    inAsc.anchors_.left_ = ItemRef::reference(comma, kPropertyRight);
    inAsc.anchors_.top_ = ItemRef::reference(sortAndOrder, kPropertyTop);
    inAsc.text_ = TVarRef::constant(TVar(QObject::tr("ascending")));
    inAsc.color_ = sortColor;
    inAsc.font_ = smallFont;
    inAsc.fontSize_ = smallFontSize;

    Text & ascOr = sortAndOrder.addNew<Text>("asc_or");
    ascOr.anchors_.left_ = ItemRef::reference(inAsc, kPropertyRight);
    ascOr.anchors_.top_ = ItemRef::reference(sortAndOrder, kPropertyTop);
    ascOr.text_ = TVarRef::constant(TVar(QObject::tr(" or ")));
    ascOr.color_ = sortColor;
    ascOr.font_ = smallFont;
    ascOr.fontSize_ = smallFontSize;

    Text & orDesc = sortAndOrder.addNew<Text>("or_desc");
    orDesc.anchors_.left_ = ItemRef::reference(ascOr, kPropertyRight);
    orDesc.anchors_.top_ = ItemRef::reference(sortAndOrder, kPropertyTop);
    orDesc.text_ = TVarRef::constant(TVar(QObject::tr("descending")));
    orDesc.color_ = sortColor;
    orDesc.font_ = smallFont;
    orDesc.fontSize_ = smallFontSize;

    Text & order = sortAndOrder.addNew<Text>("order");
    order.anchors_.left_ = ItemRef::reference(orDesc, kPropertyRight);
    order.anchors_.top_ = ItemRef::reference(sortAndOrder, kPropertyTop);
    order.text_ = TVarRef::constant(TVar(QObject::tr(" order")));
    order.color_ = sortColor;
    order.font_ = smallFont;
    order.fontSize_ = smallFontSize;

    ulName.anchors_.left_ = ItemRef::offset(byName, kPropertyLeft, -1);
    ulName.anchors_.right_ = ItemRef::offset(byName, kPropertyRight, 1);
    ulName.anchors_.top_ = ItemRef::offset(byName, kPropertyBottom, 0);
    ulName.height_ = ItemRef::constant(2);
    ulName.color_ = underlineColor;
    ulName.visible_ = ulName.
      addExpr(new IsModelSortedBy(model, PlaylistModelProxy::SortByName));

    ulTime.anchors_.left_ = ItemRef::offset(orTime, kPropertyLeft, -1);
    ulTime.anchors_.right_ = ItemRef::offset(orTime, kPropertyRight, 1);
    ulTime.anchors_.top_ = ItemRef::offset(orTime, kPropertyBottom, 0);
    ulTime.height_ = ItemRef::constant(2);
    ulTime.color_ = underlineColor;
    ulTime.visible_ = ulTime.
      addExpr(new IsModelSortedBy(model, PlaylistModelProxy::SortByTime));

    ulAsc.anchors_.left_ = ItemRef::offset(inAsc, kPropertyLeft, -1);
    ulAsc.anchors_.right_ = ItemRef::offset(inAsc, kPropertyRight, 1);
    ulAsc.anchors_.top_ = ItemRef::offset(inAsc, kPropertyBottom, 0);
    ulAsc.height_ = ItemRef::constant(2);
    ulAsc.color_ = underlineColor;
    ulAsc.visible_ = ulAsc.
      addExpr(new IsModelSortOrder(model, Qt::AscendingOrder));

    ulDesc.anchors_.left_ = ItemRef::offset(orDesc, kPropertyLeft, -1);
    ulDesc.anchors_.right_ = ItemRef::offset(orDesc, kPropertyRight, 1);
    ulDesc.anchors_.top_ = ItemRef::offset(orDesc, kPropertyBottom, 0);
    ulDesc.height_ = ItemRef::constant(2);
    ulDesc.color_ = underlineColor;
    ulDesc.visible_ = ulDesc.
      addExpr(new IsModelSortOrder(model, Qt::DescendingOrder));

    SetSortBy & sortByName = item.
      add(new SetSortBy("ma_sort_by_name", view, model, item,
                        PlaylistModelProxy::SortByName));
    sortByName.anchors_.fill(byName);

    SetSortBy & sortByTime = item.
      add(new SetSortBy("ma_sort_by_time", view, model, item,
                        PlaylistModelProxy::SortByTime));
    sortByTime.anchors_.fill(orTime);

    SetSortOrder & sortOrderAsc = item.
      add(new SetSortOrder("ma_order_asc", view, model, item,
                           Qt::AscendingOrder));
    sortOrderAsc.anchors_.fill(inAsc);

    SetSortOrder & sortOrderDesc = item.
      add(new SetSortOrder("ma_order_desc", view, model, item,
                           Qt::DescendingOrder));
    sortOrderDesc.anchors_.fill(orDesc);
  }

  //----------------------------------------------------------------
  // layoutPlaylistFooter
  //
  static void
  layoutPlaylistFooter(Item & footer,
                       PlaylistView & view,
                       PlaylistModelProxy & model,
                       const QModelIndex & itemIndex,
                       const IPlaylistViewStyle & style)
  {
    const Item & playlist = *(view.root());
    const Item & fontSize = playlist["font_size"];

    Rectangle & separator = footer.addNew<Rectangle>("footer_separator");
    separator.anchors_.fill(footer);
    separator.anchors_.bottom_.reset();
    separator.height_ = ItemRef::constant(2.0);
    separator.color_ = ColorRef::constant(style.separator_);

    QFont smallFont(style.font_small_);
    smallFont.setBold(true);
    ItemRef smallFontSize = ItemRef::scale(fontSize,
                                           kPropertyHeight,
                                           0.7 * kDpiScale);

    Text & footNote = footer.addNew<Text>("footNote");
    footNote.anchors_.top_ =
      ItemRef::reference(footer, kPropertyTop);
    footNote.anchors_.right_ =
      ItemRef::reference(footer, kPropertyRight);
    footNote.margins_.top_ =
      ItemRef::scale(fontSize, kPropertyHeight, 0.5 * kDpiScale);
    footNote.margins_.right_ =
      ItemRef::scale(fontSize, kPropertyHeight, 0.8 * kDpiScale);

    footNote.text_ = footNote.addExpr(new PlaylistFooter(model));
    footNote.font_ = smallFont;
    footNote.fontSize_ = smallFontSize;
    footNote.color_ = ColorRef::constant(style.fg_hint_);
    footNote.background_ = ColorRef::constant(style.bg_.scale_a(0.0));
  }

  //----------------------------------------------------------------
  // layoutPlaylistGroup
  //
  static void
  layoutPlaylistGroup(Item & groups,
                      TPlaylistModelItem & group,
                      PlaylistView & view,
                      PlaylistModelProxy & model,
                      const QModelIndex & groupIndex,
                      const IPlaylistViewStyle & style)
  {
    group.anchors_.left_ = ItemRef::reference(groups, kPropertyLeft);
    group.anchors_.right_ = ItemRef::reference(groups, kPropertyRight);
    group.anchors_.top_ = group.addExpr(new GroupTop(group));

    TLayoutPtr childLayout = findLayoutDelegate(view, model, groupIndex);
    if (childLayout)
    {
      childLayout->layout(group, view, model, groupIndex, style);
    }
  }

  //----------------------------------------------------------------
  // GroupListLayout
  //
  struct GroupListLayout : public PlaylistView::TLayoutDelegate
  {
    void layout(Item & groups,
                PlaylistView & view,
                PlaylistModelProxy & model,
                const QModelIndex & rootIndex,
                const IPlaylistViewStyle & style)
    {
      const int numGroups = model.rowCount(rootIndex);
      for (int i = 0; i < numGroups; i++)
      {
        QModelIndex childIndex = model.index(i, 0, rootIndex);
        TPlaylistModelItem & group =
          groups.add(new TPlaylistModelItem("group", childIndex));
        layoutPlaylistGroup(groups, group, view, model, childIndex, style);
      }
    }
  };

  //----------------------------------------------------------------
  // GroupCollapse
  //
  struct GroupCollapse : public TClickablePlaylistModelItem
  {
    GroupCollapse(const char * id, PlaylistView & view):
      TClickablePlaylistModelItem(id),
      view_(view)
    {}

    // virtual:
    bool onClick(const TVec2D & itemCSysOrigin,
                 const TVec2D & rootCSysPoint)
    {
      TClickablePlaylistModelItem::TModel & model = this->model();
      const QPersistentModelIndex & modelIndex = this->modelIndex();
      int role = PlaylistModel::kRoleCollapsed;
      bool collapsed = modelIndex.data(role).toBool();
      model.setData(modelIndex, QVariant(!collapsed), role);
      view_.requestRepaint();
      return true;
    }

    PlaylistView & view_;
  };

  //----------------------------------------------------------------
  // RemoveModelItems
  //
  struct RemoveModelItems : public TClickablePlaylistModelItem
  {
    RemoveModelItems(const char * id):
      TClickablePlaylistModelItem(id)
    {}

    // virtual:
    bool onClick(const TVec2D & itemCSysOrigin,
                 const TVec2D & rootCSysPoint)
    {
      TClickablePlaylistModelItem::TModel & model = this->model();
      const QPersistentModelIndex & modelIndex = this->modelIndex();
      model.removeItems(modelIndex);
      return true;
    }
  };

  //----------------------------------------------------------------
  // ItemPlay
  //
  struct ItemPlay : public TClickablePlaylistModelItem
  {
    ItemPlay(const char * id):
      TClickablePlaylistModelItem(id)
    {}

    // virtual:
    bool onDoubleClick(const TVec2D & itemCSysOrigin,
                       const TVec2D & rootCSysPoint)
    {
      const QPersistentModelIndex & modelIndex = this->modelIndex();
      TClickablePlaylistModelItem::TModel & model = this->model();
      model.setPlayingItem(modelIndex);
      return true;
    }
  };

  //----------------------------------------------------------------
  // layoutPlaylistItem
  //
  static void
  layoutPlaylistItem(Item & grid,
                     TPlaylistModelItem & cell,
                     const Item & cellWidth,
                     const Item & cellHeight,
                     PlaylistView & view,
                     PlaylistModelProxy & model,
                     const QModelIndex & itemIndex,
                     const IPlaylistViewStyle & style)
  {
    cell.anchors_.left_ = cell.addExpr(new GridCellLeft(cell));
    cell.anchors_.top_ = cell.addExpr(new GridCellTop(cell));
    cell.width_ = ItemRef::reference(cellWidth, kPropertyWidth);
    cell.height_ = ItemRef::reference(cellHeight, kPropertyHeight);

    ItemPlay & maPlay = cell.add(new ItemPlay("ma_cell"));
    maPlay.anchors_.fill(cell);

    TLayoutPtr childLayout = findLayoutDelegate(view, model, itemIndex);
    if (childLayout)
    {
      childLayout->layout(cell, view, model, itemIndex, style);
    }
  }

  //----------------------------------------------------------------
  // ItemGridLayout
  //
  struct ItemGridLayout : public PlaylistView::TLayoutDelegate
  {
    void layout(Item & group,
                PlaylistView & view,
                PlaylistModelProxy & model,
                const QModelIndex & groupIndex,
                const IPlaylistViewStyle & style)
    {
      // reuse pre-computed properties:
      const Item & playlist = *(view.root());
      const Item & fontSize = playlist["font_size"];
      const Item & cellWidth = playlist["cell_width"];
      const Item & cellHeight = playlist["cell_height"];
      const Item & titleHeight = playlist["title_height"];

      const Text & nowPlaying =
        dynamic_cast<const Text &>(playlist["now_playing"]);

      const Texture & xbuttonTexture =
        dynamic_cast<const Texture &>(playlist["xbutton_texture"]);

      Item & spacer = group.addNew<Item>("spacer");
      spacer.anchors_.left_ = ItemRef::reference(group, kPropertyLeft);
      spacer.anchors_.top_ = ItemRef::reference(group, kPropertyTop);
      spacer.width_ = ItemRef::reference(group, kPropertyWidth);
      spacer.height_ = ItemRef::scale(titleHeight, kPropertyHeight, 0.2);

      Item & title = group.addNew<Item>("title");
      title.anchors_.top_ = ItemRef::offset(spacer, kPropertyBottom, 5);
      title.anchors_.left_ = ItemRef::reference(group, kPropertyLeft);
      title.anchors_.right_ = ItemRef::reference(group, kPropertyRight);

      Item & chevron = title.addNew<Item>("chevron");
      Triangle & collapsed = chevron.addNew<Triangle>("collapse");

      Text & text = title.addNew<Text>("text");
      text.font_ = style.font_;

      Item & rm = title.addNew<Item>("rm");
      TexturedRect & xbutton =
        rm.add<TexturedRect>(new TexturedRect("xbutton", xbuttonTexture));
      ItemRef fontDescent =
        xbutton.addExpr(new GetFontDescent(text));
      ItemRef fontDescentNowPlaying =
        xbutton.addExpr(new GetFontDescent(nowPlaying));

      // open/close disclosure [>] button:
      chevron.width_ = ItemRef::reference(text, kPropertyHeight);
      chevron.height_ = ItemRef::reference(text, kPropertyHeight);
      chevron.anchors_.top_ = ItemRef::reference(text, kPropertyTop);
      chevron.anchors_.left_ = ItemRef::offset(title, kPropertyLeft);

      collapsed.anchors_.fill(chevron);
      collapsed.margins_.set(fontDescent);
      collapsed.collapsed_ = collapsed.addExpr
        (new TQueryBool(model, groupIndex, PlaylistModel::kRoleCollapsed));

      text.anchors_.top_ = ItemRef::reference(title, kPropertyTop);
      text.anchors_.left_ = ItemRef::reference(chevron, kPropertyRight);
      text.anchors_.right_ = ItemRef::reference(rm, kPropertyLeft);
      text.text_ = text.addExpr
        (new ModelQuery(model, groupIndex, PlaylistModel::kRoleLabel));
      text.fontSize_ =
        ItemRef::scale(fontSize, kPropertyHeight, 1.07 * kDpiScale);
      text.elide_ = Qt::ElideMiddle;

      // remove group [x] button:
      rm.width_ = ItemRef::reference(nowPlaying, kPropertyHeight);
      rm.height_ = ItemRef::reference(text, kPropertyHeight);
      rm.anchors_.top_ = ItemRef::reference(text, kPropertyTop);
      rm.anchors_.right_ = ItemRef::offset(title, kPropertyRight, -5);

      xbutton.anchors_.center(rm);
      xbutton.margins_.set(fontDescentNowPlaying);
      xbutton.width_ = xbutton.addExpr(new InscribedCircleDiameterFor(rm));
      xbutton.height_ = xbutton.width_;

      Rectangle & separator = group.addNew<Rectangle>("separator");
      separator.anchors_.top_ = ItemRef::offset(title, kPropertyBottom, 5);
      separator.anchors_.left_ = ItemRef::offset(group, kPropertyLeft, 2);
      separator.anchors_.right_ = ItemRef::reference(group, kPropertyRight);
      separator.height_ = ItemRef::constant(2.0);
      separator.color_ = style.separator_;

      Item & payload = group.addNew<Item>("payload");
      payload.anchors_.top_ = ItemRef::reference(separator, kPropertyBottom);
      payload.anchors_.left_ = ItemRef::reference(group, kPropertyLeft);
      payload.anchors_.right_ = ItemRef::reference(group, kPropertyRight);
      payload.visible_ = payload.addExpr
        (new QueryBoolInverse(model,
                              groupIndex,
                              PlaylistModel::kRoleCollapsed));
      payload.height_ = payload.addExpr(new InvisibleItemZeroHeight(payload));

      GroupCollapse & maCollapse = collapsed.
        add(new GroupCollapse("ma_collapse", view));
      maCollapse.anchors_.fill(collapsed);

      RemoveModelItems & maRmGroup = xbutton.
        add(new RemoveModelItems("ma_remove_group"));
      maRmGroup.anchors_.fill(xbutton);

      Item & grid = payload.addNew<Item>("grid");
      grid.anchors_.top_ = ItemRef::reference(separator, kPropertyBottom);
      grid.anchors_.left_ = ItemRef::reference(group, kPropertyLeft);
      grid.anchors_.right_ = ItemRef::reference(group, kPropertyRight);

      const int numCells = model.rowCount(groupIndex);
      for (int i = 0; i < numCells; i++)
      {
        QModelIndex childIndex = model.index(i, 0, groupIndex);
        TPlaylistModelItem & cell =
          grid.add(new TPlaylistModelItem("cell", childIndex));
        layoutPlaylistItem(grid,
                           cell,
                           cellWidth,
                           cellHeight,
                           view,
                           model,
                           childIndex,
                           style);
      }

      Item & footer = payload.addNew<Item>("footer");
      footer.anchors_.left_ = ItemRef::reference(group, kPropertyLeft);
      footer.anchors_.top_ = ItemRef::reference(grid, kPropertyBottom);
      footer.width_ = ItemRef::reference(group, kPropertyWidth);
      footer.height_ = ItemRef::reference(titleHeight, kPropertyHeight);
    }
  };

  //----------------------------------------------------------------
  // ImageLive
  //
  struct ImageLive : public Item
  {
    mutable Canvas * canvas_;

    ImageLive(const char * id):
      Item(id),
      canvas_(NULL)
    {}

    // virtual:
    bool paint(const Segment & xregion,
               const Segment & yregion,
               Canvas * canvas) const
    {
      canvas_ = canvas;
      return Item::paint(xregion, yregion, canvas);
    }

    // virtual:
    void paintContent() const
    {
      if (!canvas_)
      {
        return;
      }

      CanvasRenderer * renderer = canvas_->canvasRenderer();
      double croppedWidth = 0.0;
      double croppedHeight = 0.0;
      int cameraRotation = 0;
      renderer->imageWidthHeightRotated(croppedWidth,
                                        croppedHeight,
                                        cameraRotation);
      if (!croppedWidth || !croppedHeight)
      {
        return;
      }

      double x = this->left();
      double y = this->top();
      double w_max = this->width();
      double h_max = this->height();
      double w = w_max;
      double h = h_max;
      double car = w / h;
      double dar = croppedWidth / croppedHeight;

      if (dar < car)
      {
        w = h_max * dar;
        x += 0.5 * (w_max - w);
      }
      else
      {
        h = w_max / dar;
        y += 0.5 * (h_max - h);
      }

      TGLSaveMatrixState pushViewMatrix(GL_MODELVIEW);

      YAE_OGL_11_HERE();
      YAE_OGL_11(glTranslated(x, y, 0.0));
      YAE_OGL_11(glScaled(w / croppedWidth, h / croppedHeight, 1.0));

      if (cameraRotation && cameraRotation % 90 == 0)
      {
        YAE_OGL_11(glTranslated(0.5 * croppedWidth, 0.5 * croppedHeight, 0));
        YAE_OGL_11(glRotated(double(cameraRotation), 0, 0, 1));

        if (cameraRotation % 180 != 0)
        {
          YAE_OGL_11(glTranslated(-0.5 * croppedHeight,
                                  -0.5 * croppedWidth, 0));
        }
        else
        {
          YAE_OGL_11(glTranslated(-0.5 * croppedWidth,
                                  -0.5 * croppedHeight, 0));
        }
      }

      renderer->draw();
      yae_assert_gl_no_error();
    }
  };

  //----------------------------------------------------------------
  // ItemGridCellLayout
  //
  struct ItemGridCellLayout : public PlaylistView::TLayoutDelegate
  {
    void layout(Item & cell,
                PlaylistView & view,
                PlaylistModelProxy & model,
                const QModelIndex & index,
                const IPlaylistViewStyle & style)
    {
      const Item & playlist = *(view.root());
      const Item & fontSize = playlist["font_size"];

      const Texture & xbuttonTexture =
        dynamic_cast<const Texture &>(playlist["xbutton_texture"]);

      Rectangle & frame = cell.addNew<Rectangle>("frame");
      frame.anchors_.fill(cell);
      frame.color_ = frame.
        addExpr(new ItemHighlightColor(model,
                                       index,
                                       style.bg_item_,
                                       style.bg_item_selected_,
                                       style.bg_item_playing_));

      Image & thumbnail = cell.addNew<Image>("thumbnail");
      thumbnail.setContext(view);
      thumbnail.anchors_.fill(cell);
      thumbnail.anchors_.bottom_.reset();
      thumbnail.height_ = ItemRef::scale(cell, kPropertyHeight, 0.75);
      thumbnail.url_ = thumbnail.addExpr
        (new ModelQuery(model, index, PlaylistModel::kRoleThumbnail));
      thumbnail.visible_ = thumbnail.addExpr
        (new QueryBoolInverse(model, index, PlaylistModel::kRolePlaying));

      ImageLive & liveImage = cell.addNew<ImageLive>("live_image");
      liveImage.anchors_.fill(thumbnail);
      liveImage.visible_ = thumbnail.addExpr
        (new TQueryBool(model, index, PlaylistModel::kRolePlaying));

      Rectangle & badgeBg = cell.addNew<Rectangle>("badgeBg");
      Text & badge = cell.addNew<Text>("badge");
      badge.font_ = style.font_;
      badge.anchors_.top_ = ItemRef::offset(cell, kPropertyTop, 5);
      badge.anchors_.left_ = ItemRef::offset(cell, kPropertyLeft, 7);
      badge.maxWidth_ = ItemRef::offset(cell, kPropertyWidth, -14);
      badge.background_ = badge.
        addExpr(new ContrastColor(badge, kPropertyColor, 0.0));
      badge.color_ = badge.
        addExpr(new ItemHighlightColor(model,
                                       index,
                                       style.fg_badge_,
                                       style.fg_label_selected_,
                                       style.fg_badge_));
      badge.text_ = badge.addExpr
        (new ModelQuery(model, index, PlaylistModel::kRoleBadge));
      badge.fontSize_ = ItemRef::scale(fontSize,
                                       kPropertyHeight,
                                       0.8 * kDpiScale);

      badgeBg.anchors_.inset(badge, -3, 0);
      badgeBg.color_ = badge.
        addExpr(new ItemHighlightColor(model,
                                       index,
                                       style.bg_badge_,
                                       style.bg_label_selected_,
                                       style.bg_badge_));

      Rectangle & labelBg = cell.addNew<Rectangle>("labelBg");
      Text & label = cell.addNew<Text>("label");
      label.font_ = style.font_;
      label.anchors_.bottomLeft(cell, 7);
      label.maxWidth_ = ItemRef::offset(cell, kPropertyWidth, -14);
      label.text_ = label.addExpr
        (new ModelQuery(model, index, PlaylistModel::kRoleLabel));
      label.fontSize_ = ItemRef::scale(fontSize, kPropertyHeight, kDpiScale);
      label.background_ = label.
        addExpr(new ContrastColor(label, kPropertyColor, 0.0));
      label.color_ = label.
        addExpr(new ItemHighlightColor(model,
                                       index,
                                       style.fg_label_,
                                       style.fg_label_selected_,
                                       style.fg_label_));

      labelBg.anchors_.inset(label, -3, -1);
      labelBg.color_ = labelBg.
        addExpr(new ItemHighlightColor(model,
                                       index,
                                       style.bg_label_,
                                       style.bg_label_selected_,
                                       style.bg_label_));

      Item & rm = cell.addNew<Item>("remove item");

      Rectangle & playingBg = cell.addNew<Rectangle>("playingBg");
      Text & playing = cell.addNew<Text>("now playing");
      playing.font_ = style.font_;
      playing.anchors_.top_ = ItemRef::offset(cell, kPropertyTop, 5);
      playing.anchors_.right_ = ItemRef::offset(rm, kPropertyLeft, -5);
      playing.visible_ = BoolRef::reference(liveImage, kPropertyVisible);
      playing.text_ = TVarRef::constant(TVar(QObject::tr("NOW PLAYING")));
      playing.color_ = ColorRef::reference(label, kPropertyColor);
      playing.background_ = ColorRef::reference(label, kPropertyColorBg);
      playing.fontSize_ = ItemRef::scale(fontSize,
                                         kPropertyHeight,
                                         0.8 * kDpiScale);

      playingBg.anchors_.inset(playing, -3, -1);
      playingBg.visible_ = BoolRef::reference(liveImage, kPropertyVisible);
      playingBg.color_ = ColorRef::reference(labelBg, kPropertyColor);

      rm.width_ = ItemRef::reference(playing, kPropertyHeight);
      rm.height_ = ItemRef::reference(playing, kPropertyHeight);
      rm.anchors_.top_ = ItemRef::reference(playing, kPropertyTop);
      rm.anchors_.right_ = ItemRef::offset(cell, kPropertyRight, -5);

      TexturedRect & xbutton =
        rm.add<TexturedRect>(new TexturedRect("xbutton", xbuttonTexture));
      ItemRef fontDescent = xbutton.addExpr(new GetFontDescent(playing));
      xbutton.anchors_.center(rm);
      xbutton.margins_.set(fontDescent);
      xbutton.width_ = xbutton.addExpr(new InscribedCircleDiameterFor(rm));
      xbutton.height_ = xbutton.width_;

      Rectangle & underline = cell.addNew<Rectangle>("underline");
      underline.anchors_.left_ = ItemRef::offset(playing, kPropertyLeft, -1);
      underline.anchors_.right_ = ItemRef::offset(playing, kPropertyRight, 1);
      underline.anchors_.top_ = ItemRef::offset(playing, kPropertyBottom, 2);
      underline.height_ = ItemRef::constant(2);
      underline.color_ = ColorRef::constant(style.cursor_);
      underline.visible_ = BoolRef::reference(liveImage, kPropertyVisible);

      Rectangle & cur = cell.addNew<Rectangle>("current");
      cur.anchors_.left_ = ItemRef::offset(cell, kPropertyLeft, 3);
      cur.anchors_.right_ = ItemRef::offset(cell, kPropertyRight, -3);
      cur.anchors_.bottom_ = ItemRef::offset(cell, kPropertyBottom, -3);
      cur.height_ = ItemRef::constant(2);
      cur.color_ = ColorRef::constant(style.cursor_);
      cur.visible_ = cur.addExpr
        (new TQueryBool(model, index, PlaylistModel::kRoleCurrent));

      RemoveModelItems & maRmItem = xbutton.
        add(new RemoveModelItems("ma_remove_item"));
      maRmItem.anchors_.fill(xbutton);
    }
  };

  //----------------------------------------------------------------
  // GridViewStyle
  //
  struct GridViewStyle : public IPlaylistViewStyle
  {
    GridViewStyle()
    {
      // main font:
#if (QT_VERSION >= QT_VERSION_CHECK(4, 8, 0))
      font_.setHintingPreference(QFont::PreferFullHinting);
#endif
      font_.setStyleHint(QFont::SansSerif);
      font_.setStyleStrategy((QFont::StyleStrategy)
                             (QFont::PreferOutline |
                              QFont::PreferAntialias |
                              QFont::OpenGLCompatible));

      static bool hasImpact =
        QFontInfo(QFont("impact")).family().
        contains(QString::fromUtf8("impact"), Qt::CaseInsensitive);

      if (hasImpact)
      {
        font_.setFamily("impact");
      }
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0)) || !defined(__APPLE__)
      else
#endif
      {
        font_.setStretch(QFont::Condensed);
        font_.setWeight(QFont::Black);
      }

      // filter shadow gradient:
      filter_shadow_[0.0] = Color(0x1f1f1f, 1.0);
      filter_shadow_[0.42] = Color(0x1f1f1f, 0.9);
      filter_shadow_[1.0] = Color(0x1f1f1f, 0.0);

      // color palette:
      bg_ = Color(0x1f1f1f, 0.87);
      fg_ = Color(0xffffff, 1.0);

      cursor_ = Color(0xf12b24, 1.0);
      separator_ = Color(0x7f7f7f, 0.5);

      bg_focus_ = Color(0x7f7f7f, 0.5);
      fg_focus_ = Color(0xffffff, 1.0);

      bg_edit_selected_ = Color(0xffffff, 1.0);
      fg_edit_selected_ = Color(0x000000, 1.0);

      bg_hint_ = Color(0x1f1f1f, 0.0);
      fg_hint_ = Color(0xffffff, 0.5);

      bg_badge_ = Color(0x3f3f3f, 0.25);
      fg_badge_ = Color(0xffffff, 0.5);

      bg_label_ = Color(0x3f3f3f, 0.5);
      fg_label_ = Color(0xffffff, 1.0);

      bg_label_selected_ = Color(0xffffff, 0.75);
      fg_label_selected_ = Color(0x3f3f3f, 0.75);

      bg_item_ = Color(0x7f7f7f, 0.5);
      bg_item_playing_ = Color(0x1f1f1f, 0.5);
      bg_item_selected_ = Color(0xffffff, 0.75);
    }
  };

  //----------------------------------------------------------------
  // PlaylistView::PlaylistView
  //
  PlaylistView::PlaylistView():
    ItemView("playlist"),
    model_(NULL),
    style_(new GridViewStyle())
  {
    layoutDelegates_[PlaylistModel::kLayoutHintGroupList] =
      TLayoutPtr(new GroupListLayout());

    layoutDelegates_[PlaylistModel::kLayoutHintItemGrid] =
      TLayoutPtr(new ItemGridLayout());

    layoutDelegates_[PlaylistModel::kLayoutHintItemGridCell] =
      TLayoutPtr(new ItemGridCellLayout());

    root_.reset(new Item("playlist"));
    Item & root = *root_;
    const IPlaylistViewStyle & style = *style_;

    // setup an invisible item so its height property expression
    // could be computed once and the result reused in other places
    // that need to compute the same property expression:
    Item & titleHeight = root.addNewHidden<Item>("title_height");
    titleHeight.height_ =
      titleHeight.addExpr(new CalcTitleHeight(root, 24.0));

    // generate an x-button texture:
    {
      QImage img = xbuttonImage(32, style.fg_hint_);
      root.addHidden<Texture>(new Texture("xbutton_texture", img));
    }

    Rectangle & background = root.addNew<Rectangle>("background");
    background.anchors_.fill(root);
    background.color_ = ColorRef::constant(style.bg_);

    Scrollview & sview = root.addNew<Scrollview>("scrollview");

    Item & filterItem = root.addNew<Item>("filterItem");
    filterItem.anchors_.left_ = ItemRef::reference(root, kPropertyLeft);
    filterItem.anchors_.top_ = ItemRef::reference(root, kPropertyTop);
    filterItem.width_ = ItemRef::reference(root, kPropertyWidth);
    filterItem.height_ = ItemRef::scale(titleHeight, kPropertyHeight, 4.5);


    Item & scrollbar = root.addNew<Item>("scrollbar");
    scrollbar.anchors_.right_ = ItemRef::reference(root, kPropertyRight);
    scrollbar.anchors_.top_ = ItemRef::reference(sview, kPropertyTop);
    scrollbar.anchors_.bottom_ = ItemRef::offset(root, kPropertyBottom, -5);
    scrollbar.width_ =
      scrollbar.addExpr(new CalcTitleHeight(root, 50.0), 0.2);

    sview.anchors_.left_ = ItemRef::reference(root, kPropertyLeft);
    sview.anchors_.right_ = ItemRef::reference(scrollbar, kPropertyLeft);
    sview.anchors_.top_ = ItemRef::reference(filterItem, kPropertyBottom);
    sview.anchors_.bottom_ = ItemRef::reference(root, kPropertyBottom);
    sview.margins_.top_ = ItemRef::scale(filterItem, kPropertyHeight, -0.45);

    sview.content_.anchors_.left_ = ItemRef::constant(0.0);
    sview.content_.anchors_.top_ = ItemRef::constant(0.0);
    sview.content_.width_ = ItemRef::reference(sview, kPropertyWidth);

    Item & groups = sview.content_.addNew<Item>("groups");
    groups.anchors_.fill(sview.content_);
    groups.anchors_.bottom_.reset();

    Item & cellWidth = root.addNewHidden<Item>("cell_width");
    cellWidth.width_ = cellWidth.addExpr(new GridCellWidth(groups));

    Item & cellHeight = root.addNewHidden<Item>("cell_height");
    cellHeight.height_ = cellHeight.addExpr(new GridCellHeight(groups));

    Item & fontSize = root.addNewHidden<Item>("font_size");
    fontSize.height_ = fontSize.addExpr(new GetFontSize(titleHeight, 0.52,
                                                          cellHeight, 0.15));

    Text & nowPlaying = root.addNewHidden<Text>("now_playing");
    nowPlaying.font_ = style.font_;
    nowPlaying.anchors_.top_ = ItemRef::constant(0.0);
    nowPlaying.anchors_.left_ = ItemRef::constant(0.0);
    nowPlaying.text_ = TVarRef::constant(TVar(QObject::tr("NOW PLAYING")));
    nowPlaying.font_.setBold(false);
    nowPlaying.fontSize_ = ItemRef::scale(fontSize,
                                          kPropertyHeight,
                                          0.8 * kDpiScale);

    // add a footer:
    Item & footer = sview.content_.addNew<Item>("footer");
    footer.anchors_.left_ =
      ItemRef::offset(sview.content_, kPropertyLeft, 2);
    footer.anchors_.right_ =
      ItemRef::reference(sview.content_, kPropertyRight);
    footer.anchors_.top_ = ItemRef::reference(groups, kPropertyBottom);
    footer.height_ = ItemRef::scale(titleHeight, kPropertyHeight, 4.5);

    FlickableArea & maScrollview =
      sview.add(new FlickableArea("ma_sview", *this, scrollbar));
    maScrollview.anchors_.fill(sview);

    InputArea & maScrollbar = scrollbar.addNew<InputArea>("ma_scrollbar");
    maScrollbar.anchors_.fill(scrollbar);

    // configure scrollbar slider:
    RoundRect & slider = scrollbar.addNew<RoundRect>("slider");
    slider.anchors_.top_ = slider.addExpr(new CalcSliderTop(sview, slider));
    slider.anchors_.left_ = ItemRef::offset(scrollbar, kPropertyLeft, 2);
    slider.anchors_.right_ = ItemRef::offset(scrollbar, kPropertyRight, -2);
    slider.height_ = slider.addExpr(new CalcSliderHeight(sview, slider));
    slider.radius_ = ItemRef::scale(slider, kPropertyWidth, 0.5);
    slider.color_ = ColorRef::constant(style.separator_);

    SliderDrag & maSlider =
      slider.add(new SliderDrag("ma_slider", *this, sview, scrollbar));
    maSlider.anchors_.fill(slider);
  }

  //----------------------------------------------------------------
  // PlaylistView::setModel
  //
  void
  PlaylistView::setModel(PlaylistModelProxy * model)
  {
    if (model_ == model)
    {
      return;
    }

    // FIXME: disconnect previous model:
    YAE_ASSERT(!model_);

    model_ = model;

    // connect new model:
    bool ok = true;

    ok = connect(model_, SIGNAL(dataChanged(const QModelIndex &,
                                            const QModelIndex &)),
                 this, SLOT(dataChanged(const QModelIndex &,
                                        const QModelIndex &)));
    YAE_ASSERT(ok);

    ok = connect(model_, SIGNAL(layoutAboutToBeChanged()),
                 this, SLOT(layoutAboutToBeChanged()));
    YAE_ASSERT(ok);

    ok = connect(model_, SIGNAL(layoutChanged()),
                 this, SLOT(layoutChanged()));
    YAE_ASSERT(ok);

    ok = connect(model_, SIGNAL(modelAboutToBeReset()),
                 this, SLOT(modelAboutToBeReset()));
    YAE_ASSERT(ok);

    ok = connect(model_, SIGNAL(modelReset()),
                 this, SLOT(modelReset()));
    YAE_ASSERT(ok);

    ok = connect(model_, SIGNAL(rowsAboutToBeInserted(const QModelIndex &,
                                                      int, int)),
                 this, SLOT(rowsAboutToBeInserted(const QModelIndex &,
                                                  int, int)));
    YAE_ASSERT(ok);

    ok = connect(model_, SIGNAL(rowsInserted(const QModelIndex &,
                                             int, int)),
                 this, SLOT(rowsInserted(const QModelIndex &,
                                         int, int)));
    YAE_ASSERT(ok);

    ok = connect(model_, SIGNAL(rowsAboutToBeRemoved(const QModelIndex &,
                                                     int, int)),
                 this, SLOT(rowsAboutToBeRemoved(const QModelIndex &,
                                                 int, int)));
    YAE_ASSERT(ok);

    ok = connect(model_, SIGNAL(rowsRemoved(const QModelIndex &,
                                            int, int)),
                 this, SLOT(rowsRemoved(const QModelIndex &,
                                        int, int)));
    YAE_ASSERT(ok);

    const IPlaylistViewStyle & style = *style_;
    Item & root = *root_;
    Item & filterItem = root["filterItem"];
    filterItem.children_.clear();

    QModelIndex rootIndex = model_->index(-1, -1);
    layoutPlaylistFilter(filterItem, *this, *model_, rootIndex, style);

    Scrollview & sview = root.get<Scrollview>("scrollview");
    Item & footer = sview.content_["footer"];
    layoutPlaylistFooter(footer, *this, *model_, rootIndex, style);

    TextInput & filterEdit =
      root["filterItem"]["bg"].get<TextInput>("filter_edit");

    ok = connect(&filterEdit, SIGNAL(textChanged(const QString &)),
                 model_, SLOT(setItemFilter(const QString &)));
    YAE_ASSERT(ok);
  }

  //----------------------------------------------------------------
  // PlaylistView::paint
  //
  void
  PlaylistView::paint(Canvas * canvas)
  {
    // avoid stalling flickable animation:
    {
      Item & root = *root_;
      Scrollview & sview = root.get<Scrollview>("scrollview");
      FlickableArea & ma_sview = sview.get<FlickableArea>("ma_sview");
      ma_sview.onTimeout();
    }

    ItemView::paint(canvas);
  }

  //----------------------------------------------------------------
  // SelectionFlags
  //
  typedef QItemSelectionModel::SelectionFlags SelectionFlags;

  //----------------------------------------------------------------
  // select_items
  //
  static void
  select_items(PlaylistModelProxy & model,
               int groupRow,
               int itemRow,
               SelectionFlags selectionFlags)
  {
    model.selectItems(groupRow, itemRow, selectionFlags);
    model.setCurrentItem(groupRow, itemRow);
  }

  //----------------------------------------------------------------
  // scroll
  //
  static void
  scroll(PlaylistView & view, int key)
  {
    Item & root = *(view.root());
    Scrollview & sview = root.get<Scrollview>("scrollview");
    Item & scrollbar = root["scrollbar"];

    double h_scene = sview.content_.height();
    double h_view = sview.height();

    double range = (h_view < h_scene) ? (h_scene - h_view) : 0.0;
    if (range <= 0.0)
    {
      YAE_ASSERT(false);
      return;
    }

    double y_view = range * sview.position_;

    if (key == Qt::Key_PageUp)
    {
      y_view -= h_view;
      y_view = std::max<double>(y_view, 0.0);
    }
    else if (key == Qt::Key_PageDown)
    {
      y_view += h_view;
      y_view = std::min<double>(y_view, range);
    }
    else if (key == Qt::Key_Home)
    {
      y_view = 0.0;
    }
    else if (key == Qt::Key_End)
    {
      y_view = range;
    }

    sview.position_ = y_view / range;
    scrollbar.uncache();
    view.delegate()->requestRepaint();
  }

  //----------------------------------------------------------------
  // ensure_visible
  //
  static void
  ensure_visible(PlaylistView & view, int groupRow, int itemRow)
  {
    if (groupRow < 0)
    {
      // nothing to do:
      return;
    }

    Item & root = *(view.root());
    Scrollview & sview = root.get<Scrollview>("scrollview");
    Item & scrollbar = root["scrollbar"];
    Item & footer = sview.content_["footer"];
    Item & groups = sview.content_["groups"];

    if (groups.children_.size() <= groupRow)
    {
      return;
    }

    Item & group = *(groups.children_[groupRow]);
    Item & grid = group["payload"]["grid"];

    bool groupOnly = (itemRow < 0 || grid.children_.size() <= itemRow);
    Item & item = groupOnly ? group : *(grid.children_[itemRow]);

    Item & spacer = group["spacer"];
    double h_header = spacer.height();

    if (!groupOnly)
    {
      Item & title = group["title"];
      h_header += title.height();
    }

    double h_footer = footer.height();
    double h_scene = sview.content_.height();
    double h_view = sview.height();

    double range = (h_view < h_scene) ? (h_scene - h_view) : 0.0;
    if (range <= 0.0)
    {
      return;
    }

    double view_y0 = range * sview.position_;
    double view_y1 = view_y0 + h_view - h_footer;

    double h_item = groupOnly ? (h_view - h_footer) : item.height();
    double item_y0 = item.top();
    double item_y1 = item_y0 + h_item;

    if (item_y0 < view_y0 + h_header)
    {
      sview.position_ = (item_y0 - h_header) / range;
      sview.position_ = std::min<double>(1.0, sview.position_);
    }
    else if (item_y1 > view_y1)
    {
      sview.position_ = (item_y1 - (h_view - h_footer)) / range;
      sview.position_ = std::max<double>(0.0, sview.position_);
    }
    else
    {
      return;
    }

    scrollbar.uncache();
    view.delegate()->requestRepaint();
  }

  //----------------------------------------------------------------
  // TMoveCursor
  //
  typedef void(*TMoveCursor)(PlaylistView & view,
                             PlaylistModelProxy & model,
                             int & groupRow,
                             int & itemRow);

  //----------------------------------------------------------------
  // move_cursor
  //
  static void
  move_cursor(PlaylistView & view,
              SelectionFlags selectionFlags,
              TMoveCursor funcMoveCursor)
  {
    PlaylistModelProxy * model = view.model();
    if (!model)
    {
      return;
    }

    QModelIndex currentIndex = model->currentItem();
    int groupRow = -1;
    int itemRow = -1;
    PlaylistModelProxy::mapToGroupRowItemRow(currentIndex, groupRow, itemRow);

    if (itemRow < 0)
    {
      return;
    }

    if (selectionFlags == QItemSelectionModel::SelectCurrent)
    {
      // set the selection anchor, if not already set:
      select_items(*model, groupRow, itemRow, selectionFlags);
    }

    funcMoveCursor(view, *model, groupRow, itemRow);

    currentIndex = model->makeModelIndex(groupRow, itemRow);
    model->setCurrentItem(currentIndex);

    // scroll to current item:
    ensure_visible(view, groupRow, itemRow);

    select_items(*model, groupRow, itemRow, selectionFlags);
  }

  //----------------------------------------------------------------
  // move_cursor_left
  //
  static void
  move_cursor_left(PlaylistView & view,
                   PlaylistModelProxy & model,
                   int & groupRow,
                   int & itemRow)
  {
    if (itemRow > 0)
    {
      itemRow--;
    }
    else if (groupRow > 0)
    {
      groupRow--;
      const int groupSize = model.rowCount(model.makeModelIndex(groupRow, -1));
      itemRow = groupSize - 1;
    }
  }

  //----------------------------------------------------------------
  // move_cursor_right
  //
  static void
  move_cursor_right(PlaylistView & view,
                    PlaylistModelProxy & model,
                    int & groupRow,
                    int & itemRow)
  {
    const int groupSize = model.rowCount(model.makeModelIndex(groupRow, -1));
    const int numGroups = model.rowCount(model.makeModelIndex(-1, -1));

    if (itemRow + 1 < groupSize)
    {
      itemRow++;
    }
    else if (groupRow + 1 < numGroups)
    {
      groupRow++;
      itemRow = 0;
    }
  }

  //----------------------------------------------------------------
  // get_items_per_row
  //
  static int
  get_items_per_row(PlaylistView & view)
  {
    Item & root = *(view.root());
    Scrollview & sview = root.get<Scrollview>("scrollview");
    double gridWidth = sview.width();
    return calcItemsPerRow(gridWidth);
  }

  //----------------------------------------------------------------
  // move_cursor_up
  //
  static void
  move_cursor_up(PlaylistView & view,
                 PlaylistModelProxy & model,
                 int & groupRow,
                 int & itemRow)
  {
    const int itemsPerRow = get_items_per_row(view);

    if (itemRow >= itemsPerRow)
    {
      itemRow -= itemsPerRow;
    }
    else if (itemRow > 0)
    {
      itemRow = 0;
    }
    else if (groupRow > 0)
    {
      groupRow--;
      const int groupSize = model.rowCount(model.makeModelIndex(groupRow, -1));
      itemRow = groupSize - 1;
    }
  }

  //----------------------------------------------------------------
  // move_cursor_down
  //
  static void
  move_cursor_down(PlaylistView & view,
                   PlaylistModelProxy & model,
                   int & groupRow,
                   int & itemRow)
  {
    const int groupSize = model.rowCount(model.makeModelIndex(groupRow, -1));
    const int numGroups = model.rowCount(model.makeModelIndex(-1, -1));
    const int itemsPerRow = get_items_per_row(view);

    if (itemRow + itemsPerRow < groupSize)
    {
      itemRow += itemsPerRow;
    }
    else if (itemRow + 1 < groupSize)
    {
      itemRow = groupSize - 1;
    }
    else if (groupRow + 1 < numGroups)
    {
      groupRow++;
      itemRow = 0;
    }
  }

  //----------------------------------------------------------------
  // get_selection_flags
  //
  static SelectionFlags
  get_selection_flags(QInputEvent * e)
  {
    SelectionFlags f = QItemSelectionModel::ClearAndSelect;

    if (e->modifiers() & Qt::ControlModifier)
    {
      f = QItemSelectionModel::ToggleCurrent;
    }
    else if (e->modifiers() & Qt::ShiftModifier)
    {
      f = QItemSelectionModel::SelectCurrent;
    }

    return f;
  }

  //----------------------------------------------------------------
  // PlaylistView::processKeyEvent
  //
  bool
  PlaylistView::processKeyEvent(Canvas * canvas, QKeyEvent * e)
  {
    e->ignore();

    if (!model_)
    {
      return false;
    }

    QEvent::Type et = e->type();
    if (et == QEvent::KeyPress)
    {
      int key = e->key();

      if (key == Qt::Key_Left ||
          key == Qt::Key_Right ||
          key == Qt::Key_Up ||
          key == Qt::Key_Down ||
          key == Qt::Key_PageUp ||
          key == Qt::Key_PageDown ||
          key == Qt::Key_Home ||
          key == Qt::Key_End)
      {
        SelectionFlags selectionFlags = get_selection_flags(e);

        if (key == Qt::Key_Left)
        {
          move_cursor(*this, selectionFlags, &move_cursor_left);
        }
        else if (key == Qt::Key_Right)
        {
          move_cursor(*this, selectionFlags, &move_cursor_right);
        }
        else if (key == Qt::Key_Up)
        {
          move_cursor(*this, selectionFlags, &move_cursor_up);
        }
        else if (key == Qt::Key_Down)
        {
          move_cursor(*this, selectionFlags, &move_cursor_down);
        }
        else if (key == Qt::Key_PageUp ||
                 key == Qt::Key_PageDown ||
                 key == Qt::Key_Home ||
                 key == Qt::Key_End)
        {
          scroll(*this, key);
        }

        e->accept();
      }
      else if (key == Qt::Key_Return ||
               key == Qt::Key_Enter)
      {
        QModelIndex currentIndex = model_->currentItem();
        model_->setPlayingItem(currentIndex);
        e->accept();
      }
    }

    return e->isAccepted();
  }

  //----------------------------------------------------------------
  // findModelInputArea
  //
  static const TModelInputArea *
  findModelInputArea(const std::list<InputHandler> & inputHandlers)
  {
    for (TInputHandlerCRIter i = inputHandlers.rbegin();
         i != inputHandlers.rend(); ++i)
    {
      const InputHandler & handler = *i;

      const TModelInputArea * modelInput =
        dynamic_cast<const TModelInputArea *>(handler.input_);

      if (modelInput)
      {
        return modelInput;
      }
    }

    return NULL;
  }

  //----------------------------------------------------------------
  // PlaylistView::processMouseEvent
  //
  bool
  PlaylistView::processMouseEvent(Canvas * canvas, QMouseEvent * e)
  {
    bool processed = ItemView::processMouseEvent(canvas, e);

    QEvent::Type et = e->type();
    if (et == QEvent::MouseButtonPress &&
        (e->button() == Qt::LeftButton) &&
        !inputHandlers_.empty())
    {
      const TModelInputArea * ia = findModelInputArea(inputHandlers_);
      if (ia)
      {
        QModelIndex index = ia->modelIndex();
        int groupRow = -1;
        int itemRow = -1;
        PlaylistModelProxy::mapToGroupRowItemRow(index, groupRow, itemRow);

        if (!(groupRow < 0 || itemRow < 0))
        {
          // update selection:
          SelectionFlags selectionFlags = get_selection_flags(e);
          select_items(*model_, groupRow, itemRow, selectionFlags);
        }
      }
    }

    return processed;
  }

  //----------------------------------------------------------------
  // PlaylistView::resizeTo
  //
  void
  PlaylistView::resizeTo(const Canvas * canvas)
  {
    ItemView::resizeTo(canvas);

    if (model_)
    {
      QModelIndex currentIndex = model_->currentItem();
      ensureVisible(currentIndex);
    }
  }

  //----------------------------------------------------------------
  // PlaylistView::ensureVisible
  //
  void
  PlaylistView::ensureVisible(const QModelIndex & itemIndex)
  {
    TMakeCurrentContext currentContext(*context());
    int groupRow = -1;
    int itemRow = -1;
    PlaylistModelProxy::mapToGroupRowItemRow(itemIndex, groupRow, itemRow);
    ensure_visible(*this, groupRow, itemRow);
  }

  //----------------------------------------------------------------
  // PlaylistView::dataChanged
  //
  void
  PlaylistView::dataChanged(const QModelIndex & topLeft,
                            const QModelIndex & bottomRight)
  {
#if 0 // ndef NDEBUG
    std::cerr
      << "PlaylistView::dataChanged, topLeft: " << toString(topLeft)
      << ", bottomRight: " << toString(bottomRight)
      << std::endl;
#endif

    requestRepaint();
  }

  //----------------------------------------------------------------
  // PlaylistView::layoutAboutToBeChanged
  //
  void
  PlaylistView::layoutAboutToBeChanged()
  {
#if 0 // ndef NDEBUG
    std::cerr << "PlaylistView::layoutAboutToBeChanged" << std::endl;
#endif
  }

  //----------------------------------------------------------------
  // PlaylistView::layoutChanged
  //
  void
  PlaylistView::layoutChanged()
  {
#if 0 // ndef NDEBUG
    std::cerr << "PlaylistView::layoutChanged" << std::endl;
#endif

    QModelIndex rootIndex = model_->index(-1, -1);
    TLayoutPtr delegate = findLayoutDelegate(*this, *model_, rootIndex);
    if (!delegate)
    {
      return;
    }

    TMakeCurrentContext currentContext(*context());

    if (pressed_)
    {
      if (dragged_)
      {
        dragged_->input_->onCancel();
        dragged_ = NULL;
      }

      pressed_->input_->onCancel();
      pressed_ = NULL;
    }
    inputHandlers_.clear();

    const IPlaylistViewStyle & style = *style_;
    Item & root = *root_;
    root.anchors_.left_ = ItemRef::constant(0.0);
    root.anchors_.top_ = ItemRef::constant(0.0);
    root.width_ = ItemRef::constant(w_);
    root.height_ = ItemRef::constant(h_);

    Scrollview & sview = root.get<Scrollview>("scrollview");
    Item & scrollbar = root["scrollbar"];
    Item & groups = sview.content_["groups"];
    groups.children_.clear();
    sview.content_.uncache();
    sview.uncache();
    scrollbar.uncache();

    delegate->layout(groups, *this, *model_, rootIndex, style);

#if 0 // ndef NDEBUG
    root.dump(std::cerr);
#endif
  }

  //----------------------------------------------------------------
  // PlaylistView::modelAboutToBeReset
  //
  void
  PlaylistView::modelAboutToBeReset()
  {
#if 0 // ndef NDEBUG
    std::cerr << "PlaylistView::modelAboutToBeReset" << std::endl;
#endif
  }

  //----------------------------------------------------------------
  // PlaylistView::modelReset
  //
  void
  PlaylistView::modelReset()
  {
#if 0 // ndef NDEBUG
    std::cerr << "PlaylistView::modelReset" << std::endl;
#endif
    layoutChanged();
  }

  //----------------------------------------------------------------
  // PlaylistView::rowsAboutToBeInserted
  //
  void
  PlaylistView::rowsAboutToBeInserted(const QModelIndex & parent,
                                      int start, int end)
  {
#if 0 // ndef NDEBUG
    std::cerr
      << "PlaylistView::rowsAboutToBeInserted, parent: " << toString(parent)
      << ", start: " << start << ", end: " << end
      << std::endl;
#endif
  }

  //----------------------------------------------------------------
  // PlaylistView::rowsInserted
  //
  void
  PlaylistView::rowsInserted(const QModelIndex & parent, int start, int end)
  {
#if 0 // ndef NDEBUG
    std::cerr
      << "PlaylistView::rowsInserted, parent: " << toString(parent)
      << ", start: " << start << ", end: " << end
      << std::endl;
#endif

    TMakeCurrentContext currentContext(*context());
    const IPlaylistViewStyle & style = *style_;
    Item & root = *root_;
    Scrollview & sview = root.get<Scrollview>("scrollview");
    Item & scrollbar = root["scrollbar"];
    Item & groups = sview.content_["groups"];

    if (parent.isValid())
    {
      // adding group items:
      const Item & cellWidth = root["cell_width"];
      const Item & cellHeight = root["cell_height"];

      int groupIndex = parent.row();

      TPlaylistModelItem & group =
        dynamic_cast<TPlaylistModelItem &>(*(groups.children_[groupIndex]));

      Item & grid = group["payload"]["grid"];

#ifndef NDEBUG
      // FIXME: just double checking that I am not inserting duplicates:
      {
        int nitems = grid.children_.size();
        for (int i = 0; i < nitems; i++)
        {
          TPlaylistModelItem & cell =
            dynamic_cast<TPlaylistModelItem &>(*(grid.children_[i]));

          int rowIndex = cell.modelIndex().row();
          YAE_ASSERT(rowIndex < start || rowIndex > end);
        }
      }
#endif

      for (int i = start; i <= end; i++)
      {
        QModelIndex childIndex = model_->index(i, 0, parent);
        TPlaylistModelItem & cell =
          grid.insert(i, new TPlaylistModelItem("cell", childIndex));
        layoutPlaylistItem(grid,
                           cell,
                           cellWidth,
                           cellHeight,
                           *this,
                           *model_,
                           childIndex,
                           style);
      }
    }
    else
    {
      // adding item groups:
      for (int i = start; i <= end; i++)
      {
        QModelIndex childIndex = model_->index(i, 0, parent);
        TPlaylistModelItem & group =
          groups.insert(i, new TPlaylistModelItem("group", childIndex));
        layoutPlaylistGroup(groups, group, *this, *model_, childIndex, style);
      }
    }

    sview.content_.uncache();
    sview.uncache();
    scrollbar.uncache();
  }

  //----------------------------------------------------------------
  // PlaylistView::rowsAboutToBeRemoved
  //
  void
  PlaylistView::rowsAboutToBeRemoved(const QModelIndex & parent,
                                     int start, int end)
  {
#if 0 // ndef NDEBUG
    std::cerr
      << "PlaylistView::rowsAboutToBeRemoved, parent: " << toString(parent)
      << ", start: " << start << ", end: " << end
      << std::endl;
#endif

    TMakeCurrentContext currentContext(*context());
    Item & root = *root_;
    Scrollview & sview = root.get<Scrollview>("scrollview");
    Item & scrollbar = root["scrollbar"];
    Item & groups = sview.content_["groups"];

    if (parent.isValid())
    {
      // removing group items:
      int groupIndex = parent.row();

      TPlaylistModelItem & group =
        dynamic_cast<TPlaylistModelItem &>(*(groups.children_[groupIndex]));

      Item & grid = group["payload"]["grid"];
      YAE_ASSERT(start >= 0 && end < int(grid.children_.size()));

      for (int i = end; i >= start; i--)
      {
        grid.children_.erase(grid.children_.begin() + i);
      }
    }
    else
    {
      // removing item groups:
      YAE_ASSERT(start >= 0 && end < int(groups.children_.size()));

      for (int i = end; i >= start; i--)
      {
        groups.children_.erase(groups.children_.begin() + i);
      }
    }

    sview.content_.uncache();
    sview.uncache();
    scrollbar.uncache();
  }

  //----------------------------------------------------------------
  // PlaylistView::rowsRemoved
  //
  void
  PlaylistView::rowsRemoved(const QModelIndex & parent, int start, int end)
  {
#if 0 // ndef NDEBUG
    std::cerr
      << "PlaylistView::rowsRemoved, parent: " << toString(parent)
      << ", start: " << start << ", end: " << end
      << std::endl;
#endif

    requestRepaint();
  }

}