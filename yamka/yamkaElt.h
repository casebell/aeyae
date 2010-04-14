// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created   : Sat Apr 10 12:54:55 MDT 2010
// Copyright : Pavel Koshevoy
// License   : MIT -- http://www.opensource.org/licenses/mit-license.php

#ifndef YAMKA_ELT_H_
#define YAMKA_ELT_H_

// yamka includes:
#include <yamkaIStorage.h>
#include <yamkaStdInt.h>
#include <yamkaFileStorage.h>

// system includes:
#include <deque>
#include <stdexcept>


//----------------------------------------------------------------
// THROW_ARG2_IF_FALSE
// 
# ifndef THROW_ARG2_IF_FALSE
# define THROW_ARG2_IF_FALSE(predicate, arg2)   \
  if (predicate) {} else throw arg2
#endif

//----------------------------------------------------------------
// THROW_ERR_IF_FALSE
// 
# ifndef THROW_ERR_IF_FALSE
# define THROW_ERR_IF_FALSE(predicate, errStr)   \
  if (predicate) {} else throw std::runtime_error(std::string(errStr))
#endif


namespace Yamka
{
  //----------------------------------------------------------------
  // EbmlGlobalID
  // 
  // Void and CRC-32 elements may occur at any level in an EBML
  // document, therefore every element should know these IDs.
  // 
  enum EbmlGlobalID
  {
    kIdCrc32 = 0xBF,
    kIdVoid = 0xEC
  };
  
  //----------------------------------------------------------------
  // TElt
  //
  // This is a template class representing an EBML element
  // The template is parametarized by
  // 
  //   element payload type,
  //   unique element ID
  //   element name type that provides T::getName() API
  // 
  template <typename payload_t,
            unsigned int EltId,
            typename elt_name_t>
  struct TElt
  {
    typedef payload_t TPayload;
    typedef TElt<TPayload, EltId, elt_name_t> TSelf;
    
    static unsigned int id()
    { return EltId; }
    
    static const char * name()
    { return elt_name_t::getName(); }
    
    TElt():
      alwaysSave_(false),
      computeCrc32_(false),
      checksumCrc32_(0)
    {}
    
    TSelf & enableCrc32(bool enable)
    {
      computeCrc32_ = true;
      return *this;
    }
    
    uint64 calcSize() const
    {
      if (!mustSave())
      {
        return 0;
      }
      
      // the payload size:
      uint64 payloadSize = payload_.calcSize();
      
      // the EBML ID, payload size descriptor, and payload size:
      uint64 size =
        uintNumBytes(EltId) +
        vsizeNumBytes(payloadSize) +
        payloadSize;
      
      if (computeCrc32_)
      {
        // CRC-32 element size:
        size +=
          uintNumBytes(kIdCrc32) +
          vsizeNumBytes(4) +
          4;
      }
      
      return size;
    }
    
    IStorage::IReceiptPtr
    save(IStorage & storage, Crc32 * parentCrc32 = NULL) const
    {
      if (!mustSave())
      {
        return storage.receipt();
      }
      
      if (computeCrc32_)
      {
        Bytes bytes;
        bytes << uintEncode(kIdCrc32)
              << vsizeEncode(4)
              << uintEncode(0, 4);
        
        receiptCrc32_ = storage.save(bytes);
        if (!receiptCrc32_)
        {
          return receiptCrc32_;
        }
      }
      
      Bytes bytes;
      bytes << uintEncode(EltId);
      
      Crc32 eltCrc32;
      Crc32 * crc32 = computeCrc32_ ? &eltCrc32 : parentCrc32;
      
      receipt_ = storage.saveAndCalcCrc32(bytes, crc32);
      if (!receipt_)
      {
        return receipt_;
      }
      
      receiptPayload_ = payload_.save(storage, crc32);
      if (!receiptPayload_)
      {
        return receiptPayload_;
      }
      
      if (computeCrc32_)
      {
        checksumCrc32_ = eltCrc32.checksum();
        return receiptCrc32_;
      }
      
      return receipt_;
    }
    
    uint64
    load(FileStorage & storage, uint64 storageSize, Crc32 * crc = NULL)
    {
      // save current seek position, so it can be restored if necessary:
      File::Seek storageStart(storage.file_);
      
      // keep track of the number of bytes read successfully:
      uint64 bytesRead = 0;
      Bytes bytesCrc32;
      
      uint64 eltId = loadEbmlId(storage, crc);
      if (eltId == kIdCrc32)
      {
        uint64 vsize = vsizeDecode(storage, crc);
        if (vsize != 4)
        {
          // wrong CRC-32 payload size:
          return 0;
        }
        
        bytesCrc32 = Bytes(4);
        if (!storage.loadAndCalcCrc32(bytesCrc32))
        {
          // failed to load CRC-32 checksum:
          return 0;
        }
        
        // move on to the real element:
        bytesRead += (uintNumBytes(eltId) +
                      uintNumBytes(vsize) +
                      4);
        
        eltId = loadEbmlId(storage, crc);
      }
      
      if (eltId != id())
      {
        // element id wrong for my type:
        return 0;
      }
      
      // this appears to be a good payload:
      storageStart.doNotRestore();
      
      // store the checksum:
      if (!bytesCrc32.empty())
      {
        computeCrc32_ = true;
        checksumCrc32_ = (unsigned int)uintDecode(TByteVec(bytesCrc32), 4);
      }
      
      bytesRead += uintNumBytes(eltId);
      uint64 payloadSize = payload_.load(storage,
                                         storageSize - bytesRead,
                                         crc);
      bytesRead += payloadSize;
      return bytesRead;
    }
    
    // check whether this element payload holds a default value:
    bool mustSave() const
    {
      return alwaysSave_ || !payload_.isDefault();
    }
    
    // set the flag indicating that this element must be saved
    // even when it holds a default value:
    TSelf & alwaysSave()
    {
      alwaysSave_ = true;
      return *this;
    }
    
    // this flag indicates that this element must be saved
    // even when it holds a default value:
    bool alwaysSave_;
    
    // the contents of this element:
    TPayload payload_;
    
    // The CRC32 container can be placed around any EBML element or
    // elements. The value stored in CRC32Value is the result of the
    // CRC-32 [CRC32] checksum performed on the other child elements.
    // 
    // CRC32 := c3 container [ level:1..; card:*; ] {
    //   %children;
    //   CRC32Value := 42fe binary [ size:4; ]
    // }
    bool computeCrc32_;
    
    // loaded/computed CRC-32 checksum:
    mutable unsigned int checksumCrc32_;
    
    // storage receipt for crc32 and this element:
    mutable IStorage::IReceiptPtr receiptCrc32_;
    mutable IStorage::IReceiptPtr receipt_;
    mutable IStorage::IReceiptPtr receiptPayload_;
  };
  
  
  //----------------------------------------------------------------
  // TElts
  // 
  template <typename payload_t,
            unsigned int EltId,
            typename elt_name_t>
  struct TElts : public std::deque<TElt<payload_t, EltId, elt_name_t> >
  {};

  //----------------------------------------------------------------
  // 
  // NOTE: Currently (2010) C++ templates can not be parameterized
  // with const string literal constants:
  // 
  //   TElt<int, 0x1, "One"> one; // does not compiles
  //
  // 
  // Therefore I added helper C preprocessor macro(s) that
  // works around this limitation by creating a wrapper class
  // to return the string literal, and passing the wrapper
  // class as a template parameter to TElt:
  //
  //   struct EltName0x1 { static const char * getName() { return "One" } };
  //   TElt<int, 0x1, EltName0x1> one; // compiles just fine
  //
  // 
  // The wrapper macros are used like this:
  // 
  //   Elt(int, 0x1, "One") one;
  //   Elts(int, 0x2, "Two") twos;
  //
  
  //----------------------------------------------------------------
  // Elt
  // 
  // Helper macro used to declare an element.
  // 
  // EXAMPLE: Elt(VUInt, 0x4286, "EBMLVersion") version_;
  // 
# define Elt(EltType, EbmlId, Name)                                    \
  struct EltName##EbmlId { static const char * getName() { return Name; } }; \
  TElt<EltType, EbmlId, EltName##EbmlId>
  
  //----------------------------------------------------------------
  // Elts
  // 
  // Helper macro used to declare an element list
  // 
  // EXAMPLE: Elts(VUInt, 0xEC, "Void") voids_;
  //       
# define Elts(EltType, EbmlId, Name)                                   \
  struct EltName##EbmlId { static const char * getName() { return Name; } }; \
  TElts<EltType, EbmlId, EltName##EbmlId>

  //----------------------------------------------------------------
  // TypeOfElt
  // 
# define TypeOfElt(EltType, EbmlId, Name)       \
  TElt<EltType, EbmlId, EltName##EbmlId>

  //----------------------------------------------------------------
  // TypeOfElts
  // 
# define TypeOfElts(EltType, EbmlId, Name)      \
  TElts<EltType, EbmlId, EltName##EbmlId>
  
}


#endif // YAMKA_ELT_H_
