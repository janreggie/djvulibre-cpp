// Copyright [2022] Jan Reggie Dela Cruz
// Copyright [2002] Leon Bottou and Yann Le Cun.
// Copyright [2001] AT&T
// Copyright [1999-2001] LizardTech, Inc.

// Files `Arrays.h` and `Arrays.cpp` implement three array template classes.
// Class TArray implements an array of objects of trivial types such as char,
// int, float, etc. It is faster than general implementation for any type
// done in DArray because it does not cope with element's constructors,
// destructors and copy operators. Although implemented as a template, which
// makes it possible to incorrectly use TArray with non-trivial classes,
// it should not be done.
//
// A lot of things is shared by these three arrays. That is why there are more
// base classes:
//
// - ArrayBase defines functions independent of the elements type
// - ArrayBaseT template class defining functions shared by DArray and TArray
//
// The main difference between GArray (now obsolete) and these ones is the
// copy-on-demand strategy, which allows you to copy array objects without
// copying the real data. It's the same thing, which has been implemented in
// GString long ago: as long as you don't try to modify the underlying data, it
// may be shared between several copies of array objects. As soon as you attempt
// to make any changes, a private copy is created automatically and
// transparently for you - the procedure, that we call "copy-on-demand".
//
// Also, please note that now there is no separate class which does fast sort.
// Both TArray (dynamic array for trivial types) and DArray (dynamic array for
// arbitrary types) can sort their elements.
//
//                        Historical comments
//
// Leon chose to implement his own arrays because the STL classes were not
// universally available and the compilers were rarely able to deal with such a
// template galore. Later it became clear that there is no really good reason
// why arrays should be derived from containers. It was also suggested to create
// separate arrays implementation for simple classes and do the copy-on-demand
// strategy, which would allow to assign array objects without immediate copying
// of their elements.
//
// At this point DArray and TArray should only be used when it is critical to
// have the copy-on-demand feature.  The GArray implementation is a lot more
// efficient.

#ifndef LIBDJVU_ARRAYS_H_
#define LIBDJVU_ARRAYS_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
#pragma interface
#endif

#include <cstring>

#include "GException.h"
#include "GSmartPointer.h"

namespace DJVU {

// Auxiliary classes: Will be used in place of GPBase and GPEnabled objects
class _ArrayRep {
  friend class _ArrayBase;

 public:
  _ArrayRep() : count_(0) {}
  _ArrayRep(const _ArrayRep &) {}
  virtual ~_ArrayRep() {}

  _ArrayRep &operator=(const _ArrayRep &) { return *this; }

  int get_count() const { return count_; }

 private:
  // Reference counter
  int count_;
  void ref() { count_++; }
  void unref() {
    if (--count_ == 0) delete this;
  }
};

class _ArrayBase {
 public:
  _ArrayBase() : rep(0) {}
  _ArrayBase(const _ArrayBase &ab) : rep(nullptr) {
    if (ab.rep) ab.rep->ref();
    rep = ab.rep;
  }
  explicit _ArrayBase(_ArrayRep *ar) : rep(nullptr) {
    if (ar) ar->ref();
    rep = ar;
  }
  virtual ~_ArrayBase() {
    if (rep) {
      rep->unref();
      rep = nullptr;
    }
  }

  _ArrayRep *get() const { return rep; }
  _ArrayBase &assign(_ArrayRep *ar) {
    if (ar) ar->ref();
    if (rep) rep->unref();
    rep = ar;
    return *this;
  }
  _ArrayBase &operator=(const _ArrayBase &ab) { return assign(ab.rep); }
  bool operator==(const _ArrayBase &ab) { return rep == ab.rep; }

 private:
  _ArrayRep *rep;
};

// Internal "Array repository" holding the pointer to the actual data, data
// bounds, etc. It copes with data elements with the help of five static
// functions which pointers are supposed to be passed to the constructor.
class DJVUAPI ArrayRep : public _ArrayRep {
 public:
  ArrayRep(int elsize, void (*xdestroy)(void *, int, int),
           void (*xinit1)(void *, int, int),
           void (*xinit2)(void *, int, int, const void *, int, int),
           void (*xcopy)(void *, int, int, const void *, int, int),
           void (*xinsert)(void *, int, int, const void *, int));
  ArrayRep(int elsize, void (*xdestroy)(void *, int, int),
           void (*xinit1)(void *, int, int),
           void (*xinit2)(void *, int, int, const void *, int, int),
           void (*xcopy)(void *, int, int, const void *, int, int),
           void (*xinsert)(void *, int, int, const void *, int), int hibound);
  ArrayRep(int elsize, void (*xdestroy)(void *, int, int),
           void (*xinit1)(void *, int, int),
           void (*xinit2)(void *, int, int, const void *, int, int),
           void (*xcopy)(void *, int, int, const void *, int, int),
           void (*xinsert)(void *, int, int, const void *, int), int lobound,
           int hibound);
  ArrayRep(const ArrayRep &rep);

  virtual ~ArrayRep();

  // Following is the standard interface to DArray.
  // DArray will call these functions to access data.
  int size() const;
  int lbound() const;
  int hbound() const;

  void empty();
  void touch(int n);
  void resize(int lobound, int hibound);
  void shift(int disp);
  void del(int n, unsigned int howmany = 1);

  // ins() is an exception. It does it job only partially.
  // The derived class is supposed to finish insertion.
  void ins(int n, const void *what, unsigned int howmany);

  ArrayRep &operator=(const ArrayRep &rep);

  // All data is public because DArray... classes will need access to it
  void *data_;
  int minlo_;
  int maxhi_;
  int lobound_;
  int hibound_;
  int elsize_;

 private:
  // These functions can't be virtual as they're called from constructors and
  // destructors.

  // destroy(): should destroy elements in data[] array from 'lo' to 'hi'
  void (*destroy)(void *data, int lo, int hi);
  // init1(): should initialize elements in data[] from 'lo' to 'hi'
  // using default constructors
  void (*init1)(void *data, int lo, int hi);
  // init2(): should initialize elements in data[] from 'lo' to 'hi'
  // using corresponding elements from src[] (copy constructor)
  void (*init2)(void *data, int lo, int hi, const void *src, int src_lo,
                int src_hi);
  // copy(): should copy elements from src[] to dst[] (copy operator)
  void (*copy)(void *dst, int dst_lo, int dst_hi, const void *src, int src_lo,
               int src_hi);
  // insert(): should insert '*what' at position 'where' 'howmany' times
  // into array data[] having 'els' initialized elements
  void (*insert)(void *data, int els, int where, const void *what, int howmany);
};

inline int ArrayRep::size() const { return hibound_ - lobound_ + 1; }

inline int ArrayRep::lbound() const { return lobound_; }

inline int ArrayRep::hbound() const { return hibound_; }

inline void ArrayRep::empty() { resize(0, -1); }

inline void ArrayRep::touch(int n) {
  if (hibound_ < lobound_) {
    resize(n, n);
  } else {
    int nlo = lobound_;
    int nhi = hibound_;
    if (n < nlo) nlo = n;
    if (n > nhi) nhi = n;
    resize(nlo, nhi);
  }
}

// ArrayBase is a dynamic array base class.
//
// Warning: This is an auxiliary base class for DArray and TArray
// implementing some shared functions independent of the type of array elements.
// It's not supposed to be constructed by hand.
// Use DArray and TArray instead.
class DJVUAPI ArrayBase : protected _ArrayBase {
 protected:
  void check();
  void detach();

  ArrayBase() {}

 public:
  // Returns the number of elements in the array
  int size() const;
  // Returns the lower bound of the valid subscript range.
  int lbound() const;
  // Returns the upper bound of the valid subscript range.
  int hbound() const;
  // Erases the array contents. All elements in the array are destroyed.
  // The valid subscript range is set to the empty range.
  void empty();

  // Extends the subscript range so that is contains `n`.
  // This function does nothing if `n` is already in the valid subscript range.
  // If the valid range was empty,
  // both the lower bound and the upper bound are set to `n`.
  // Otherwise the valid subscript range is extended to encompass `n`.
  // This function is very handy when called before setting an array element:
  //
  // ```c++
  // int lineno=1;
  // DArray<GString> a;
  // while (! end_of_file()) {
  //   a.touch(lineno);
  //   a[lineno++] = read_a_line();
  // }
  // ```
  void touch(int n);

  // Resets the valid subscript range to `0` to `hibound`.
  // This function may destroy some array elements
  // and may construct new array elements with the null constructor.
  // Setting `hibound` to `-1` resets the valid subscript range to empty.
  void resize(int hibound);

  // Resets the valid subscript range to `lobound` to `hibound`.
  // This function may destroy some array elements
  // and may construct new array elements with the null constructor.
  // resize(0, -1) == resize(-1) == empty().
  void resize(int lobound, int hibound);

  // Shifts the valid subscript range.
  // Argument `disp` is added to both bounds of the valid subscript range.
  // Array elements previously located at subscript `x`
  // will now be located at subscript `x+disp`.
  void shift(int disp);

  // Deletes `howmany` elements starting from `n`.
  // Array elements with subscripts `n` to `n+howmany-1` are destroyed.
  // All array elements previously located at subscripts >= `n+howmany`
  // are moved to subscripts starting with `n`.
  // The new subscript upper bound is reduced to account for this shift.
  void del(int n, unsigned int howmany = 1);

  virtual ~ArrayBase() {}
};

inline void ArrayBase::check() {
  if (get()->get_count() > 1) detach();
}

inline void ArrayBase::detach() {
  ArrayRep *new_rep = new ArrayRep(*(ArrayRep *)get());
  assign(new_rep);
}

inline int ArrayBase::size() const { return ((const ArrayRep *)get())->size(); }

inline int ArrayBase::lbound() const {
  return ((const ArrayRep *)get())->lobound_;
}

inline int ArrayBase::hbound() const {
  return ((const ArrayRep *)get())->hibound_;
}

inline void ArrayBase::empty() {
  check();
  ((ArrayRep *)get())->empty();
}

inline void ArrayBase::resize(int lo, int hi) {
  check();
  ((ArrayRep *)get())->resize(lo, hi);
}

inline void ArrayBase::resize(int hi) { resize(0, hi); }

inline void ArrayBase::touch(int n) {
  check();
  ((ArrayRep *)get())->touch(n);
}

inline void ArrayBase::shift(int disp) {
  check();
  ((ArrayRep *)get())->shift(disp);
}

inline void ArrayBase::del(int n, unsigned int howmany) {
  check();

  ((ArrayRep *)get())->del(n, howmany);
}

// ArrayBaseT implements a dynamic array template base class.
// This is an auxiliary template base class for DArray and TArray
// implementing some shared functions
// which *depend* on the type of the array elements
// (this is contrary to ArrayBase).
//
// Warning: It's not supposed to be constructed by hand.
// Use DArray and TArray instead.
template <class TYPE>
class ArrayBaseT : public ArrayBase {
 public:
  virtual ~ArrayBaseT() {}

  // Returns a reference to the array element for subscript `n`.
  // This reference can be used for reading (`a[n]`) and writing (`a[n]=v`).
  // This operation will not extend the valid subscript range:
  // an exception GException is thrown if argument `n` is outside the range.
  TYPE &operator[](int n);

  // Returns a constant reference to the array element for subscript `n`.
  // This reference can only be used for reading (`a[n]`).
  // This operation will not extend the valid subscript range:
  //  an exception GException is thrown if argument `n` is outside the range.
  // This const method is necessary when dealing with a `const DArray<TYPE>`.
  const TYPE &operator[](int n) const;

  // Returns a pointer for reading or writing the array elements.
  // This pointer can be used to access the array elements with the same
  // subscripts and the usual bracket syntax.  This pointer remains valid
  // as long as the valid subscript range is unchanged.
  // If you change the subscript range, you must stop using the pointers
  // returned by prior invocation of this conversion operator.
  operator TYPE *();

  // Returns a pointer for reading (but not modifying) the array elements.
  // This pointer can be used to access the array elements with the same
  // subscripts and the usual bracket syntax.  This pointer remains valid
  // as long as the valid subscript range is unchanged.
  // If you change the subscript range, you must stop using the pointers
  // returned by prior invocation of this conversion operator.
  operator const TYPE *() const;

  // Insert new elements into an array.
  // This function inserts `howmany` elements at position `n` into the array.
  // The initial value `val` is copied into the new elements.
  // All array elements previously located at subscripts `n` and higher
  // are moved to subscripts `n+howmany` and higher.
  // The upper bound of the valid subscript range is increased
  // in order to account for this shift.
  void ins(int n, const TYPE &val, unsigned int howmany = 1);

  // Sort all array elements in ascending order.
  // Array elements are compared using the <= comparison operator for type TYPE.
  void sort();

  // Sort all array elements whose subscripts are `lo`..`hi` in ascending order.
  // The other elements of the array are left untouched.
  // An exception is thrown if arguments `lo` and `hi` are not valid.
  // Array elements are compared using the <= comparison operator for type TYPE.
  void sort(int lo, int hi);

 protected:
  ArrayBaseT() {}

 private:
  // Callbacks called from ArrayRep
  static void destroy(void *data, int lo, int hi);
  static void init1(void *data, int lo, int hi);
  static void init2(void *data, int lo, int hi, const void *src, int src_lo,
                    int src_hi);
  static void copy(void *dst, int dst_lo, int dst_hi, const void *src,
                   int src_lo, int src_hi);
  static void insert(void *data, int els, int where, const void *what,
                     int howmany);
};

template <class TYPE>
inline ArrayBaseT<TYPE>::operator TYPE *() {
  check();

  ArrayRep *rep = (ArrayRep *)get();
  return &((TYPE *)rep->data_)[-rep->minlo_];
}

template <class TYPE>
inline ArrayBaseT<TYPE>::operator const TYPE *() const {
  const ArrayRep *rep = (const ArrayRep *)get();
  return &((const TYPE *)rep->data_)[-rep->minlo_];
}

template <class TYPE>
inline TYPE &ArrayBaseT<TYPE>::operator[](int n) {
  check();

  ArrayRep *rep = (ArrayRep *)get();
  if (n < rep->lobound_ || n > rep->hibound_)
    G_THROW(ERR_MSG("arrays.ill_sub"));
  return ((TYPE *)rep->data_)[n - rep->minlo_];
}

template <class TYPE>
inline const TYPE &ArrayBaseT<TYPE>::operator[](int n) const {
  const ArrayRep *rep = (const ArrayRep *)get();
  if (n < rep->lobound_ || n > rep->hibound_)
    G_THROW(ERR_MSG("arrays.ill_sub"));
  return ((const TYPE *)rep->data_)[n - rep->minlo_];
}

template <class TYPE>
inline void ArrayBaseT<TYPE>::ins(int n, const TYPE &val,
                                  unsigned int howmany) {
  check();

  ((ArrayRep *)get())->ins(n, &val, howmany);
}

template <class TYPE>
void ArrayBaseT<TYPE>::sort() {
  sort(lbound(), hbound());
}

template <class TYPE>
void ArrayBaseT<TYPE>::sort(int lo, int hi) {
  if (hi <= lo) return;
  // Test for insertion sort (optimize!)
  if (hi <= lo + 20) {
    for (int i = lo + 1; i <= hi; i++) {
      int j = i;
      TYPE tmp = (*this)[i];
      while ((--j >= lo) && !((*this)[j] <= tmp)) (*this)[j + 1] = (*this)[j];
      (*this)[j + 1] = tmp;
    }
    return;
  }
  // -- determine suitable quick-sort pivot
  TYPE tmp = (*this)[lo];
  TYPE pivot = (*this)[(lo + hi) / 2];
  if (pivot <= tmp) {
    tmp = pivot;
    pivot = (*this)[lo];
  }
  if ((*this)[hi] <= tmp) {
    pivot = tmp;
  } else if ((*this)[hi] <= pivot) {
    pivot = (*this)[hi];
  }
  // -- partition set
  int h = hi;
  int l = lo;
  while (l < h) {
    while (!(pivot <= (*this)[l])) l++;
    while (!((*this)[h] <= pivot)) h--;
    if (l < h) {
      tmp = (*this)[l];
      (*this)[l] = (*this)[h];
      (*this)[h] = tmp;
      l = l + 1;
      h = h - 1;
    }
  }
  // -- recursively restart
  sort(lo, h);
  sort(l, hi);
}

// TArray<TYPE> implements an array of elements of *simple* type TYPE.
// Simple means that the type may be char, int, float, etc.
// The limitation is imposed by the way in which TArray works with its elements:
// it does not execute elements' constructors, destructors or copy operators,
// instead just doing bitwise copy.
// Except for this it's pretty much the same as DArray.
//
// Please note that most of the methods are implemented in the base classes
// ArrayBase and ArrayBaseT.
template <class TYPE>
class TArray : public ArrayBaseT<TYPE> {
 public:
  // Constructs an empty array.
  TArray();
  // Constructs an array with subscripts in range 0 to `hibound`.
  explicit TArray(int hibound);
  // Constructs an array with subscripts in range `lobound` to `hibound`.
  TArray(int lobound, int hibound);

  virtual ~TArray() {}

 private:
  // Callbacks called from ArrayRep
  static void destroy(void *data, int lo, int hi);
  static void init1(void *data, int lo, int hi);
  static void init2(void *data, int lo, int hi, const void *src, int src_lo,
                    int src_hi);
  static void insert(void *data, int els, int where, const void *what,
                     int howmany);
};

template <class TYPE>
void TArray<TYPE>::destroy(void *data, int lo, int hi) {}

template <class TYPE>
void TArray<TYPE>::init1(void *data, int lo, int hi) {}

template <class TYPE>
void TArray<TYPE>::init2(void *data, int lo, int hi, const void *src,
                         int src_lo, int src_hi) {
  if (data && src) {
    int els = hi - lo + 1;
    if (els > src_hi - src_lo + 1) els = src_hi - src_lo + 1;
    if (els > 0)
      memmove((void *)&((TYPE *)data)[lo], (void *)&((TYPE *)src)[src_lo],
              els * sizeof(TYPE));
  };
}

// inline removed
template <class TYPE>
void TArray<TYPE>::insert(void *data, int els, int where, const void *what,
                          int howmany) {
  memmove(((TYPE *)data) + where + howmany, ((TYPE *)data) + where,
          sizeof(TYPE) * (els - where));
  for (int i = 0; i < howmany; i++) ((TYPE *)data)[where + i] = *(TYPE *)what;
}

template <class TYPE>
TArray<TYPE>::TArray() {
  this->assign(
      new ArrayRep(sizeof(TYPE), destroy, init1, init2, init2, insert));
}

template <class TYPE>
TArray<TYPE>::TArray(int hi) {
  this->assign(
      new ArrayRep(sizeof(TYPE), destroy, init1, init2, init2, insert, hi));
}

template <class TYPE>
TArray<TYPE>::TArray(int lo, int hi) {
  this->assign(
      new ArrayRep(sizeof(TYPE), destroy, init1, init2, init2, insert, lo, hi));
}

// inline removal ends

// DArray is a dynamic array for general types.
// Template class DArray<TYPE> implements an array of elements of type TYPE.
// Each element is identified by an integer subscript.
// The valid subscripts range is defined by dynamically adjustable lower- and
// upper-bounds. Besides accessing and setting elements, member functions
// are provided to insert or delete elements at specified positions.
//
// This template class must be able to access:
// - a null constructor TYPE::TYPE(),
// - a copy constructor TYPE::TYPE(const TYPE &),
// - and a copy operator TYPE & operator=(const TYPE &).
//
// The class offers "copy-on-demand" policy,
// which means that when you copy the array object, array elements will stay
// intact as long as you don't try to modify them.
// As soon as you make an attempt to change array contents,
// the copying is done automatically and transparently for you:
// the procedure that we call "copy-on-demand".
// This is the main difference between this class and GArray (now obsolete).
//
// Please note that most of the methods
// are implemented in the base classes ArrayBase and ArrayBaseT.
template <class TYPE>
class DArray : public ArrayBaseT<TYPE> {
 public:
  // Constructs an empty array.
  // The valid subscript range is initially empty.
  // The subscript range can be modified with member fxns `touch` and `resize`.
  DArray();
  // Constructs an array with subscripts in range 0 to `hibound`.
  // The subscript range can be modified with member fxns `touch` and `resize`.
  explicit DArray(const int hibound);
  // Constructs an array with subscripts in range `lobound` to `hibound`.
  // The subscript range can be modified with member fxns `touch` and `resize`.
  DArray(const int lobound, const int hibound);

  virtual ~DArray() {}

 private:
  // Callbacks called from ArrayRep
  static void destroy(void *data, int lo, int hi);
  static void init1(void *data, int lo, int hi);
  static void init2(void *data, int lo, int hi, const void *src, int src_lo,
                    int src_hi);
  static void copy(void *dst, int dst_lo, int dst_hi, const void *src,
                   int src_lo, int src_hi);
  static void insert(void *data, int els, int where, const void *what,
                     int howmany);
};

template <class TYPE>
void DArray<TYPE>::destroy(void *data, int lo, int hi) {
  if (data)
    for (int i = lo; i <= hi; i++) ((TYPE *)data)[i].TYPE::~TYPE();
}

template <class TYPE>
void DArray<TYPE>::init1(void *data, int lo, int hi) {
  if (data)
    for (int i = lo; i <= hi; i++) new ((void *)&((TYPE *)data)[i]) TYPE;
}

template <class TYPE>
void DArray<TYPE>::init2(void *data, int lo, int hi, const void *src,
                         int src_lo, int src_hi) {
  if (data && src) {
    int i, j;
    for (i = lo, j = src_lo; i <= hi && j <= src_hi; i++, j++)
      new ((void *)&((TYPE *)data)[i]) TYPE(((TYPE *)src)[j]);
  };
}

template <class TYPE>
void DArray<TYPE>::copy(void *dst, int dst_lo, int dst_hi, const void *src,
                        int src_lo, int src_hi) {
  if (dst && src) {
    int i, j;
    for (i = dst_lo, j = src_lo; i <= dst_hi && j <= src_hi; i++, j++)
      ((TYPE *)dst)[i] = ((TYPE *)src)[j];
  };
}

template <class TYPE>
inline void DArray<TYPE>::insert(void *data, int els, int where,
                                 const void *what, int howmany) {
  // Now do the insertion
  TYPE *d = (TYPE *)data;

  int i;
  for (i = els + howmany - 1; i >= els; i--) {
    if (i - where >= (int)howmany)
      new ((void *)&d[i]) TYPE(d[i - howmany]);
    else
      new ((void *)&d[i]) TYPE(*(TYPE *)what);
  }

  for (i = els - 1; i >= where; i--) {
    if (i - where >= (int)howmany)
      d[i] = d[i - howmany];
    else
      d[i] = *(TYPE *)what;
  }
}

template <class TYPE>
inline DArray<TYPE>::DArray() {
  this->assign(new ArrayRep(sizeof(TYPE), destroy, init1, init2, copy, insert));
}

template <class TYPE>
inline DArray<TYPE>::DArray(const int hi) {
  this->assign(
      new ArrayRep(sizeof(TYPE), destroy, init1, init2, copy, insert, hi));
}

template <class TYPE>
inline DArray<TYPE>::DArray(const int lo, const int hi) {
  this->assign(
      new ArrayRep(sizeof(TYPE), destroy, init1, init2, copy, insert, lo, hi));
}

/** Dynamic array for \Ref{GPBase}d classes.

    There are many situations when it's necessary to create arrays of
    \Ref{GP} pointers. For example, #DArray<GP<Dialog> ># or #DArray<GP<Button>
   >#. This would result in compilation of two instances of \Ref{DArray} because
    from the viewpoint of the compiler there are two different classes used
    as array elements: #GP<Dialog># and #GP<Button>#. In reality though,
    all \Ref{GP} pointers have absolutely the same binary structure because
    they are derived from \Ref{GPBase} class and do not add any variables
    or virtual functions. That's why it's possible to instantiate \Ref{DArray}
    only once for \Ref{GPBase} elements and then just cast types.

    To implement this idea we have created this #DPArray<TYPE># class,
    which can be used instead of #DArray<GP<TYPE> >#. It behaves absolutely
    the same way as \Ref{DArray} but has one big advantage: overhead of
    using #DPArray# with one more type is negligible.
  */
template <class TYPE>
class DPArray : public DArray<GPBase> {
 public:
  // -- CONSTRUCTORS
  DPArray();
  explicit DPArray(int hibound);
  DPArray(int lobound, int hibound);
  DPArray(const DPArray<TYPE> &gc);
  // -- DESTRUCTOR
  virtual ~DPArray();
  // -- ACCESS
  GP<TYPE> &operator[](int n);
  const GP<TYPE> &operator[](int n) const;
  // -- CONVERSION
  operator GP<TYPE> *();
  operator const GP<TYPE> *();
  operator const GP<TYPE> *() const;
  // -- ALTERATION
  void ins(int n, const GP<TYPE> &val, unsigned int howmany = 1);
  DPArray<TYPE> &operator=(const DPArray &ga);
};

template <class TYPE>
DPArray<TYPE>::DPArray() {}

template <class TYPE>
DPArray<TYPE>::DPArray(int hibound) : DArray<GPBase>(hibound) {}

template <class TYPE>
DPArray<TYPE>::DPArray(int lobound, int hibound)
    : DArray<GPBase>(lobound, hibound) {}

template <class TYPE>
DPArray<TYPE>::DPArray(const DPArray<TYPE> &gc) : DArray<GPBase>(gc) {}

template <class TYPE>
DPArray<TYPE>::~DPArray() {}

template <class TYPE>
inline GP<TYPE> &DPArray<TYPE>::operator[](int n) {
  return (GP<TYPE> &)DArray<GPBase>::operator[](n);
}

template <class TYPE>
inline const GP<TYPE> &DPArray<TYPE>::operator[](int n) const {
  return (const GP<TYPE> &)DArray<GPBase>::operator[](n);
}

template <class TYPE>
inline DPArray<TYPE>::operator GP<TYPE> *() {
  return (GP<TYPE> *)DArray<GPBase>::operator GPBase *();
}

template <class TYPE>
inline DPArray<TYPE>::operator const GP<TYPE> *() {
  return (const GP<TYPE> *)DArray<GPBase>::operator const GPBase *();
}

template <class TYPE>
inline DPArray<TYPE>::operator const GP<TYPE> *() const {
  return (const GP<TYPE> *)DArray<GPBase>::operator const GPBase *();
}

template <class TYPE>
inline void DPArray<TYPE>::ins(int n, const GP<TYPE> &val,
                               unsigned int howmany) {
  DArray<GPBase>::ins(n, val, howmany);
}

template <class TYPE>
inline DPArray<TYPE> &DPArray<TYPE>::operator=(const DPArray &ga) {
  DArray<GPBase>::operator=(ga);
  return *this;
}

}  // namespace DJVU
#endif  // LIBDJVU_ARRAYS_H_
