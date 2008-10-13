/*
Copyright 2004-2007 University of Utah

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


// File         : the_qt_mutex.hxx
// Author       : Pavel Aleksandrovich Koshevoy
// Created      : Sun Feb 18 16:12:00 MST 2007
// Copyright    : (C) 2007
// License      : MIT
// Description  : A Qt mutex wrapper class.

#ifndef THE_QT_MUTEX_HXX_
#define THE_QT_MUTEX_HXX_

// Qt includes:
#include <QMutex>

// local includes:
#include "thread/the_mutex_interface.hxx"


//----------------------------------------------------------------
// the_qt_mutex_t
// 
class the_qt_mutex_t : public QMutex, public the_mutex_interface_t
{
protected:
  the_qt_mutex_t();
  
  // the destructor is protected on purpose,
  // see delete_this for details:
  virtual ~the_qt_mutex_t();
  
public:
  // In order to avoid memory management problems with shared libraries,
  // whoever provides this interface instance (via it's creator), has to
  // provide a way to delete the instance as well.  This will avoid
  // issues with multiple-instances of C runtime libraries being
  // used by the app and whatever libraries it links against that
  // either use or provide this interface:
  virtual void delete_this();
  
  // the creation method:
  static the_mutex_interface_t * create();
  
  // virtual:
  void lock();
  
  // virtual:
  void unlock();
  
  // virtual:
  bool try_lock();
};


#endif // THE_QT_MUTEX_HXX_
