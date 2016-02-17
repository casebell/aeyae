// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created      : Tue Oct 20 19:19:59 PDT 2015
// Copyright    : Pavel Koshevoy
// License      : MIT -- http://www.opensource.org/licenses/mit-license.php

// local interfaces:
#include "yaeInputArea.h"


namespace yae
{

  //----------------------------------------------------------------
  // InputArea::InputArea
  //
  InputArea::InputArea(const char * id, bool draggable):
    Item(id),
    draggable_(draggable)
  {}

  //----------------------------------------------------------------
  // InputArea::getInputHandlers
  //
  void
  InputArea::getInputHandlers(// coordinate system origin of
                              // the input area, expressed in the
                              // coordinate system of the root item:
                              const TVec2D & itemCSysOrigin,

                              // point expressed in the coord.sys. of the item,
                              // rootCSysPoint = itemCSysOrigin + itemCSysPoint
                              const TVec2D & itemCSysPoint,

                              // pass back input areas overlapping above point,
                              // along with its coord. system origin expressed
                              // in the coordinate system of the root item:
                              std::list<InputHandler> & inputHandlers)
  {
    if (!Item::overlaps(itemCSysPoint))
    {
      return;
    }

    inputHandlers.push_back(InputHandler(this, itemCSysOrigin));
  }

  //----------------------------------------------------------------
  // InputArea::onCancel
  //
  void
  InputArea::onCancel()
  {}

  //----------------------------------------------------------------
  // InputArea::onDragEnd
  //
  bool
  InputArea::onDragEnd(const TVec2D & itemCSysOrigin,
                       const TVec2D & rootCSysDragStart,
                       const TVec2D & rootCSysDragEnd)
  {
    return this->onDrag(itemCSysOrigin,
                        rootCSysDragStart,
                        rootCSysDragEnd);
  }
}
