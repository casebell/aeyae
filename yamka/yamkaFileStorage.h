// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created   : Sat Apr 10 12:48:32 MDT 2010
// Copyright : Pavel Koshevoy
// License   : MIT -- http://www.opensource.org/licenses/mit-license.php

#ifndef YAMKA_FILE_STORAGE_H_
#define YAMKA_FILE_STORAGE_H_

// yamka includes:
#include <yamkaIStorage.h>
#include <yamkaFile.h>


namespace Yamka
{

  //----------------------------------------------------------------
  // FileStorage
  //
  struct FileStorage : public IStorage
  {
    FileStorage(const std::string & pathUTF8 = std::string(),
                File::AccessMode fileMode = File::kReadWrite);

    // virtual:
    IReceiptPtr receipt() const;

    // virtual:
    IReceiptPtr save(const unsigned char * data, std::size_t size);
    IReceiptPtr load(unsigned char * data, std::size_t size);
    std::size_t peek(unsigned char * data, std::size_t size);
    uint64      skip(uint64 numBytes);

    // virtual: must override this as the base class implementation
    // simply throws a runtime exception.
    //
    // NOTE: this will throw an exception if the seek fails:
    void        seekTo(uint64 absolutePosition);

    //----------------------------------------------------------------
    // Receipt
    //
    struct Receipt : public IReceipt
    {
      Receipt(const File & file);

      // virtual:
      uint64 position() const;

      // virtual:
      uint64 numBytes() const;

      // virtual:
      Receipt & setNumBytes(uint64 numBytes);

      // virtual:
      Receipt & add(uint64 numBytes);

      // virtual:
      bool save(const unsigned char * data, std::size_t size);
      bool load(unsigned char * data);

      // virtual:
      bool calcCrc32(Crc32 & computeCrc32, const IReceiptPtr & receiptSkip);

      // virtual:
      IReceiptPtr receipt(uint64 offset, uint64 size) const;

    protected:
      File file_;
      TFileOffset addr_;
      uint64 numBytes_;
    };

    // file used to store data:
    File file_;
  };

}


#endif // YAMKA_FILE_STORAGE_H_
