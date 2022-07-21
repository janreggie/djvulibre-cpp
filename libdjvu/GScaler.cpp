// Copyright [2022] Jan Reggie Dela Cruz
// Copyright [2002] Léon Bottou and Yann Le Cun.
// Copyright [2001] AT&T
// Copyright [1999-2001] LizardTech, Inc.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// Rescale images with fast bilinear interpolation
// From: Leon Bottou, 1/31/2002
// Almost equal to my initial code.

#include <algorithm>
#include <cstdint>

#include "GScaler.h"

namespace DJVU {

////////////////////////////////////////
// CONSTANTS

constexpr int FRACBITS = 4;
constexpr int FRACSIZE = (1 << FRACBITS);
constexpr int FRACSIZE2 = (FRACSIZE >> 1);
constexpr int FRACMASK = (FRACSIZE - 1);

////////////////////////////////////////
// UTILITIES

static int interp_ok = 0;
static std::int16_t interp[FRACSIZE][512];

static void prepare_interp() {
  if (!interp_ok) {
    interp_ok = 1;
    for (int i = 0; i < FRACSIZE; i++) {
      auto *deltas = &interp[i][256];
      for (int j = -255; j <= 255; j++)
        deltas[j] = (j * i + FRACSIZE2) >> FRACBITS;
    }
  }
}

////////////////////////////////////////
// GSCALER

GScaler::GScaler()
    : inw(0),
      inh(0),
      xshift(0),
      yshift(0),
      redw(0),
      redh(0),
      outw(0),
      outh(0),
      gvcoord(vcoord, 0),
      ghcoord(hcoord, 0) {}

GScaler::~GScaler() {}

void GScaler::set_input_size(int w, int h) {
  inw = w;
  inh = h;
  if (vcoord) {
    gvcoord.resize(0);
  }
  if (hcoord) {
    ghcoord.resize(0);
  }
}

void GScaler::set_output_size(int w, int h) {
  outw = w;
  outh = h;
  if (vcoord) {
    gvcoord.resize(0);
  }
  if (hcoord) {
    ghcoord.resize(0);
  }
}

static void prepare_coord(int *coord, int inmax, int outmax, int in, int out) {
  int len = (in * FRACSIZE);
  int beg = (len + out) / (2 * out) - FRACSIZE2;
  // Bresenham algorithm
  int y = beg;
  int z = out / 2;
  int inmaxlim = (inmax - 1) * FRACSIZE;
  for (int x = 0; x < outmax; x++) {
    coord[x] = std::min(y, inmaxlim);
    z = z + len;
    y = y + z / out;
    z = z % out;
  }
  // Result must fit exactly
  if (out == outmax && y != beg + len) G_THROW(ERR_MSG("GScaler.assertion"));
}

void GScaler::set_horz_ratio(int numer, int denom) {
  if (!(inw > 0 && inh > 0 && outw > 0 && outh > 0))
    G_THROW(ERR_MSG("GScaler.undef_size"));
  // Implicit ratio (determined by the input/output sizes)
  if (numer == 0 && denom == 0) {
    numer = outw;
    denom = inw;
  } else if (numer <= 0 || denom <= 0)
    G_THROW(ERR_MSG("GScaler.ratios"));
  // Compute horz reduction
  xshift = 0;
  redw = inw;
  while (numer + numer < denom) {
    xshift += 1;
    redw = (redw + 1) >> 1;
    numer = numer << 1;
  }
  // Compute coordinate table
  if (!hcoord) ghcoord.resize(outw);
  prepare_coord(hcoord, redw, outw, denom, numer);
}

void GScaler::set_vert_ratio(int numer, int denom) {
  if (!(inw > 0 && inh > 0 && outw > 0 && outh > 0))
    G_THROW(ERR_MSG("GScaler.undef_size"));
  // Implicit ratio (determined by the input/output sizes)
  if (numer == 0 && denom == 0) {
    numer = outh;
    denom = inh;
  } else if (numer <= 0 || denom <= 0) {
    G_THROW(ERR_MSG("GScaler.ratios"));
  }
  // Compute horz reduction
  yshift = 0;
  redh = inh;
  while (numer + numer < denom) {
    yshift += 1;
    redh = (redh + 1) >> 1;
    numer = numer << 1;
  }
  // Compute coordinate table
  if (!vcoord) {
    gvcoord.resize(outh);
  }
  prepare_coord(vcoord, redh, outh, denom, numer);
}

void GScaler::make_rectangles(const GRect &desired, GRect &red, GRect &inp) {
  // Parameter validation
  if (desired.xmin_ < 0 || desired.ymin_ < 0 || desired.xmax_ > outw ||
      desired.ymax_ > outh)
    G_THROW(ERR_MSG("GScaler.too_big"));
  // Compute ratio (if not done yet)
  if (!vcoord) set_vert_ratio(0, 0);
  if (!hcoord) set_horz_ratio(0, 0);
  // Compute reduced bounds
  red.xmin_ = (hcoord[desired.xmin_]) >> FRACBITS;
  red.ymin_ = (vcoord[desired.ymin_]) >> FRACBITS;
  red.xmax_ = (hcoord[desired.xmax_ - 1] + FRACSIZE - 1) >> FRACBITS;
  red.ymax_ = (vcoord[desired.ymax_ - 1] + FRACSIZE - 1) >> FRACBITS;
  // Borders
  red.xmin_ = std::max(red.xmin_, 0);
  red.xmax_ = std::min(red.xmax_ + 1, redw);
  red.ymin_ = std::max(red.ymin_, 0);
  red.ymax_ = std::min(red.ymax_ + 1, redh);
  // Input
  inp.xmin_ = std::max(red.xmin_ << xshift, 0);
  inp.xmax_ = std::min(red.xmax_ << xshift, inw);
  inp.ymin_ = std::max(red.ymin_ << yshift, 0);
  inp.ymax_ = std::min(red.ymax_ << yshift, inh);
}

void GScaler::get_input_rect(const GRect &desired_output,
                             GRect &required_input) {
  GRect red;
  make_rectangles(desired_output, red, required_input);
}

////////////////////////////////////////
// GBITMAPSCALER

GBitmapScaler::GBitmapScaler()
    : glbuffer(lbuffer, 0), gconv(conv, 0), gp1(p1, 0), gp2(p2, 0) {}

GBitmapScaler::GBitmapScaler(int inw, int inh, int outw, int outh)
    : glbuffer(lbuffer, 0), gconv(conv, 0), gp1(p1, 0), gp2(p2, 0) {
  set_input_size(inw, inh);
  set_output_size(outw, outh);
}

GBitmapScaler::~GBitmapScaler() {}

unsigned char *GBitmapScaler::get_line(int fy, const GRect &required_red,
                                       const GRect &provided_input,
                                       const GBitmap &input) {
  if (fy < required_red.ymin_)
    fy = required_red.ymin_;
  else if (fy >= required_red.ymax_)
    fy = required_red.ymax_ - 1;
  // Cached line
  if (fy == l2) return p2;
  if (fy == l1) return p1;
  // Shift
  unsigned char *p = p1;
  p1 = p2;
  l1 = l2;
  p2 = p;
  l2 = fy;
  if (xshift == 0 && yshift == 0) {
    // Fast mode
    int dx = required_red.xmin_ - provided_input.xmin_;
    int dx1 = required_red.xmax_ - provided_input.xmin_;
    const unsigned char *inp1 = input[fy - provided_input.ymin_] + dx;
    while (dx++ < dx1) *p++ = conv[*inp1++];
    return p2;
  } else {
    // Compute location of line
    GRect line;
    line.xmin_ = required_red.xmin_ << xshift;
    line.xmax_ = required_red.xmax_ << xshift;
    line.ymin_ = fy << yshift;
    line.ymax_ = (fy + 1) << yshift;
    line.intersect(line, provided_input);
    line.translate(-provided_input.xmin_, -provided_input.ymin_);
    // Prepare variables
    const unsigned char *botline = input[line.ymin_];
    int rowsize = input.rowsize();
    int sw = 1 << xshift;
    int div = xshift + yshift;
    int rnd = 1 << (div - 1);
    // Compute averages
    for (int x = line.xmin_; x < line.xmax_; x += sw, p++) {
      int g = 0, s = 0;
      const unsigned char *inp0 = botline + x;
      int sy1 = std::min(line.height(), (1 << yshift));
      for (int sy = 0; sy < sy1; sy++, inp0 += rowsize) {
        const unsigned char *inp1;
        const unsigned char *inp2 = inp0 + std::min(x + sw, line.xmax_) - x;
        for (inp1 = inp0; inp1 < inp2; inp1++) {
          g += conv[*inp1];
          s += 1;
        }
      }
      if (s == rnd + rnd)
        *p = (g + rnd) >> div;
      else
        *p = (g + s / 2) / s;
    }
    // Return
    return p2;
  }
}

void GBitmapScaler::scale(const GRect &provided_input, const GBitmap &input,
                          const GRect &desired_output, GBitmap &output) {
  // Compute rectangles
  GRect required_input;
  GRect required_red;
  make_rectangles(desired_output, required_red, required_input);
  // Parameter validation
  if (provided_input.width() != (int)input.columns() ||
      provided_input.height() != (int)input.rows())
    G_THROW(ERR_MSG("GScaler.no_match"));
  if (provided_input.xmin_ > required_input.xmin_ ||
      provided_input.ymin_ > required_input.ymin_ ||
      provided_input.xmax_ < required_input.xmax_ ||
      provided_input.ymax_ < required_input.ymax_)
    G_THROW(ERR_MSG("GScaler.too_small"));
  // Adjust output pixmap
  if (desired_output.width() != (int)output.columns() ||
      desired_output.height() != (int)output.rows())
    output.init(desired_output.height(), desired_output.width());
  output.set_grays(256);
  // Prepare temp stuff
  gp1.resize(0);
  gp2.resize(0);
  glbuffer.resize(0);
  prepare_interp();
  const int bufw = required_red.width();
  glbuffer.resize(bufw + 2);
  gp1.resize(bufw);
  gp2.resize(bufw);
  l1 = l2 = -1;
  // Prepare gray conversion array (conv)
  gconv.resize(0);
  gconv.resize(256);
  int maxgray = input.get_grays() - 1;
  for (int i = 0; i < 256; i++) {
    conv[i] = (i <= maxgray) ? (((i * 255) + (maxgray >> 1)) / maxgray) : 255;
  }
  // Loop on output lines
  for (int y = desired_output.ymin_; y < desired_output.ymax_; y++) {
    // Perform vertical interpolation
    {
      int fy = vcoord[y];
      int fy1 = fy >> FRACBITS;
      int fy2 = fy1 + 1;
      const unsigned char *lower, *upper;
      // Obtain upper and lower line in reduced image
      lower = get_line(fy1, required_red, provided_input, input);
      upper = get_line(fy2, required_red, provided_input, input);
      // Compute line
      unsigned char *dest = lbuffer + 1;
      const short *deltas = &interp[fy & FRACMASK][256];
      for (unsigned char const *const edest =
               (unsigned char const *)dest + bufw;
           dest < edest; upper++, lower++, dest++) {
        const int l = *lower;
        const int u = *upper;
        *dest = l + deltas[u - l];
      }
    }
    // Perform horizontal interpolation
    {
      // Prepare for side effects
      lbuffer[0] = lbuffer[1];
      lbuffer[bufw + 1] = lbuffer[bufw];
      unsigned char *line = lbuffer + 1 - required_red.xmin_;
      unsigned char *dest = output[y - desired_output.ymin_];
      // Loop horizontally
      for (int x = desired_output.xmin_; x < desired_output.xmax_; x++) {
        int n = hcoord[x];
        const unsigned char *lower = line + (n >> FRACBITS);
        const short *deltas = &interp[n & FRACMASK][256];
        int l = lower[0];
        int u = lower[1];
        *dest = l + deltas[u - l];
        dest++;
      }
    }
  }
  // Free temporaries
  gp1.resize(0);
  gp2.resize(0);
  glbuffer.resize(0);
  gconv.resize(0);
}

////////////////////////////////////////
// GPIXMAPSCALER

GPixmapScaler::GPixmapScaler() : glbuffer(lbuffer, 0), gp1(p1, 0), gp2(p2, 0) {}

GPixmapScaler::GPixmapScaler(int inw, int inh, int outw, int outh)
    : glbuffer(lbuffer, 0), gp1(p1, 0), gp2(p2, 0) {
  set_input_size(inw, inh);
  set_output_size(outw, outh);
}

GPixmapScaler::~GPixmapScaler() {}

GPixel *GPixmapScaler::get_line(int fy, const GRect &required_red,
                                const GRect &provided_input,
                                const GPixmap &input) {
  if (fy < required_red.ymin_)
    fy = required_red.ymin_;
  else if (fy >= required_red.ymax_)
    fy = required_red.ymax_ - 1;
  // Cached line
  if (fy == l2) return p2;
  if (fy == l1) return p1;
  // Shift
  GPixel *p = p1;
  p1 = p2;
  l1 = l2;
  p2 = p;
  l2 = fy;
  // Compute location of line
  GRect line;
  line.xmin_ = required_red.xmin_ << xshift;
  line.xmax_ = required_red.xmax_ << xshift;
  line.ymin_ = fy << yshift;
  line.ymax_ = (fy + 1) << yshift;
  line.intersect(line, provided_input);
  line.translate(-provided_input.xmin_, -provided_input.ymin_);
  // Prepare variables
  const GPixel *botline = input[line.ymin_];
  int rowsize = input.rowsize();
  int sw = 1 << xshift;
  int div = xshift + yshift;
  int rnd = 1 << (div - 1);
  // Compute averages
  for (int x = line.xmin_; x < line.xmax_; x += sw, p++) {
    int r = 0, g = 0, b = 0, s = 0;
    const GPixel *inp0 = botline + x;
    int sy1 = std::min(line.height(), (1 << yshift));
    for (int sy = 0; sy < sy1; sy++, inp0 += rowsize) {
      const GPixel *inp1;
      const GPixel *inp2 = inp0 + std::min(x + sw, line.xmax_) - x;
      for (inp1 = inp0; inp1 < inp2; inp1++) {
        r += inp1->r;
        g += inp1->g;
        b += inp1->b;
        s += 1;
      }
    }
    if (s == rnd + rnd) {
      p->r = (r + rnd) >> div;
      p->g = (g + rnd) >> div;
      p->b = (b + rnd) >> div;
    } else {
      p->r = (r + s / 2) / s;
      p->g = (g + s / 2) / s;
      p->b = (b + s / 2) / s;
    }
  }
  // Return
  return (GPixel *)p2;
}

void GPixmapScaler::scale(const GRect &provided_input, const GPixmap &input,
                          const GRect &desired_output, GPixmap &output) {
  // Compute rectangles
  GRect required_input;
  GRect required_red;
  make_rectangles(desired_output, required_red, required_input);
  // Parameter validation
  if (provided_input.width() != (int)input.columns() ||
      provided_input.height() != (int)input.rows())
    G_THROW(ERR_MSG("GScaler.no_match"));
  if (provided_input.xmin_ > required_input.xmin_ ||
      provided_input.ymin_ > required_input.ymin_ ||
      provided_input.xmax_ < required_input.xmax_ ||
      provided_input.ymax_ < required_input.ymax_)
    G_THROW(ERR_MSG("GScaler.too_small"));
  // Adjust output pixmap
  if (desired_output.width() != (int)output.columns() ||
      desired_output.height() != (int)output.rows())
    output.init(desired_output.height(), desired_output.width());
  // Prepare temp stuff
  gp1.resize(0);
  gp2.resize(0);
  glbuffer.resize(0);
  prepare_interp();
  const int bufw = required_red.width();
  glbuffer.resize(bufw + 2);
  if (xshift > 0 || yshift > 0) {
    gp1.resize(bufw);
    gp2.resize(bufw);
    l1 = l2 = -1;
  }
  // Loop on output lines
  for (int y = desired_output.ymin_; y < desired_output.ymax_; y++) {
    // Perform vertical interpolation
    {
      int fy = vcoord[y];
      int fy1 = fy >> FRACBITS;
      int fy2 = fy1 + 1;
      const GPixel *lower, *upper;
      // Obtain upper and lower line in reduced image
      if (xshift > 0 || yshift > 0) {
        lower = get_line(fy1, required_red, provided_input, input);
        upper = get_line(fy2, required_red, provided_input, input);
      } else {
        int dx = required_red.xmin_ - provided_input.xmin_;
        fy1 = std::max(fy1, required_red.ymin_);
        fy2 = std::min(fy2, required_red.ymax_ - 1);
        lower = input[fy1 - provided_input.ymin_] + dx;
        upper = input[fy2 - provided_input.ymin_] + dx;
      }
      // Compute line
      GPixel *dest = lbuffer + 1;
      const short *deltas = &interp[fy & FRACMASK][256];
      for (GPixel const *const edest = (GPixel const *)dest + bufw;
           dest < edest; upper++, lower++, dest++) {
        const int lower_r = lower->r;
        const int delta_r = deltas[(int)upper->r - lower_r];
        dest->r = lower_r + delta_r;
        const int lower_g = lower->g;
        const int delta_g = deltas[(int)upper->g - lower_g];
        dest->g = lower_g + delta_g;
        const int lower_b = lower->b;
        const int delta_b = deltas[(int)upper->b - lower_b];
        dest->b = lower_b + delta_b;
      }
    }
    // Perform horizontal interpolation
    {
      // Prepare for side effects
      lbuffer[0] = lbuffer[1];
      lbuffer[bufw + 1] = lbuffer[bufw];
      GPixel *line = lbuffer + 1 - required_red.xmin_;
      GPixel *dest = output[y - desired_output.ymin_];
      // Loop horizontally
      for (int x = desired_output.xmin_; x < desired_output.xmax_;
           x++, dest++) {
        const int n = hcoord[x];
        const GPixel *lower = line + (n >> FRACBITS);
        const short *deltas = &interp[n & FRACMASK][256];
        const int lower_r = lower[0].r;
        const int delta_r = deltas[(int)lower[1].r - lower_r];
        dest->r = lower_r + delta_r;
        const int lower_g = lower[0].g;
        const int delta_g = deltas[(int)lower[1].g - lower_g];
        dest->g = lower_g + delta_g;
        const int lower_b = lower[0].b;
        const int delta_b = deltas[(int)lower[1].b - lower_b];
        dest->b = lower_b + delta_b;
      }
    }
  }
  // Free temporaries
  gp1.resize(0);
  gp2.resize(0);
  glbuffer.resize(0);
}

}  // namespace DJVU
using namespace DJVU;
