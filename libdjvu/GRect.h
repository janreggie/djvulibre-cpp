// Copyright [2022] Jan Reggie Dela Cruz
// Copyright [2002] Léon Bottou and Yann Le Cun.
// Copyright [2001] AT&T
// Copyright [1999-2001] LizardTech, Inc.

#ifndef LIBDJVU_GRECT_H_
#define LIBDJVU_GRECT_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// \file GRect.h
///
/// Files "GRect.h" and "GRect.cpp" implement basic operations on rectangles.
/// Class \Ref{GRect} is used to represent rectangles.
/// Class \Ref{GRectMapper} represent the correspondence between points
/// relative to given rectangles.
/// Class \Ref{GRatio} is used to represent scaling factors as rational numbers.

#include <utility>

#include "DjVuGlobal.h"

namespace DJVU {

/// \class GRect
/// \brief Rectangle class
///
/// Each instance of this class represents a rectangle whose sides are parallel
/// to the axis. Such a rectangle represents all the points whose coordinates
/// lies between well defined minimal and maximal values. Member functions can
/// combine several rectangles by computing the intersection of rectangles
/// (\Ref{intersect}) or the smallest rectangle enclosing two rectangles
/// (\Ref{recthull}).
class DJVUAPI GRect {
 public:
  /// Constructs an empty rectangle
  GRect();
  /// Constructs a rectangle given its minimal coordinates #xmin# and #ymin#,
  /// and its measurements #width# and #height#.
  /// Setting #width# or #height# to zero produces an empty rectangle.
  GRect(int xmin, int ymin, unsigned int width = 0, unsigned int height = 0);

  int width() const;
  int height() const;
  int area() const;
  bool isempty() const;
  /// Returns true if the rectangle contains pixel (#x#,#y#).
  /// A rectangle contains all pixels with horizontal pixel coordinates
  /// in range #xmin# (inclusive) to #xmax# (exclusive)
  /// and vertical coordinates #ymin# (inclusive) to #ymax# (exclusive).
  bool contains(int x, int y) const;
  /// Returns true if this rectangle contains the passed rectangle #rect#.
  /// The function basically checks that the intersection of this rectangle
  /// with #rect# is #rect#.
  bool contains(const GRect &rect) const;
  /// Returns true if rectangles #r1# and #r2# are equal.
  friend bool operator==(const GRect &r1, const GRect &r2);
  /// Returns true if rectangles #r1# and #r2# are not equal.
  friend bool operator!=(const GRect &r1, const GRect &r2);
  /// Resets the rectangle to the empty rectangle
  void clear();
  /// Fatten the rectangle.
  /// Both vertical sides of the rectangle are pushed apart by #dx# units.
  /// Both horizontal sides of the rectangle are pushed apart by #dy# units.
  /// Setting arguments #dx# (resp. #dy#) to a negative value
  /// reduces the rectangle horizontal (resp. vertical) size.
  /// Returns whether the resulting rectangle is non-empty.
  bool inflate(int dx, int dy);
  /// Translate the rectangle.
  /// The new rectangle is composed of all the points of the old rectangle
  /// translated by #dx# units horizontally and #dy# units vertically.
  /// Returns whether the resulting rectangle is non-empty.
  bool translate(int dx, int dy);
  /// Sets the rectangle to the intersection of rectangles #rect1# and #rect2#.
  /// Returns whether the resulting rectangle is not-empty.
  bool intersect(const GRect &rect1, const GRect &rect2);
  /// Sets the rectangle to the smallest rectangle
  /// containing the points of both rectangles #rect1# and #rect2#.
  /// Returns whether the resulting rectangle is not-empty.
  bool recthull(const GRect &rect1, const GRect &rect2);
  /// Multiplies xmin, ymin, xmax, ymax by factor and scales the rectangle
  ///
  /// TODO: Convert to double
  void scale(float factor);
  /// Multiplies xmin, xmax by xfactor and ymin, ymax by yfactor
  /// and scales the rectangle
  void scale(float xfactor, float yfactor);

 public:
  int xmin_;
  int ymin_;
  int xmax_;
  int ymax_;
};

/// \class GRectMapper
/// \brief Maps points from one rectangle to another rectangle.
///
/// This class represents a relation between the points of two rectangles.
/// Given the coordinates of a point in the first rectangle (input rectangle),
/// function \Ref{map} computes the coordinates of the corresponding point in
/// the second rectangle (the output rectangle).
/// This function actually implements an affine transform which maps the corners
/// of the first rectangle onto the matching corners of the second rectangle.
/// The scaling operation is performed using integer fraction arithmetic in
/// order to maximize accuracy.
class DJVUAPI GRectMapper {
 public:
  /// Constructs a rectangle mapper.
  GRectMapper();
  /// Resets the rectangle mapper state.
  /// Both input and output rectangles are marked as undefined.
  void clear();
  void set_input(const GRect &rect);
  GRect get_input();
  void set_output(const GRect &rect);
  GRect get_output();
  /// Composes the affine transform with a rotation of #count# quarter turns
  /// counter-clockwise.  This operation essentially is a modification of the
  /// match between the corners of the input rectangle and the corners of the
  /// output rectangle.
  void rotate(int count = 1);
  /// Composes the affine transform with a symmetry with respect to the
  /// vertical line crossing the center of the output rectangle.  This
  /// operation essentially is a modification of the match between the corners
  /// of the input rectangle and the corners of the output rectangle.
  void mirrorx();
  /// Composes the affine transform with a symmetry with respect to the
  /// horizontal line crossing the center of the output rectangle.  This
  /// operation essentially is a modification of the match between the corners
  /// of the input rectangle and the corners of the output rectangle.
  void mirrory();
  /// Maps a point according to the affine transform.
  /// Variables #x# and #y# initially contain the coordinates of a point.
  /// This operation returns another point whose coordinates are located in the
  /// same position relative to the corners of the output rectangle as the
  /// first point relative to the matching corners of the input rectangle.
  /// Coordinates are rounded to the nearest integer.
  std::pair<int, int> map(int x, int y);
  /// Maps a rectangle according to the affine transform.
  /// This operation consists in mapping the rectangle corners, reordering the
  /// corners in the canonical rectangle representation, and returning it.
  GRect map(const GRect &rect);
  /// Maps a point according to the inverse of the affine transform.
  /// Variables #x# and #y# initially contain the coordinates of a point.
  /// This operation returns another point whose coordinates are located in the
  /// same position relative to the corners of the input rectangle as the
  /// first point relative to the matching corners of the output rectangle.
  /// Coordinates are rounded to the nearest integer.
  std::pair<int, int> unmap(int x, int y);
  /// Maps a rectangle according to the inverse of the affine transform.
  /// This operation consists in mapping the rectangle corners, reordering the
  /// corners in the canonical rectangle representation, and returning it.
  GRect unmap(const GRect &rect);

 public:
  // GRatio represents a rational number.
  struct GRatio {
    GRatio();
    GRatio(int p, int q);
    int p;
    int q;
  };

 private:
  // Data
  GRect rectFrom_;
  GRect rectTo_;
  unsigned char code_;

  // code_ is a bit array encoding the following transforms:
  static constexpr unsigned char MIRRORX = 1, MIRRORY = 2, SWAPXY = 4;

  // Helper
  void precalc();
  friend int operator*(int n, GRatio r);
  friend int operator/(int n, GRatio r);
  GRatio rw_;
  GRatio rh_;
};

// ---- INLINES

inline GRect::GRect() : xmin_(0), ymin_(0), xmax_(0), ymax_(0) {}

inline GRect::GRect(int xmin, int ymin, unsigned int width, unsigned int height)
    : xmin_(xmin), ymin_(ymin), xmax_(xmin + width), ymax_(ymin + height) {}

inline int GRect::width() const { return xmax_ - xmin_; }

inline int GRect::height() const { return ymax_ - ymin_; }

inline bool GRect::isempty() const {
  return (xmin_ >= xmax_ || ymin_ >= ymax_);
}

inline int GRect::area() const {
  return isempty() ? 0 : (xmax_ - xmin_) * (ymax_ - ymin_);
}

inline bool GRect::contains(int x, int y) const {
  return (x >= xmin_ && x < xmax_ && y >= ymin_ && y < ymax_);
}

inline void GRect::clear() { xmin_ = xmax_ = ymin_ = ymax_ = 0; }

inline bool operator!=(const GRect &r1, const GRect &r2) { return !(r1 == r2); }

}  // namespace DJVU
#endif  // LIBDJVU_GRECT_H_
