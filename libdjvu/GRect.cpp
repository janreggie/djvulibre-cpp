// C-  -*- C++ -*-
// C- -------------------------------------------------------------------
// C- DjVuLibre-3.5
// C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
// C- Copyright (c) 2001  AT&T
// C-
// C- This software is subject to, and may be distributed under, the
// C- GNU General Public License, either Version 2 of the license,
// C- or (at your option) any later version. The license should have
// C- accompanied the software or you may obtain a copy of the license
// C- from the Free Software Foundation at http://www.fsf.org .
// C-
// C- This program is distributed in the hope that it will be useful,
// C- but WITHOUT ANY WARRANTY; without even the implied warranty of
// C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// C- GNU General Public License for more details.
// C-
// C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
// C- Lizardtech Software.  Lizardtech Software has authorized us to
// C- replace the original DjVu(r) Reference Library notice by the following
// C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
// C-
// C-  ------------------------------------------------------------------
// C- | DjVu (r) Reference Library (v. 3.5)
// C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
// C- | The DjVu Reference Library is protected by U.S. Pat. No.
// C- | 6,058,214 and patents pending.
// C- |
// C- | This software is subject to, and may be distributed under, the
// C- | GNU General Public License, either Version 2 of the license,
// C- | or (at your option) any later version. The license should have
// C- | accompanied the software or you may obtain a copy of the license
// C- | from the Free Software Foundation at http://www.fsf.org .
// C- |
// C- | The computer code originally released by LizardTech under this
// C- | license and unmodified by other parties is deemed "the LIZARDTECH
// C- | ORIGINAL CODE."  Subject to any third party intellectual property
// C- | claims, LizardTech grants recipient a worldwide, royalty-free,
// C- | non-exclusive license to make, use, sell, or otherwise dispose of
// C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the
// C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU
// C- | General Public License.   This grant only confers the right to
// C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to
// C- | the extent such infringement is reasonably necessary to enable
// C- | recipient to make, have made, practice, sell, or otherwise dispose
// C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to
// C- | any greater extent that may be necessary to utilize further
// C- | modifications or combinations.
// C- |
// C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
// C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
// C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
// C- +------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
#pragma implementation
#endif

// -- Implementation of class GRect and GRectMapper
// - Author: Leon Bottou, 05/1997

#include <tuple>

#include "GException.h"
#include "GRect.h"

namespace DJVU {

// -- Local utilities

static inline int imin(int x, int y) {
  if (x < y)
    return x;
  else
    return y;
}

static inline int imax(int x, int y) {
  if (x > y)
    return x;
  else
    return y;
}

// -- Class GRect

bool operator==(const GRect &r1, const GRect &r2) {
  if (r1.isempty() && r2.isempty()) return true;
  return (r1.xmin_ == r2.xmin_ && r1.xmax_ == r2.xmax_ && r1.ymin_ == r2.ymin_ &&
          r1.ymax_ == r2.ymax_);
}

int GRect::inflate(int dx, int dy) {
  xmin_ -= dx;
  xmax_ += dx;
  ymin_ -= dy;
  ymax_ += dy;
  if (!isempty()) return 1;
  xmin_ = ymin_ = xmax_ = ymax_ = 0;
  return 0;
}

int GRect::translate(int dx, int dy) {
  xmin_ += dx;
  xmax_ += dx;
  ymin_ += dy;
  ymax_ += dy;
  if (!isempty()) return 1;
  xmin_ = ymin_ = xmax_ = ymax_ = 0;
  return 0;
}

int GRect::intersect(const GRect &rect1, const GRect &rect2) {
  xmin_ = imax(rect1.xmin_, rect2.xmin_);
  xmax_ = imin(rect1.xmax_, rect2.xmax_);
  ymin_ = imax(rect1.ymin_, rect2.ymin_);
  ymax_ = imin(rect1.ymax_, rect2.ymax_);
  if (!isempty()) return 1;
  xmin_ = ymin_ = xmax_ = ymax_ = 0;
  return 0;
}

int GRect::recthull(const GRect &rect1, const GRect &rect2) {
  if (rect1.isempty()) {
    xmin_ = rect2.xmin_;
    xmax_ = rect2.xmax_;
    ymin_ = rect2.ymin_;
    ymax_ = rect2.ymax_;
    return !isempty();
  }
  if (rect2.isempty()) {
    xmin_ = rect1.xmin_;
    xmax_ = rect1.xmax_;
    ymin_ = rect1.ymin_;
    ymax_ = rect1.ymax_;
    return !isempty();
  }
  xmin_ = imin(rect1.xmin_, rect2.xmin_);
  xmax_ = imax(rect1.xmax_, rect2.xmax_);
  ymin_ = imin(rect1.ymin_, rect2.ymin_);
  ymax_ = imax(rect1.ymax_, rect2.ymax_);
  return 1;
}

int GRect::contains(const GRect &rect) const {
  GRect tmp_rect;
  tmp_rect.intersect(*this, rect);
  return tmp_rect == rect;
}

void GRect::scale(float factor) {
  xmin_ = (int)(((float)xmin_) * factor);
  ymin_ = (int)(((float)ymin_) * factor);
  xmax_ = (int)(((float)xmax_) * factor);
  ymax_ = (int)(((float)ymax_) * factor);
}

void GRect::scale(float xfactor, float yfactor) {
  xmin_ = (int)(((float)xmin_) * xfactor);
  ymin_ = (int)(((float)ymin_) * yfactor);
  xmax_ = (int)(((float)xmax_) * xfactor);
  ymax_ = (int)(((float)ymax_) * yfactor);
}
// -- Class GRatio

inline GRectMapper::GRatio::GRatio() : p(0), q(1) {}

inline GRectMapper::GRatio::GRatio(int p, int q) : p(p), q(q) {
  if (q == 0) G_THROW(ERR_MSG("GRect.div_zero"));
  if (p == 0) q = 1;
  if (q < 0) {
    p = -p;
    q = -q;
  }
  int gcd = 1;
  int g1 = p;
  int g2 = q;
  if (g1 > g2) {
    gcd = g1;
    g1 = g2;
    g2 = gcd;
  }
  while (g1 > 0) {
    gcd = g1;
    g1 = g2 % g1;
    g2 = gcd;
  }
  p /= gcd;
  q /= gcd;
}

#ifdef HAVE_LONG_LONG_INT
#define llint_t long long int
#else
#define llint_t long int
#endif

inline int operator*(int n, GRectMapper::GRatio r) {
  /* [LB] -- This computation is carried out with integers and
     rational numbers because it must be exact.  Some lizard changed
     it to double and this is wrong.  I suspect they did so because
     they encountered overflow issues.  Let's use long long ints. */
  llint_t x = (llint_t)n * (llint_t)r.p;
  if (x >= 0)
    return ((r.q / 2 + x) / r.q);
  else
    return -((r.q / 2 - x) / r.q);
}

inline int operator/(int n, GRectMapper::GRatio r) {
  /* [LB] -- See comment in operator*() above. */
  llint_t x = (llint_t)n * (llint_t)r.q;
  if (x >= 0)
    return ((r.p / 2 + x) / r.p);
  else
    return -((r.p / 2 - x) / r.p);
}

// -- Class GRectMapper

#define MIRRORX 1
#define MIRRORY 2
#define SWAPXY 4

GRectMapper::GRectMapper()
    : rectFrom(0, 0, 1, 1), rectTo(0, 0, 1, 1), code(0) {}

void GRectMapper::clear() {
  rectFrom = GRect(0, 0, 1, 1);
  rectTo = GRect(0, 0, 1, 1);
  code = 0;
}

void GRectMapper::set_input(const GRect &rect) {
  if (rect.isempty()) G_THROW(ERR_MSG("GRect.empty_rect1"));
  rectFrom = rect;
  if (code & SWAPXY) {
    std::swap(rectFrom.xmin_, rectFrom.ymin_);
    std::swap(rectFrom.xmax_, rectFrom.ymax_);
  }
  rw = rh = GRatio();
}

void GRectMapper::set_output(const GRect &rect) {
  if (rect.isempty()) G_THROW(ERR_MSG("GRect.empty_rect2"));
  rectTo = rect;
  rw = rh = GRatio();
}

void GRectMapper::rotate(int count) {
  int oldcode = code;
  switch (count & 0x3) {
    case 1:
      code ^= (code & SWAPXY) ? MIRRORY : MIRRORX;
      code ^= SWAPXY;
      break;
    case 2:
      code ^= (MIRRORX | MIRRORY);
      break;
    case 3:
      code ^= (code & SWAPXY) ? MIRRORX : MIRRORY;
      code ^= SWAPXY;
      break;
  }
  if ((oldcode ^ code) & SWAPXY) {
    std::swap(rectFrom.xmin_, rectFrom.ymin_);
    std::swap(rectFrom.xmax_, rectFrom.ymax_);
    rw = rh = GRatio();
  }
}

void GRectMapper::mirrorx() { code ^= MIRRORX; }

void GRectMapper::mirrory() { code ^= MIRRORY; }

void GRectMapper::precalc() {
  if (rectTo.isempty() || rectFrom.isempty())
    G_THROW(ERR_MSG("GRect.empty_rect3"));
  rw = GRatio(rectTo.width(), rectFrom.width());
  rh = GRatio(rectTo.height(), rectFrom.height());
}

std::pair<int, int> GRectMapper::map(int x, int y) {
  int mx = x;
  int my = y;
  // precalc
  if (!(rw.p && rh.p)) precalc();
  // swap and mirror
  if (code & SWAPXY) std::swap(mx, my);
  if (code & MIRRORX) mx = rectFrom.xmin_ + rectFrom.xmax_ - mx;
  if (code & MIRRORY) my = rectFrom.ymin_ + rectFrom.ymax_ - my;
  // scale and translate
  x = rectTo.xmin_ + (mx - rectFrom.xmin_) * rw;
  y = rectTo.ymin_ + (my - rectFrom.ymin_) * rh;
  return {x, y};
}

std::pair<int, int> GRectMapper::unmap(int x, int y) {
  // precalc
  if (!(rw.p && rh.p)) precalc();
  // scale and translate
  int mx = rectFrom.xmin_ + (x - rectTo.xmin_) / rw;
  int my = rectFrom.ymin_ + (y - rectTo.ymin_) / rh;
  //  mirror and swap
  if (code & MIRRORX) mx = rectFrom.xmin_ + rectFrom.xmax_ - mx;
  if (code & MIRRORY) my = rectFrom.ymin_ + rectFrom.ymax_ - my;
  if (code & SWAPXY) std::swap(mx, my);
  return {mx, my};
}

GRect GRectMapper::map(const GRect &rect) {
  GRect r;
  std::tie(r.xmin_, r.ymin_) = map(rect.xmin_, rect.ymin_);
  std::tie(r.xmax_, r.ymax_) = map(rect.xmax_, rect.ymax_);
  if (r.xmin_ > r.xmax_) std::swap(r.xmin_, r.xmax_);
  if (r.ymin_ > r.ymax_) std::swap(r.ymin_, r.ymax_);
  return r;
}

GRect GRectMapper::unmap(const GRect &rect) {
  GRect r;
  std::tie(r.xmin_, r.ymin_) = unmap(rect.xmin_, rect.ymin_);
  std::tie(r.xmax_, r.ymax_) = unmap(rect.xmax_, rect.ymax_);
  if (r.xmin_ >= r.xmax_) std::swap(r.xmin_, r.xmax_);
  if (r.ymin_ >= r.ymax_) std::swap(r.ymin_, r.ymax_);
  return r;
}

GRect GRectMapper::get_input() { return rectFrom; }

GRect GRectMapper::get_output() { return rectTo; }

}  // namespace DJVU
