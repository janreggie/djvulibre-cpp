// Copyright [2022] Jan Reggie Dela Cruz
// Copyright [2002] Leon Bottou and Yann Le Cun.
// Copyright [2001] AT&T
// Copyright [1999-2001] LizardTech, Inc.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "Arrays.h"
#include "GException.h"

namespace DJVU {

ArrayRep::ArrayRep(int xelsize, void (*xdestroy)(void *, int, int),
                   void (*xinit1)(void *, int, int),
                   void (*xinit2)(void *, int, int, const void *, int, int),
                   void (*xcopy)(void *, int, int, const void *, int, int),
                   void (*xinsert)(void *, int, int, const void *, int))
    : data_(0),
      minlo_(0),
      maxhi_(-1),
      lobound_(0),
      hibound_(-1),
      elsize_(xelsize),
      destroy(xdestroy),
      init1(xinit1),
      init2(xinit2),
      copy(xcopy),
      insert(xinsert) {}

ArrayRep::ArrayRep(int xelsize, void (*xdestroy)(void *, int, int),
                   void (*xinit1)(void *, int, int),
                   void (*xinit2)(void *, int, int, const void *, int, int),
                   void (*xcopy)(void *, int, int, const void *, int, int),
                   void (*xinsert)(void *, int, int, const void *, int), int hi)
    : data_(0),
      minlo_(0),
      maxhi_(-1),
      lobound_(0),
      hibound_(-1),
      elsize_(xelsize),
      destroy(xdestroy),
      init1(xinit1),
      init2(xinit2),
      copy(xcopy),
      insert(xinsert) {
  resize(0, hi);
}

ArrayRep::ArrayRep(int xelsize, void (*xdestroy)(void *, int, int),
                   void (*xinit1)(void *, int, int),
                   void (*xinit2)(void *, int, int, const void *, int, int),
                   void (*xcopy)(void *, int, int, const void *, int, int),
                   void (*xinsert)(void *, int, int, const void *, int), int lo,
                   int hi)
    : data_(0),
      minlo_(0),
      maxhi_(-1),
      lobound_(0),
      hibound_(-1),
      elsize_(xelsize),
      destroy(xdestroy),
      init1(xinit1),
      init2(xinit2),
      copy(xcopy),
      insert(xinsert) {
  resize(lo, hi);
}

ArrayRep::ArrayRep(const ArrayRep &arr)
    : data_(0),
      minlo_(0),
      maxhi_(-1),
      lobound_(0),
      hibound_(-1),
      elsize_(arr.elsize_),
      destroy(arr.destroy),
      init1(arr.init1),
      init2(arr.init2),
      copy(arr.copy),
      insert(arr.insert) {
  resize(arr.lobound_, arr.hibound_);
  arr.copy(data_, lobound_ - minlo_, hibound_ - minlo_, arr.data_,
           arr.lobound_ - arr.minlo_, arr.hibound_ - arr.minlo_);
}

ArrayRep::~ArrayRep() {
  destroy(data_, lobound_ - minlo_, hibound_ - minlo_);
  operator delete(data_);
  data_ = 0;
}

ArrayRep &ArrayRep::operator=(const ArrayRep &rep) {
  if (&rep == this) return *this;
  empty();
  resize(rep.lobound_, rep.hibound_);
  copy(data_, lobound_ - minlo_, hibound_ - minlo_, rep.data_,
       rep.lobound_ - rep.minlo_, rep.hibound_ - rep.minlo_);
  return *this;
}

void ArrayRep::resize(int lo, int hi) {
  int nsize = hi - lo + 1;
  // Validation
  if (nsize < 0) G_THROW(ERR_MSG("arrays.resize"));
  // Destruction
  if (nsize == 0) {
    destroy(data_, lobound_ - minlo_, hibound_ - minlo_);
    operator delete(data_);
    data_ = 0;
    lobound_ = minlo_ = lo;
    hibound_ = maxhi_ = hi;
    return;
  }
  // Simple extension
  if (lo >= minlo_ && hi <= maxhi_) {
    init1(data_, lo - minlo_, lobound_ - 1 - minlo_);
    destroy(data_, lobound_ - minlo_, lo - 1 - minlo_);
    init1(data_, hibound_ + 1 - minlo_, hi - minlo_);
    destroy(data_, hi + 1 - minlo_, hibound_ - minlo_);
    lobound_ = lo;
    hibound_ = hi;
    return;
  }
  // General case
  int nminlo = minlo_;
  int nmaxhi = maxhi_;
  if (nminlo > nmaxhi) nminlo = nmaxhi = lo;
  while (nminlo > lo) {
    int incr = nmaxhi - nminlo;
    nminlo -= (incr < 8 ? 8 : (incr > 32768 ? 32768 : incr));
  }
  while (nmaxhi < hi) {
    int incr = nmaxhi - nminlo;
    nmaxhi += (incr < 8 ? 8 : (incr > 32768 ? 32768 : incr));
  }
  // allocate
  int bytesize = elsize_ * (nmaxhi - nminlo + 1);
  void *ndata;
  GPBufferBase gndata(ndata, bytesize, 1);
  std::memset(ndata, 0, bytesize);
  // initialize
  init1(ndata, lo - nminlo, lobound_ - 1 - nminlo);
  init2(ndata, lobound_ - nminlo, hibound_ - nminlo, data_, lobound_ - minlo_,
        hibound_ - minlo_);
  init1(ndata, hibound_ + 1 - nminlo, hi - nminlo);
  destroy(data_, lobound_ - minlo_, hibound_ - minlo_);

  // free and replace
  void *tmp = data_;
  data_ = ndata;
  ndata = tmp;

  minlo_ = nminlo;
  maxhi_ = nmaxhi;
  lobound_ = lo;
  hibound_ = hi;
}

void ArrayRep::shift(int disp) {
  lobound_ += disp;
  hibound_ += disp;
  minlo_ += disp;
  maxhi_ += disp;
}

void ArrayRep::del(int n, unsigned int howmany) {
  if (howmany == 0) return;
  if (static_cast<int>(n + howmany) > hibound_ + 1)
    G_THROW(ERR_MSG("arrays.ill_arg"));
  copy(data_, n - minlo_, hibound_ - howmany - minlo_, data_,
       n + howmany - minlo_, hibound_ - minlo_);
  destroy(data_, hibound_ + 1 - howmany - minlo_, hibound_ - minlo_);
  hibound_ = hibound_ - howmany;
}

void ArrayRep::ins(int n, const void *what, unsigned int howmany) {
  int nhi = hibound_ + howmany;
  if (howmany == 0) return;
  if (maxhi_ < nhi) {
    int nmaxhi = maxhi_;
    while (nmaxhi < nhi)
      nmaxhi += (nmaxhi < 8 ? 8 : (nmaxhi > 32768 ? 32768 : nmaxhi));
    int bytesize = elsize_ * (nmaxhi - minlo_ + 1);
    void *ndata;
    GPBufferBase gndata(ndata, bytesize, 1);
    std::memset(ndata, 0, bytesize);
    copy(ndata, lobound_ - minlo_, hibound_ - minlo_, data_, lobound_ - minlo_,
         hibound_ - minlo_);
    destroy(data_, lobound_ - minlo_, hibound_ - minlo_);
    data_ = ndata;
    maxhi_ = nmaxhi;
  }

  insert(data_, hibound_ + 1 - minlo_, n - minlo_, what, howmany);
  hibound_ = nhi;
}

}  // namespace DJVU
using namespace DJVU;

// ---------------------------------------
// BEGIN HACK
// ---------------------------------------
// Included here to avoid dependency
// from ByteStream.o to Arrays.o

#ifndef DO_NOT_MOVE_GET_DATA_TO_ARRAYS_CPP
#include "ByteStream.h"

namespace DJVU {
TArray<char> ByteStream::get_data(void) {
  const int s = size();
  if (s > 0) {
    TArray<char> data(0, s - 1);
    readat((char *)data, s, 0);
    return data;
  } else {
    TArray<char> data(0, -1);
    return data;
  }
}

}  // namespace DJVU
using namespace DJVU;
#endif

// ---------------------------------------
// END HACK
// ---------------------------------------
