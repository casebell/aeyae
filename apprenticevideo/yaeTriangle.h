// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created      : Tue Oct 20 19:19:59 PDT 2015
// Copyright    : Pavel Koshevoy
// License      : MIT -- http://www.opensource.org/licenses/mit-license.php

#ifndef YAE_TRIANGLE_H_
#define YAE_TRIANGLE_H_

// local interfaces:
#include "yaeItem.h"


namespace yae
{

  //----------------------------------------------------------------
  // Triangle
  //
  struct Triangle : public Item
  {
    Triangle(const char * id);

    // virtual:
    void uncache();

    // virtual:
    void paintContent() const;

    BoolRef collapsed_;
    ItemRef border_;
    ColorRef color_;
    ColorRef colorBorder_;
  };

}


#endif // YAE_TRIANGLE_H_
