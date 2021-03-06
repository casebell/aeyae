// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// File         : the_wacom_device.hxx
// Author       : Pavel Aleksandrovich Koshevoy
// Created      : 2001/06/24 16:47:24
// Copyright    : (C) 2001, 2002, 2003
// License      : MIT
// Description  : wacom tablet device abstraction classes.

#ifndef THE_WACOM_DEVICE_HXX_
#define THE_WACOM_DEVICE_HXX_

// local includes:
#include "eh/the_input_device.hxx"
#include "eh/the_input_device_event.hxx"
#include "math/v3x1p3x1.hxx"


//----------------------------------------------------------------
// the_wacom_device_t
//
class the_wacom_device_t : public the_input_device_t
{
public:
  the_wacom_device_t();

  // update the device:
  void update(const the_wacom_event_t & e);

  // unique id of the wacom tool that generated the event:
  inline const unsigned long int & tool_id() const
  { return tool_id_; }

  // tool location:
  inline const p2x1_t & scs_pt() const
  { return scs_pt_; }

  // pen/eraser pressure:
  inline float pressure() const
  { return pressure_; }

protected:
  // wacom tool ID (unique):
  unsigned long int tool_id_;

  // tool location expressed in the screen coordinate system:
  p2x1_t scs_pt_;

  // pen/eraser pressure:
  float pressure_;
};


//----------------------------------------------------------------
// the_wacom_stylus_t
//
class the_wacom_stylus_t : public the_wacom_device_t
{
public:
  the_wacom_stylus_t():
    the_wacom_device_t(),
    tilt_(0, 0)
  {}

  // update the device:
  void update(const the_wacom_event_t & e)
  {
    the_wacom_device_t::update(e);
    tilt_ = e.tilt();
  }

  // accessor to the pen/erase tilt vector:
  inline const p2x1_t & tilt() const
  { return tilt_; }

protected:
  // pen/erase tilt vector:
  p2x1_t tilt_;
};


//----------------------------------------------------------------
// the_wacom_cursor_t
//
class the_wacom_cursor_t : public the_wacom_device_t
{
public:
  the_wacom_cursor_t():
    the_wacom_device_t(),
    rotation_(0.0)
  {}

  // update the device:
  void update(const the_wacom_event_t & e)
  {
    the_wacom_device_t::update(e);
    rotation_ = e.rotation();
  }

  // accessor to the cursor Z-rotation (orientation) value:
  inline float rotation() const
  { return rotation_; }

protected:
  // cursor orientation:
  float rotation_;
};


//----------------------------------------------------------------
// the_wacom_t
//
class the_wacom_t
{
public:
  the_wacom_t() {}

  void update(const the_wacom_event_t & e);

  // device accessors:
  inline const the_wacom_stylus_t & pencil() const { return pencil_; }
  inline const the_wacom_stylus_t & eraser() const { return eraser_; }
  inline const the_wacom_cursor_t & cursor() const { return cursor_; }

private:
  the_wacom_stylus_t pencil_;
  the_wacom_stylus_t eraser_;
  the_wacom_cursor_t cursor_;
};


// shortcuts to the input devices:
#define THE_WACOM THE_TRAIL.wacom()


#endif // THE_WACOM_DEVICE_HXX_
