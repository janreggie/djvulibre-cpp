// Copyright [2022] Jan Reggie Dela Cruz
// Copyright [2002] Léon Bottou and Yann Le Cun.
// Copyright [2001] AT&T
// Copyright [1999-2001] LizardTech, Inc.

#ifdef HAVE_CONFIG_H
#include "./config.h"
#endif


// -- Implementation of class GRect and GRectMapper
// - Author: Leon Bottou, 05/1997

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <tuple>

#include "GException.h"
#include "GRect.h"

namespace DJVU {

// -- Class GRect

bool operator==(const GRect &r1, const GRect &r2) {
  if (r1.isempty() && r2.isempty()) return true;
  return (r1.xmin_ == r2.xmin_ && r1.xmax_ == r2.xmax_ &&
          r1.ymin_ == r2.ymin_ && r1.ymax_ == r2.ymax_);
}

bool GRect::inflate(int dx, int dy) {
  xmin_ -= dx;
  xmax_ += dx;
  ymin_ -= dy;
  ymax_ += dy;
  if (!isempty()) return true;
  xmin_ = ymin_ = xmax_ = ymax_ = 0;
  return false;
}

bool GRect::translate(int dx, int dy) {
  xmin_ += dx;
  xmax_ += dx;
  ymin_ += dy;
  ymax_ += dy;
  if (!isempty()) return true;
  xmin_ = ymin_ = xmax_ = ymax_ = 0;
  return false;
}

bool GRect::intersect(const GRect &rect1, const GRect &rect2) {
  xmin_ = std::max(rect1.xmin_, rect2.xmin_);
  xmax_ = std::min(rect1.xmax_, rect2.xmax_);
  ymin_ = std::max(rect1.ymin_, rect2.ymin_);
  ymax_ = std::min(rect1.ymax_, rect2.ymax_);
  if (!isempty()) return true;
  xmin_ = ymin_ = xmax_ = ymax_ = 0;
  return false;
}

bool GRect::recthull(const GRect &rect1, const GRect &rect2) {
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
  xmin_ = std::min(rect1.xmin_, rect2.xmin_);
  xmax_ = std::max(rect1.xmax_, rect2.xmax_);
  ymin_ = std::min(rect1.ymin_, rect2.ymin_);
  ymax_ = std::max(rect1.ymax_, rect2.ymax_);
  return true;
}

bool GRect::contains(const GRect &rect) const {
  GRect tmp_rect;
  tmp_rect.intersect(*this, rect);
  return tmp_rect == rect;
}

void GRect::scale(float factor) {
  xmin_ = static_cast<int>(xmin_ * factor);
  ymin_ = static_cast<int>(ymin_ * factor);
  xmax_ = static_cast<int>(xmax_ * factor);
  ymax_ = static_cast<int>(ymax_ * factor);
}

void GRect::scale(float xfactor, float yfactor) {
  xmin_ = static_cast<int>(xmin_ * xfactor);
  ymin_ = static_cast<int>(ymin_ * yfactor);
  xmax_ = static_cast<int>(xmax_ * xfactor);
  ymax_ = static_cast<int>(ymax_ * yfactor);
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
  int gcd = std::gcd(p, q);
  p /= gcd;
  q /= gcd;
}

inline int operator*(int n, GRectMapper::GRatio r) {
  /* [LB] -- This computation is carried out with integers and
     rational numbers because it must be exact.  Some lizard changed
     it to double and this is wrong.  I suspect they did so because
     they encountered overflow issues.  Let's use long long ints. */
  int64_t x = (int64_t)n * (int64_t)r.p;
  if (x >= 0)
    return ((r.q / 2 + x) / r.q);
  else
    return -((r.q / 2 - x) / r.q);
}

inline int operator/(int n, GRectMapper::GRatio r) {
  /* [LB] -- See comment in operator*() above. */
  int64_t x = (int64_t)n * (int64_t)r.q;
  if (x >= 0)
    return ((r.p / 2 + x) / r.p);
  else
    return -((r.p / 2 - x) / r.p);
}

// -- Class GRectMapper

GRectMapper::GRectMapper()
    : rectFrom_(0, 0, 1, 1), rectTo_(0, 0, 1, 1), code_(0) {}

void GRectMapper::clear() {
  rectFrom_ = GRect(0, 0, 1, 1);
  rectTo_ = GRect(0, 0, 1, 1);
  code_ = 0;
}

void GRectMapper::set_input(const GRect &rect) {
  if (rect.isempty()) G_THROW(ERR_MSG("GRect.empty_rect1"));
  rectFrom_ = rect;
  if (code_ & SWAPXY) {
    std::swap(rectFrom_.xmin_, rectFrom_.ymin_);
    std::swap(rectFrom_.xmax_, rectFrom_.ymax_);
  }
  rw_ = rh_ = GRatio();
}

void GRectMapper::set_output(const GRect &rect) {
  if (rect.isempty()) G_THROW(ERR_MSG("GRect.empty_rect2"));
  rectTo_ = rect;
  rw_ = rh_ = GRatio();
}

void GRectMapper::rotate(int count) {
  int oldcode = code_;
  switch (count & 0x3) {
    case 1:
      code_ ^= (code_ & SWAPXY) ? MIRRORY : MIRRORX;
      code_ ^= SWAPXY;
      break;
    case 2:
      code_ ^= (MIRRORX | MIRRORY);
      break;
    case 3:
      code_ ^= (code_ & SWAPXY) ? MIRRORX : MIRRORY;
      code_ ^= SWAPXY;
      break;
  }
  if ((oldcode ^ code_) & SWAPXY) {
    std::swap(rectFrom_.xmin_, rectFrom_.ymin_);
    std::swap(rectFrom_.xmax_, rectFrom_.ymax_);
    rw_ = rh_ = GRatio();
  }
}

void GRectMapper::mirrorx() { code_ ^= MIRRORX; }

void GRectMapper::mirrory() { code_ ^= MIRRORY; }

void GRectMapper::precalc() {
  if (rectTo_.isempty() || rectFrom_.isempty())
    G_THROW(ERR_MSG("GRect.empty_rect3"));
  rw_ = GRatio(rectTo_.width(), rectFrom_.width());
  rh_ = GRatio(rectTo_.height(), rectFrom_.height());
}

std::pair<int, int> GRectMapper::map(int x, int y) {
  int mx = x;
  int my = y;
  // precalc
  if (rw_.p == 0 || rh_.p == 0) precalc();
  // swap and mirror
  if (code_ & SWAPXY) std::swap(mx, my);
  if (code_ & MIRRORX) mx = rectFrom_.xmin_ + rectFrom_.xmax_ - mx;
  if (code_ & MIRRORY) my = rectFrom_.ymin_ + rectFrom_.ymax_ - my;
  // scale and translate
  x = rectTo_.xmin_ + (mx - rectFrom_.xmin_) * rw_;
  y = rectTo_.ymin_ + (my - rectFrom_.ymin_) * rh_;
  return {x, y};
}

std::pair<int, int> GRectMapper::unmap(int x, int y) {
  // precalc
  if (rw_.p == 0 || rh_.p == 0) precalc();
  // scale and translate
  int mx = rectFrom_.xmin_ + (x - rectTo_.xmin_) / rw_;
  int my = rectFrom_.ymin_ + (y - rectTo_.ymin_) / rh_;
  //  mirror and swap
  if (code_ & MIRRORX) mx = rectFrom_.xmin_ + rectFrom_.xmax_ - mx;
  if (code_ & MIRRORY) my = rectFrom_.ymin_ + rectFrom_.ymax_ - my;
  if (code_ & SWAPXY) std::swap(mx, my);
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

GRect GRectMapper::get_input() { return rectFrom_; }

GRect GRectMapper::get_output() { return rectTo_; }

}  // namespace DJVU
