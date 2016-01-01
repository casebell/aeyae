// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created      : Tue Oct 20 19:19:59 PDT 2015
// Copyright    : Pavel Koshevoy
// License      : MIT -- http://www.opensource.org/licenses/mit-license.php

#ifndef YAE_TEXTURED_RECT_H_
#define YAE_TEXTURED_RECT_H_

// local interfaces:
#include "yaeTexture.h"


namespace yae
{

  //----------------------------------------------------------------
  // TexturedRect
  //
  class TexturedRect : public Item
  {
    TexturedRect(const TexturedRect &);
    TexturedRect & operator = (const TexturedRect &);

  public:
    TexturedRect(const char * id, const Texture & texture);

    // virtual:
    void paintContent() const;

    const Texture & texture_;
  };

}


#endif // YAE_TEXTURED_RECT_H_