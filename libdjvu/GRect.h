// Copyright [2022] Jan Reggie Dela Cruz
// Copyright [2002] Léon Bottou and Yann Le Cun.
// Copyright [2001] AT&T
// Copyright [1999-2001] LizardTech, Inc.

#ifndef LIBDJVU_GRECT_H_
#define LIBDJVU_GRECT_H_
#ifdef HAVE_CONFIG_H
#include "./config.h"
#endif
#if NEED_GNUG_PRAGMAS
#pragma interface
#endif

/** @name GRect.h
    Files #"GRect.h"# and #"GRect.cpp"# implement basic operations on
    rectangles. Class \Ref{GRect} is used to represent rectangles.  Class
    \Ref{GRectMapper} represent the correspondence between points relative to
    given rectangles.  Class \Ref{GRatio} is used to represent scaling factors
    as rational numbers.
    @memo
    Rectangle manipulation class.
    @author L\'eon Bottou <leonb@research.att.com> -- initial implementation.
*/
//@{

#include <utility>

#include "DjVuGlobal.h"

namespace DJVU {

/** @name Point Coordinates vs. Pixel Coordinates

    The DjVu technology relies on the accurate superposition of images at
    different resolutions.  Such an accuracy cannot be reached with the usual
    assumption that pixels are small enough to be considered infinitesimally
    small.  We must distinguish very precisely ``points'' and ``pixels''.
    This distinction is essential for performing scaling operations.

    The pixels of an image are identified by ``pixel coordinates''.  The
    bottom-left corner pixel has coordinates #(0,0)# and the top-right corner
    pixel has coordinates #(w-1,h-1)# where #w# and #h# are the image size.
    Pixel coordinates are necessarily integers since pixels never overlap.

    An infinitesimally small point is identified by its ``point coordinates''.
    There may be fractional point coordinates, although this library does not
    make use of them.  Points with integer coordinates are located {\em on the
    corners of each pixel}.  They are not located on the pixel centers.  The
    center of the pixel with pixel coordinates #(i,j)# is located at point
    coordinates #(i+1/2,j+1/2)#.  In other words, the pixel #(i,j)# extends
    from point #(i,j)# to point #(i+1,j+1)#.

    Therefore, the point located on the bottom left corner of an image has
    coordinates #(0,0)#.  This point is in fact the bottom left corner of the
    bottom left pixel of the image.  The point located on the top right corner
    of an image has coordinates #(w,h)# where #w# and #h# are the image size.
    This is in fact the top right corner of pixel #(w-1,h-1)# which is the
    image pixel with the highest coordinates.
*/
//@{
//@}

/** Rectangle class.  Each instance of this class represents a rectangle whose
    sides are parallel to the axis. Such a rectangle represents all the points
    whose coordinates lies between well defined minimal and maximal values.
    Member functions can combine several rectangles by computing the
    intersection of rectangles (\Ref{intersect}) or the smallest rectangle
    enclosing two rectangles (\Ref{recthull}).  */
class DJVUAPI GRect {
 public:
  /// Constructs an empty rectangle
  GRect();
  /// Constructs a rectangle given its minimal coordinates #xmin# and #ymin#,
  /// and its measurements #width# and #height#.
  /// Setting #width# or #height# to zero produces an empty rectangle.
  GRect(int xmin, int ymin, unsigned int width = 0, unsigned int height = 0);

  /// Returns the rectangle width.
  int width() const;
  /// Returns the rectangle height.
  int height() const;
  /// Returns the area of the rectangle.
  int area() const;
  /// Returns true if the rectangle is empty.
  bool isempty() const;
  /// Returns true if the rectangle contains pixel (#x#,#y#).
  /// A rectangle contains all pixels with horizontal pixel coordinates
  /// in range #xmin# (inclusive) to #xmax# (exclusive)
  /// and vertical coordinates #ymin# (inclusive) to #ymax# (exclusive).
  int contains(int x, int y) const;
  /// Returns true if this rectangle contains the passed rectangle #rect#.
  /// The function basically checks that the intersection of this rectangle
  /// with #rect# is #rect#.
  int contains(const GRect &rect) const;
  /// Returns true if rectangles #r1# and #r2# are equal.
  friend bool operator==(const GRect &r1, const GRect &r2);
  /// Returns true if rectangles #r1# and #r2# are not equal.
  friend bool operator!=(const GRect &r1, const GRect &r2);
  /** Resets the rectangle to the empty rectangle */
  void clear();
  /// Fatten the rectangle.
  /// Both vertical sides of the rectangle are pushed apart by #dx# units.
  /// Both horizontal sides of the rectangle are pushed apart by #dy# units.
  /// Setting arguments #dx# (resp. #dy#) to a negative value
  /// reduces the rectangle horizontal (resp. vertical) size.
  int inflate(int dx, int dy);
  /** Translate the rectangle. The new rectangle is composed of all the points
      of the old rectangle translated by #dx# units horizontally and #dy#
      units vertically. */
  int translate(int dx, int dy);
  /** Sets the rectangle to the intersection of rectangles #rect1# and #rect2#.
      This function returns true if the intersection rectangle is not empty. */
  int intersect(const GRect &rect1, const GRect &rect2);
  /** Sets the rectangle to the smallest rectangle containing the points of
      both rectangles #rect1# and #rect2#. This function returns true if the
      created rectangle is not empty. */
  int recthull(const GRect &rect1, const GRect &rect2);
  /** Multiplies xmin, ymin, xmax, ymax by factor and scales the rectangle*/
  void scale(float factor);
  /** Multiplies xmin, xmax by xfactor and ymin, ymax by yfactor and scales the
   * rectangle*/
  void scale(float xfactor, float yfactor);

 public:
  /** Minimal horizontal point coordinate of the rectangle. */
  int xmin_;
  /** Minimal vertical point coordinate of the rectangle. */
  int ymin_;
  /** Maximal horizontal point coordinate of the rectangle. */
  int xmax_;
  /** Maximal vertical point coordinate of the rectangle. */
  int ymax_;
};

/** Maps points from one rectangle to another rectangle.  This class
    represents a relation between the points of two rectangles. Given the
    coordinates of a point in the first rectangle (input rectangle), function
    \Ref{map} computes the coordinates of the corresponding point in the
    second rectangle (the output rectangle).  This function actually implements
    an affine transform which maps the corners of the first rectangle onto the
    matching corners of the second rectangle. The scaling operation is
    performed using integer fraction arithmetic in order to maximize
    accuracy. */
class DJVUAPI GRectMapper {
 public:
  /** Constructs a rectangle mapper. */
  GRectMapper();
  /** Resets the rectangle mapper state. Both the input rectangle
      and the output rectangle are marked as undefined. */
  void clear();
  /** Sets the input rectangle. */
  void set_input(const GRect &rect);
  /** Returns the input rectangle. */
  GRect get_input();
  /** Sets the output rectangle. */
  void set_output(const GRect &rect);
  /** Returns the output rectangle. */
  GRect get_output();
  /** Composes the affine transform with a rotation of #count# quarter turns
      counter-clockwise.  This operation essentially is a modification of the
      match between the corners of the input rectangle and the corners of the
      output rectangle. */
  void rotate(int count = 1);
  /** Composes the affine transform with a symmetry with respect to the
      vertical line crossing the center of the output rectangle.  This
      operation essentially is a modification of the match between the corners
      of the input rectangle and the corners of the output rectangle. */
  void mirrorx();
  /** Composes the affine transform with a symmetry with respect to the
      horizontal line crossing the center of the output rectangle.  This
      operation essentially is a modification of the match between the corners
      of the input rectangle and the corners of the output rectangle. */
  void mirrory();
  /** Maps a point according to the affine transform.  Variables #x# and #y#
      initially contain the coordinates of a point. This operation overwrites
      these variables with the coordinates of a second point located in the
      same position relative to the corners of the output rectangle as the
      first point relative to the matching corners of the input rectangle.
      Coordinates are rounded to the nearest integer. */
  std::pair<int, int> map(int x, int y);
  /** Maps a rectangle according to the affine transform. This operation
      consists in mapping the rectangle corners and reordering the corners
     in the canonical rectangle representation.  Variable #rect# is
     overwritten with the new rectangle coordinates. */
  GRect map(const GRect &rect);
  /** Maps a point according to the inverse of the affine transform.
      Variables #x# and #y# initially contain the coordinates of a point. This
      operation overwrites these variables with the coordinates of a second
      point located in the same position relative to the corners of input
      rectangle as the first point relative to the matching corners of the
      input rectangle. Coordinates are rounded to the nearest integer. */
  std::pair<int, int> unmap(int x, int y);
  /** Maps a rectangle according to the inverse of the affine transform. This
      operation consists in mapping the rectangle corners and reordering the
      corners in the canonical rectangle representation.  Variable #rect# is
      overwritten with the new rectangle coordinates. */
  GRect unmap(const GRect &rect);

 public:
  // GRatio
  struct GRatio {
    GRatio();
    GRatio(int p, int q);
    int p;
    int q;
  };

 private:
  // Data
  GRect rectFrom;
  GRect rectTo;
  int code;
  // Helper
  void precalc();
  friend int operator*(int n, GRatio r);
  friend int operator/(int n, GRatio r);
  GRatio rw;
  GRatio rh;
};

//@}

// ---- INLINES

inline GRect::GRect() : xmin_(0), ymin_(0), xmax_(0), ymax_(0) {}

inline GRect::GRect(int xmin, int ymin, unsigned int width, unsigned int height)
    : xmin_(xmin), ymin_(ymin), xmax_(xmin + width), ymax_(ymin + height) {}

inline int GRect::width() const { return xmax_ - xmin_; }

inline int GRect::height() const { return ymax_ - ymin_; }

inline bool GRect::isempty() const { return (xmin_ >= xmax_ || ymin_ >= ymax_); }

inline int GRect::area() const {
  return isempty() ? 0 : (xmax_ - xmin_) * (ymax_ - ymin_);
}

inline int GRect::contains(int x, int y) const {
  return (x >= xmin_ && x < xmax_ && y >= ymin_ && y < ymax_);
}

inline void GRect::clear() { xmin_ = xmax_ = ymin_ = ymax_ = 0; }

inline bool operator!=(const GRect &r1, const GRect &r2) { return !(r1 == r2); }

// ---- THE END

}  // namespace DJVU
#endif  // LIBDJVU_GRECT_H_
