// Copyright [2022] Jan Reggie Dela Cruz
// Copyright [2002] Léon Bottou and Yann Le Cun.
// Copyright [2001] AT&T
// Copyright [1999-2001] LizardTech, Inc.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "ByteStream.h"
#include "GBitmap.h"
#include "GException.h"
#include "GRect.h"
#include "GString.h"
#include "GThreads.h"

namespace DJVU {

// ----- constructor and destructor

GBitmap::~GBitmap() {}

void GBitmap::destroy(void) {
  gbytes_data_.resize(0);
  bytes_ = 0;
  grle_.resize(0);
  grlerows_.resize(0);
  rlelength_ = 0;
}

GBitmap::GBitmap()
    : nrows_(0),
      ncolumns_(0),
      border_(0),
      bytes_per_row_(0),
      grays_(0),
      bytes_(0),
      gbytes_data_(bytes_data_),
      grle_(rle_),
      grlerows_(rlerows_),
      rlelength_(0),
      monitorptr_(0) {}

GBitmap::GBitmap(int nrows, int ncolumns, int border)
    : nrows_(0),
      ncolumns_(0),
      border_(0),
      bytes_per_row_(0),
      grays_(0),
      bytes_(0),
      gbytes_data_(bytes_data_),
      grle_(rle_),
      grlerows_(rlerows_),
      rlelength_(0),
      monitorptr_(0) {
  G_TRY { init(nrows, ncolumns, border); }
  G_CATCH_ALL {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}

GBitmap::GBitmap(ByteStream &ref, int border)
    : nrows_(0),
      ncolumns_(0),
      border_(0),
      bytes_per_row_(0),
      grays_(0),
      bytes_(0),
      gbytes_data_(bytes_data_),
      grle_(rle_),
      grlerows_(rlerows_),
      rlelength_(0),
      monitorptr_(0) {
  G_TRY { init(ref, border); }
  G_CATCH_ALL {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}

GBitmap::GBitmap(const GBitmap &ref)
    : nrows_(0),
      ncolumns_(0),
      border_(0),
      bytes_per_row_(0),
      grays_(0),
      bytes_(0),
      gbytes_data_(bytes_data_),
      grle_(rle_),
      grlerows_(rlerows_),
      rlelength_(0),
      monitorptr_(0) {
  G_TRY { init(ref, ref.border_); }
  G_CATCH_ALL {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}

GBitmap::GBitmap(const GBitmap &ref, int border)
    : nrows_(0),
      ncolumns_(0),
      border_(0),
      bytes_per_row_(0),
      grays_(0),
      bytes_(0),
      gbytes_data_(bytes_data_),
      grle_(rle_),
      grlerows_(rlerows_),
      rlelength_(0),
      monitorptr_(0) {
  G_TRY { init(ref, border); }
  G_CATCH_ALL {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}

GBitmap::GBitmap(const GBitmap &ref, const GRect &rect, int border)
    : nrows_(0),
      ncolumns_(0),
      border_(0),
      bytes_per_row_(0),
      grays_(0),
      bytes_(0),
      gbytes_data_(bytes_data_),
      grle_(rle_),
      grlerows_(rlerows_),
      rlelength_(0),
      monitorptr_(0) {
  G_TRY { init(ref, rect, border); }
  G_CATCH_ALL {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}

// ----- initialization

void GBitmap::init(int arows, int acolumns, int aborder) {
  std::size_t np = arows * (acolumns + aborder) + aborder;
  if (arows != (std::uint16_t)arows || acolumns != (std::uint16_t)acolumns ||
      acolumns + aborder != (std::uint16_t)(acolumns + aborder) ||
      (arows > 0 &&
       (np - aborder) / (std::size_t)arows != (std::size_t)(acolumns + aborder)))
    G_THROW("GBitmap: image size exceeds maximum (corrupted file?)");
  GMonitorLock lock(monitor());
  destroy();
  grays_ = 2;
  nrows_ = arows;
  ncolumns_ = acolumns;
  border_ = aborder;
  bytes_per_row_ = ncolumns_ + border_;
  int npixels = nrows_ * bytes_per_row_ + border_;
  gzerobuffer_ = zeroes(bytes_per_row_ + border_);
  if (npixels > 0) {
    gbytes_data_.resize(npixels);
    gbytes_data_.clear();
    bytes_ = bytes_data_;
  }
}

void GBitmap::init(const GBitmap &ref, int aborder) {
  GMonitorLock lock(monitor());
  if (this != &ref) {
    GMonitorLock lock(ref.monitor());
    init(ref.nrows_, ref.ncolumns_, aborder);
    grays_ = ref.grays_;
    unsigned char *row = bytes_data_ + border_;
    for (int n = 0; n < nrows_; n++, row += bytes_per_row_)
      memcpy((void *)row, (void *)ref[n], ncolumns_);
  } else if (aborder > border_) {
    minborder(aborder);
  }
}

void GBitmap::init(const GBitmap &ref, const GRect &rect, int border) {
  GMonitorLock lock(monitor());
  // test bitmap physical equality
  if (this == &ref) {
    GBitmap tmp;
    tmp.grays_ = grays_;
    tmp.border_ = border;
    tmp.bytes_per_row_ = bytes_per_row_;
    tmp.ncolumns_ = ncolumns_;
    tmp.nrows_ = nrows_;
    tmp.bytes_ = bytes_;
    tmp.gbytes_data_.swap(gbytes_data_);
    tmp.grle_.swap(grle_);
    bytes_ = 0;
    init(tmp, rect, border);
  } else {
    GMonitorLock lock(ref.monitor());
    // create empty bitmap
    init(rect.height(), rect.width(), border);
    grays_ = ref.grays_;
    // compute destination rectangle
    GRect rect2(0, 0, ref.columns(), ref.rows());
    rect2.intersect(rect2, rect);
    rect2.translate(-rect.xmin_, -rect.ymin_);
    // copy bits
    if (!rect2.isempty()) {
      for (int y = rect2.ymin_; y < rect2.ymax_; y++) {
        unsigned char *dst = (*this)[y];
        const unsigned char *src = ref[y + rect.ymin_] + rect.xmin_;
        for (int x = rect2.xmin_; x < rect2.xmax_; x++) dst[x] = src[x];
      }
    }
  }
}

void GBitmap::init(ByteStream &ref, int aborder) {
  GMonitorLock lock(monitor());
  // Get magic number
  char magic[2];
  magic[0] = magic[1] = 0;
  ref.readall((void *)magic, sizeof(magic));
  char lookahead = '\n';
  int acolumns = read_integer(lookahead, ref);
  int arows = read_integer(lookahead, ref);
  int maxval = 1;
  init(arows, acolumns, aborder);
  // go reading file
  if (magic[0] == 'P') {
    switch (magic[1]) {
      case '1':
        grays_ = 2;
        read_pbm_text(ref);
        return;
      case '2':
        maxval = read_integer(lookahead, ref);
        if (maxval > 65535)
          G_THROW("Cannot read PGM with depth greater than 16 bits.");
        grays_ = (maxval > 255 ? 256 : maxval + 1);
        read_pgm_text(ref, maxval);
        return;
      case '4':
        grays_ = 2;
        read_pbm_raw(ref);
        return;
      case '5':
        maxval = read_integer(lookahead, ref);
        if (maxval > 65535)
          G_THROW("Cannot read PGM with depth greater than 16 bits.");
        grays_ = (maxval > 255 ? 256 : maxval + 1);
        read_pgm_raw(ref, maxval);
        return;
    }
  } else if (magic[0] == 'R') {
    switch (magic[1]) {
      case '4':
        grays_ = 2;
        read_rle_raw(ref);
        return;
    }
  }
  G_THROW(ERR_MSG("GBitmap.bad_format"));
}

void GBitmap::donate_data(unsigned char *data, int w, int h) {
  destroy();
  grays_ = 2;
  nrows_ = h;
  ncolumns_ = w;
  border_ = 0;
  bytes_per_row_ = w;
  gbytes_data_.replace(data, w * h);
  bytes_ = bytes_data_;
  rlelength_ = 0;
}

void GBitmap::donate_rle(unsigned char *rledata, unsigned int rledatalen, int w,
                         int h) {
  destroy();
  grays_ = 2;
  nrows_ = h;
  ncolumns_ = w;
  border_ = 0;
  bytes_per_row_ = w;
  //  rle_ = rledata;
  grle_.replace(rledata, rledatalen);
  rlelength_ = rledatalen;
}

unsigned char *GBitmap::take_data(std::size_t &offset) {
  GMonitorLock lock(monitor());
  unsigned char *ret = bytes_data_;
  if (ret) offset = (std::size_t)border_;
  bytes_data_ = 0;
  return ret;
}

const unsigned char *GBitmap::get_rle(unsigned int &rle_length) {
  if (!rle_) compress();
  rle_length = rlelength_;
  return rle_;
}

// ----- compression

void GBitmap::compress() {
  if (grays_ > 2) G_THROW(ERR_MSG("GBitmap.cant_compress"));
  GMonitorLock lock(monitor());
  if (bytes_) {
    grle_.resize(0);
    grlerows_.resize(0);
    rlelength_ = encode(rle_, grle_);
    if (rlelength_) {
      gbytes_data_.resize(0);
      bytes_ = 0;
    }
  }
}

void GBitmap::uncompress() {
  GMonitorLock lock(monitor());
  if (!bytes_ && rle_) decode(rle_);
}

unsigned int GBitmap::get_memory_usage() const {
  unsigned long usage = sizeof(GBitmap);
  if (bytes_) usage += nrows_ * bytes_per_row_ + border_;
  if (rle_) usage += rlelength_;
  return usage;
}

void GBitmap::minborder(int minimum) {
  if (border_ < minimum) {
    GMonitorLock lock(monitor());
    if (border_ < minimum) {
      if (bytes_) {
        GBitmap tmp(*this, minimum);
        bytes_per_row_ = tmp.bytes_per_row_;
        tmp.gbytes_data_.swap(gbytes_data_);
        bytes_ = bytes_data_;
        tmp.bytes_ = 0;
      }
      border_ = minimum;
      gzerobuffer_ = zeroes(border_ + ncolumns_ + border_);
    }
  }
}

#define NMONITORS 8
static GMonitor monitors[NMONITORS];

void GBitmap::share() {
  if (!monitorptr_) {
    std::size_t x = (std::size_t)this;
    monitorptr_ = &monitors[(x ^ (x >> 5)) % NMONITORS];
  }
}

// ----- gray levels

void GBitmap::set_grays(int ngrays) {
  if (ngrays < 2 || ngrays > 256) G_THROW(ERR_MSG("GBitmap.bad_levels"));
  // set gray levels
  GMonitorLock lock(monitor());
  grays_ = ngrays;
  if (ngrays > 2 && !bytes_) uncompress();
}

void GBitmap::change_grays(int ngrays) {
  GMonitorLock lock(monitor());
  // set number of grays
  int ng = ngrays - 1;
  int og = grays_ - 1;
  set_grays(ngrays);
  // setup conversion table
  unsigned char conv[256];
  for (int i = 0; i < 256; i++) {
    if (i > og)
      conv[i] = ng;
    else
      conv[i] = (i * ng + og / 2) / og;
  }
  // perform conversion
  for (int row = 0; row < nrows_; row++) {
    unsigned char *p = (*this)[row];
    for (int n = 0; n < ncolumns_; n++) p[n] = conv[p[n]];
  }
}

void GBitmap::binarize_grays(int threshold) {
  GMonitorLock lock(monitor());
  if (bytes_)
    for (int row = 0; row < nrows_; row++) {
      unsigned char *p = (*this)[row];
      for (unsigned char const *const pend = p + ncolumns_; p < pend; ++p) {
        *p = (*p > threshold) ? 1 : 0;
      }
    }
  grays_ = 2;
}

// ----- additive blitting

#undef min
#undef max

static inline int min(int x, int y) { return (x < y ? x : y); }

static inline int max(int x, int y) { return (x > y ? x : y); }

void GBitmap::blit(const GBitmap *bm, int x, int y) {
  // Check boundaries
  if ((x >= ncolumns_) || (y >= nrows_) || (x + (int)bm->columns() < 0) ||
      (y + (int)bm->rows() < 0))
    return;

  // Perform blit
  GMonitorLock lock1(monitor());
  GMonitorLock lock2(bm->monitor());
  if (bm->bytes_) {
    if (!bytes_data_) uncompress();
    // Blit from bitmap
    const unsigned char *srow = bm->bytes_ + bm->border_;
    unsigned char *drow = bytes_data_ + border_ + y * bytes_per_row_ + x;
    for (int sr = 0; sr < bm->nrows_; sr++) {
      if (sr + y >= 0 && sr + y < nrows_) {
        int sc = max(0, -x);
        int sc1 = min(bm->ncolumns_, ncolumns_ - x);
        while (sc < sc1) {
          drow[sc] += srow[sc];
          sc += 1;
        }
      }
      srow += bm->bytes_per_row_;
      drow += bytes_per_row_;
    }
  } else if (bm->rle_) {
    if (!bytes_data_) uncompress();
    // Blit from rle
    const unsigned char *runs = bm->rle_;
    unsigned char *drow = bytes_data_ + border_ + y * bytes_per_row_ + x;
    int sr = bm->nrows_ - 1;
    drow += sr * bytes_per_row_;
    int sc = 0;
    char p = 0;
    while (sr >= 0) {
      const int z = read_run(runs);
      if (sc + z > bm->ncolumns_) G_THROW(ERR_MSG("GBitmap.lost_sync"));
      int nc = sc + z;
      if (p && sr + y >= 0 && sr + y < nrows_) {
        if (sc + x < 0) sc = min(-x, nc);
        while (sc < nc && sc + x < ncolumns_) drow[sc++] += 1;
      }
      sc = nc;
      p = 1 - p;
      if (sc >= bm->ncolumns_) {
        p = 0;
        sc = 0;
        drow -= bytes_per_row_;
        sr -= 1;
      }
    }
  }
}

void GBitmap::blit(const GBitmap *bm, int xh, int yh, int subsample) {
  // Use code when no subsampling is necessary
  if (subsample == 1) {
    blit(bm, xh, yh);
    return;
  }

  // Check boundaries
  if ((xh >= ncolumns_ * subsample) || (yh >= nrows_ * subsample) ||
      (xh + (int)bm->columns() < 0) || (yh + (int)bm->rows() < 0))
    return;

  // Perform subsampling blit
  GMonitorLock lock1(monitor());
  GMonitorLock lock2(bm->monitor());
  if (bm->bytes_) {
    if (!bytes_data_) uncompress();
    // Blit from bitmap
    int dr, dr1, zdc, zdc1;
    euclidian_ratio(yh, subsample, dr, dr1);
    euclidian_ratio(xh, subsample, zdc, zdc1);
    const unsigned char *srow = bm->bytes_ + bm->border_;
    unsigned char *drow = bytes_data_ + border_ + dr * bytes_per_row_;
    for (int sr = 0; sr < bm->nrows_; sr++) {
      if (dr >= 0 && dr < nrows_) {
        int dc = zdc;
        int dc1 = zdc1;
        for (int sc = 0; sc < bm->ncolumns_; sc++) {
          if (dc >= 0 && dc < ncolumns_) drow[dc] += srow[sc];
          if (++dc1 >= subsample) {
            dc1 = 0;
            dc += 1;
          }
        }
      }
      // next line in source
      srow += bm->bytes_per_row_;
      // next line fraction in destination
      if (++dr1 >= subsample) {
        dr1 = 0;
        dr += 1;
        drow += bytes_per_row_;
      }
    }
  } else if (bm->rle_) {
    if (!bytes_data_) uncompress();
    // Blit from rle
    int dr, dr1, zdc, zdc1;
    euclidian_ratio(yh + bm->nrows_ - 1, subsample, dr, dr1);
    euclidian_ratio(xh, subsample, zdc, zdc1);
    const unsigned char *runs = bm->rle_;
    unsigned char *drow = bytes_data_ + border_ + dr * bytes_per_row_;
    int sr = bm->nrows_ - 1;
    int sc = 0;
    char p = 0;
    int dc = zdc;
    int dc1 = zdc1;
    while (sr >= 0) {
      int z = read_run(runs);
      if (sc + z > bm->ncolumns_) G_THROW(ERR_MSG("GBitmap.lost_sync"));
      int nc = sc + z;

      if (dr >= 0 && dr < nrows_)
        while (z > 0 && dc < ncolumns_) {
          int zd = subsample - dc1;
          if (zd > z) zd = z;
          if (p && dc >= 0) drow[dc] += zd;
          z -= zd;
          dc1 += zd;
          if (dc1 >= subsample) {
            dc1 = 0;
            dc += 1;
          }
        }
      // next fractional row
      sc = nc;
      p = 1 - p;
      if (sc >= bm->ncolumns_) {
        sc = 0;
        dc = zdc;
        dc1 = zdc1;
        p = 0;
        sr -= 1;
        if (--dr1 < 0) {
          dr1 = subsample - 1;
          dr -= 1;
          drow -= bytes_per_row_;
        }
      }
    }
  }
}

// ------ load bitmaps

unsigned int GBitmap::read_integer(char &c, ByteStream &bs) {
  unsigned int x = 0;
  // eat blank before integer
  while (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '#') {
    if (c == '#') do {
      } while (bs.read(&c, 1) && c != '\n' && c != '\r');
    c = 0;
    bs.read(&c, 1);
  }
  // check integer
  if (c < '0' || c > '9') G_THROW(ERR_MSG("GBitmap.not_int"));
  // eat integer
  while (c >= '0' && c <= '9') {
    x = x * 10 + c - '0';
    c = 0;
    bs.read(&c, 1);
  }
  return x;
}

void GBitmap::read_pbm_text(ByteStream &bs) {
  unsigned char *row = bytes_data_ + border_;
  row += (nrows_ - 1) * bytes_per_row_;
  for (int n = nrows_ - 1; n >= 0; n--) {
    for (int c = 0; c < ncolumns_; c++) {
      char bit = 0;
      bs.read(&bit, 1);
      while (bit == ' ' || bit == '\t' || bit == '\r' || bit == '\n') {
        bit = 0;
        bs.read(&bit, 1);
      }
      if (bit == '1')
        row[c] = 1;
      else if (bit == '0')
        row[c] = 0;
      else
        G_THROW(ERR_MSG("GBitmap.bad_PBM"));
    }
    row -= bytes_per_row_;
  }
}

void GBitmap::read_pgm_text(ByteStream &bs, int maxval) {
  unsigned char *row = bytes_data_ + border_;
  row += (nrows_ - 1) * bytes_per_row_;
  char lookahead = '\n';
  GTArray<unsigned char> ramp(0, maxval);
  for (int i = 0; i <= maxval; i++)
    ramp[i] =
        (i < maxval ? ((grays_ - 1) * (maxval - i) + maxval / 2) / maxval : 0);
  for (int n = nrows_ - 1; n >= 0; n--) {
    for (int c = 0; c < ncolumns_; c++)
      row[c] = ramp[(int)read_integer(lookahead, bs)];
    row -= bytes_per_row_;
  }
}

void GBitmap::read_pbm_raw(ByteStream &bs) {
  unsigned char *row = bytes_data_ + border_;
  row += (nrows_ - 1) * bytes_per_row_;
  for (int n = nrows_ - 1; n >= 0; n--) {
    unsigned char acc = 0;
    unsigned char mask = 0;
    for (int c = 0; c < ncolumns_; c++) {
      if (!mask) {
        bs.read(&acc, 1);
        mask = (unsigned char)0x80;
      }
      if (acc & mask)
        row[c] = 1;
      else
        row[c] = 0;
      mask >>= 1;
    }
    row -= bytes_per_row_;
  }
}

void GBitmap::read_pgm_raw(ByteStream &bs, int maxval) {
  int maxbin = (maxval > 255) ? 65536 : 256;
  GTArray<unsigned char> ramp(0, maxbin - 1);
  for (int i = 0; i < maxbin; i++)
    ramp[i] =
        (i < maxval ? ((grays_ - 1) * (maxval - i) + maxval / 2) / maxval : 0);
  unsigned char *bramp = ramp;
  unsigned char *row = bytes_data_ + border_;
  row += (nrows_ - 1) * bytes_per_row_;
  for (int n = nrows_ - 1; n >= 0; n--) {
    if (maxbin > 256) {
      for (int c = 0; c < ncolumns_; c++) {
        unsigned char x[2];
        bs.read((void *)&x, 2);
        row[c] = bramp[x[0] * 256 + x[1]];
      }
    } else {
      for (int c = 0; c < ncolumns_; c++) {
        unsigned char x;
        bs.read((void *)&x, 1);
        row[c] = bramp[x];
      }
    }
    row -= bytes_per_row_;
  }
}

void GBitmap::read_rle_raw(ByteStream &bs) {
  // interpret runs data
  unsigned char h;
  unsigned char p = 0;
  unsigned char *row = bytes_data_ + border_;
  int n = nrows_ - 1;
  row += n * bytes_per_row_;
  int c = 0;
  while (n >= 0) {
    if (bs.read(&h, 1) <= 0) G_THROW(ByteStream::EndOfFile);
    int x = h;
    if (x >= (int)RUNOVERFLOWVALUE) {
      if (bs.read(&h, 1) <= 0) G_THROW(ByteStream::EndOfFile);
      x = h + ((x - (int)RUNOVERFLOWVALUE) << 8);
    }
    if (c + x > ncolumns_) G_THROW(ERR_MSG("GBitmap.lost_sync"));
    while (x-- > 0) row[c++] = p;
    p = 1 - p;
    if (c >= ncolumns_) {
      c = 0;
      p = 0;
      row -= bytes_per_row_;
      n -= 1;
    }
  }
}

// ------ save bitmaps

void GBitmap::save_pbm(ByteStream &bs, int raw) {
  // check arguments
  if (grays_ > 2) G_THROW(ERR_MSG("GBitmap.cant_make_PBM"));
  GMonitorLock lock(monitor());
  // header
  {
    GUTF8String head;
    head.format("P%c\n%d %d\n", (raw ? '4' : '1'), ncolumns_, nrows_);
    bs.writall((void *)(const char *)head, head.length());
  }
  // body
  if (raw) {
    if (!rle_) compress();
    const unsigned char *runs = rle_;
    const unsigned char *const runs_end = rle_ + rlelength_;
    const int count = (ncolumns_ + 7) >> 3;
    unsigned char *buf;
    GPBuffer<unsigned char> gbuf(buf, count);
    while (runs < runs_end) {
      rle_get_bitmap(ncolumns_, runs, buf, false);
      bs.writall(buf, count);
    }
  } else {
    if (!bytes_) uncompress();
    const unsigned char *row = bytes_ + border_;
    int n = nrows_ - 1;
    row += n * bytes_per_row_;
    while (n >= 0) {
      unsigned char eol = '\n';
      for (int c = 0; c < ncolumns_;) {
        unsigned char bit = (row[c] ? '1' : '0');
        bs.write((void *)&bit, 1);
        c += 1;
        if (c == ncolumns_ || (c & (int)RUNMSBMASK) == 0)
          bs.write((void *)&eol, 1);
      }
      // next row
      row -= bytes_per_row_;
      n -= 1;
    }
  }
}

void GBitmap::save_pgm(ByteStream &bs, int raw) {
  // checks
  GMonitorLock lock(monitor());
  if (!bytes_) uncompress();
  // header
  GUTF8String head;
  head.format("P%c\n%d %d\n%d\n", (raw ? '5' : '2'), ncolumns_, nrows_,
              grays_ - 1);
  bs.writall((void *)(const char *)head, head.length());
  // body
  const unsigned char *row = bytes_ + border_;
  int n = nrows_ - 1;
  row += n * bytes_per_row_;
  while (n >= 0) {
    if (raw) {
      for (int c = 0; c < ncolumns_; c++) {
        char x = grays_ - 1 - row[c];
        bs.write((void *)&x, 1);
      }
    } else {
      unsigned char eol = '\n';
      for (int c = 0; c < ncolumns_;) {
        head.format("%d ", grays_ - 1 - row[c]);
        bs.writall((void *)(const char *)head, head.length());
        c += 1;
        if (c == ncolumns_ || (c & 0x1f) == 0) bs.write((void *)&eol, 1);
      }
    }
    row -= bytes_per_row_;
    n -= 1;
  }
}

void GBitmap::save_rle(ByteStream &bs) {
  // checks
  if (ncolumns_ == 0 || nrows_ == 0) G_THROW(ERR_MSG("GBitmap.not_init"));
  GMonitorLock lock(monitor());
  if (grays_ > 2) G_THROW(ERR_MSG("GBitmap.cant_make_PBM"));
  // header
  GUTF8String head;
  head.format("R4\n%d %d\n", ncolumns_, nrows_);
  bs.writall((void *)(const char *)head, head.length());
  // body
  if (rle_) {
    bs.writall((void *)rle_, rlelength_);
  } else {
    unsigned char *runs = 0;
    GPBuffer<unsigned char> gruns(runs);
    int size = encode(runs, gruns);
    bs.writall((void *)runs, size);
  }
}

// ------ runs

void GBitmap::makerows(int nrows, const int ncolumns, unsigned char *runs,
                       unsigned char *rlerows[]) {
  while (nrows-- > 0) {
    rlerows[nrows] = runs;
    int c;
    for (c = 0; c < ncolumns; c += GBitmap::read_run(runs)) EMPTY_LOOP;
    if (c > ncolumns) G_THROW(ERR_MSG("GBitmap.lost_sync2"));
  }
}

void GBitmap::rle_get_bitmap(const int ncolumns, const unsigned char *&runs,
                             unsigned char *bitmap, const bool invert) {
  const int obyte_def = invert ? 0xff : 0;
  const int obyte_ndef = invert ? 0 : 0xff;
  int mask = 0x80, obyte = 0;
  for (int c = ncolumns; c > 0;) {
    int x = read_run(runs);
    c -= x;
    while ((x--) > 0) {
      if (!(mask >>= 1)) {
        *(bitmap++) = obyte ^ obyte_def;
        obyte = 0;
        mask = 0x80;
        for (; x >= 8; x -= 8) {
          *(bitmap++) = obyte_def;
        }
      }
    }
    if (c > 0) {
      int x = read_run(runs);
      c -= x;
      while ((x--) > 0) {
        obyte |= mask;
        if (!(mask >>= 1)) {
          *(bitmap++) = obyte ^ obyte_def;
          obyte = 0;
          mask = 0x80;
          for (; (x > 8); x -= 8) *(bitmap++) = obyte_ndef;
        }
      }
    }
  }
  if (mask != 0x80) {
    *(bitmap++) = obyte ^ obyte_def;
  }
}

int GBitmap::rle_get_bits(int rowno, unsigned char *bits) const {
  GMonitorLock lock(monitor());
  if (!rle_) return 0;
  if (rowno < 0 || rowno >= nrows_) return 0;
  if (!rlerows_) {
    const_cast<GPBuffer<unsigned char *> &>(grlerows_).resize(nrows_);
    makerows(nrows_, ncolumns_, rle_, const_cast<unsigned char **>(rlerows_));
  }
  int n = 0;
  int p = 0;
  int c = 0;
  unsigned char *runs = rlerows_[rowno];
  while (c < ncolumns_) {
    const int x = read_run(runs);
    if ((c += x) > ncolumns_) c = ncolumns_;
    while (n < c) bits[n++] = p;
    p = 1 - p;
  }
  return n;
}

int GBitmap::rle_get_runs(int rowno, int *rlens) const {
  GMonitorLock lock(monitor());
  if (!rle_) return 0;
  if (rowno < 0 || rowno >= nrows_) return 0;
  if (!rlerows_) {
    const_cast<GPBuffer<unsigned char *> &>(grlerows_).resize(nrows_);
    makerows(nrows_, ncolumns_, rle_, const_cast<unsigned char **>(rlerows_));
  }
  int n = 0;
  int d = 0;
  int c = 0;
  unsigned char *runs = rlerows_[rowno];
  while (c < ncolumns_) {
    const int x = read_run(runs);
    if (n > 0 && !x) {
      n--;
      d = d - rlens[n];
    } else {
      rlens[n++] = (c += x) - d;
      d = c;
    }
  }
  return n;
}

int GBitmap::rle_get_rect(GRect &rect) const {
  GMonitorLock lock(monitor());
  if (!rle_) return 0;
  int area = 0;
  unsigned char *runs = rle_;
  rect.xmin_ = ncolumns_;
  rect.ymin_ = nrows_;
  rect.xmax_ = 0;
  rect.ymax_ = 0;
  int r = nrows_;
  while (--r >= 0) {
    int p = 0;
    int c = 0;
    int n = 0;
    while (c < ncolumns_) {
      const int x = read_run(runs);
      if (x) {
        if (p) {
          if (c < rect.xmin_) rect.xmin_ = c;
          if ((c += x) > rect.xmax_) rect.xmax_ = c - 1;
          n += x;
        } else {
          c += x;
        }
      }
      p = 1 - p;
    }
    area += n;
    if (n) {
      rect.ymin_ = r;
      if (r > rect.ymax_) rect.ymax_ = r;
    }
  }
  if (area == 0) rect.clear();
  return area;
}

// ------ helpers

int GBitmap::encode(unsigned char *&pruns,
                    GPBuffer<unsigned char> &gpruns) const {
  // uncompress rle information
  if (nrows_ == 0 || ncolumns_ == 0) {
    gpruns.resize(0);
    return 0;
  }
  if (!bytes_) {
    unsigned char *runs;
    GPBuffer<unsigned char> gruns(runs, rlelength_);
    memcpy((void *)runs, rle_, rlelength_);
    gruns.swap(gpruns);
    return rlelength_;
  }
  gpruns.resize(0);
  // create run array
  int pos = 0;
  int maxpos = 1024 + ncolumns_ + ncolumns_;
  unsigned char *runs;
  GPBuffer<unsigned char> gruns(runs, maxpos);
  // encode bitmap as rle
  const unsigned char *row = bytes_ + border_;
  int n = nrows_ - 1;
  row += n * bytes_per_row_;
  while (n >= 0) {
    if (maxpos < pos + ncolumns_ + ncolumns_ + 2) {
      maxpos += 1024 + ncolumns_ + ncolumns_;
      gruns.resize(maxpos);
    }

    unsigned char *runs_pos = runs + pos;
    const unsigned char *const runs_pos_start = runs_pos;
    append_line(runs_pos, row, ncolumns_);
    pos += (std::size_t)runs_pos - (std::size_t)runs_pos_start;
    row -= bytes_per_row_;
    n -= 1;
  }
  // return result
  gruns.resize(pos);
  gpruns.swap(gruns);
  return pos;
}

void GBitmap::decode(unsigned char *runs) {
  // initialize pixel array
  if (nrows_ == 0 || ncolumns_ == 0) G_THROW(ERR_MSG("GBitmap.not_init"));
  bytes_per_row_ = ncolumns_ + border_;
  if (runs == 0) G_THROW(ERR_MSG("GBitmap.null_arg"));
  std::size_t npixels = nrows_ * bytes_per_row_ + border_;
  if (!bytes_data_) {
    gbytes_data_.resize(npixels);
    bytes_ = bytes_data_;
  }
  gbytes_data_.clear();
  gzerobuffer_ = zeroes(bytes_per_row_ + border_);
  // interpret runs data
  int c, n;
  unsigned char p = 0;
  unsigned char *row = bytes_data_ + border_;
  n = nrows_ - 1;
  row += n * bytes_per_row_;
  c = 0;
  while (n >= 0) {
    int x = read_run(runs);
    if (c + x > ncolumns_) G_THROW(ERR_MSG("GBitmap.lost_sync2"));
    while (x-- > 0) row[c++] = p;
    p = 1 - p;
    if (c >= ncolumns_) {
      c = 0;
      p = 0;
      row -= bytes_per_row_;
      n -= 1;
    }
  }
  // Free rle data possibly attached to this bitmap
  grle_.resize(0);
  grlerows_.resize(0);
  rlelength_ = 0;
#ifndef NDEBUG
  check_border();
#endif
}

class GBitmap::ZeroBuffer : public GPEnabled {
 public:
  ZeroBuffer(const unsigned int zerosize);
  unsigned char *zerobuffer;
  GPBuffer<unsigned char> gzerobuffer;
};

GBitmap::ZeroBuffer::ZeroBuffer(const unsigned int zerosize)
    : gzerobuffer(zerobuffer, zerosize) {
  gzerobuffer.clear();
  GBitmap::zerobuffer_ = zerobuffer;
  GBitmap::zerosize_ = zerosize;
}

static const unsigned char static_zerobuffer[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 32
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 64
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 96
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 128
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 160
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 192
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 234
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 256
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 288
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 320
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 352
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 384
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 416
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 448
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 480
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 512
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 544
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 576
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 608
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 640
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 672
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 704
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 736
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 768
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 800
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 832
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 864
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 896
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 928
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 960
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 992
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+32
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+64
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+96
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+128
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+160
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+192
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+234
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+256
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+288
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+320
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+352
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+384
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+416
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+448
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+480
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+512
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+544
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+576
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+608
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+640
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+672
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+704
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+736
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+768
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+800
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+832
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+864
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+896
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+928
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+960
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1024+992
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+32
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+64
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+96
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+128
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+160
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+192
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+234
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+256
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+288
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+320
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+352
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+384
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+416
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+448
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+480
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+512
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+544
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+576
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+608
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+640
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+672
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+704
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+736
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+768
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+800
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+832
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+864
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+896
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+928
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+960
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 2048+992
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+32
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+64
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+96
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+128
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+160
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+192
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+234
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+256
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+288
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+320
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+352
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+384
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+416
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+448
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+480
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+512
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+544
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+576
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+608
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+640
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+672
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+704
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+736
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+768
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+800
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+832
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+864
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+896
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+928
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+960
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 3072+992
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // 4096

int GBitmap::zerosize_ = sizeof(static_zerobuffer);
unsigned char *GBitmap::zerobuffer_ =
    const_cast<unsigned char *>(static_zerobuffer);

GP<GBitmap::ZeroBuffer> GBitmap::zeroes(int required) {
  GMonitorLock lock(&monitors[0]);  // any monitor would do
  static GP<GBitmap::ZeroBuffer> gzerobuffer;
  if (zerosize_ < required) {
    int z;
    for (z = zerosize_; z < required; z <<= 1) EMPTY_LOOP;
    z = (z + 0xfff) & (~0xfff);
    gzerobuffer = new GBitmap::ZeroBuffer((unsigned int)z);
  }
  return gzerobuffer;
}

// Fills a bitmap with the given value
void GBitmap::fill(unsigned char value) {
  GMonitorLock lock(monitor());
  for (unsigned int y = 0; y < rows(); y++) {
    unsigned char *bm_y = (*this)[y];
    for (unsigned int x = 0; x < columns(); x++) bm_y[x] = value;
  }
}

void GBitmap::append_long_run(unsigned char *&data, int count) {
  while (count > MAXRUNSIZE) {
    data[0] = data[1] = 0xff;
    data[2] = 0;
    data += 3;
    count -= MAXRUNSIZE;
  }
  if (count < RUNOVERFLOWVALUE) {
    data[0] = count;
    data += 1;
  } else {
    data[0] = (count >> 8) + GBitmap::RUNOVERFLOWVALUE;
    data[1] = (count & 0xff);
    data += 2;
  }
}

void GBitmap::append_line(unsigned char *&data, const unsigned char *row,
                          const int rowlen, bool invert) {
  const unsigned char *rowend = row + rowlen;
  bool p = !invert;
  while (row < rowend) {
    int count = 0;
    if ((p = !p)) {
      if (*row)
        for (++count, ++row; (row < rowend) && *row; ++count, ++row) EMPTY_LOOP;
    } else if (!*row) {
      for (++count, ++row; (row < rowend) && !*row; ++count, ++row) EMPTY_LOOP;
    }
    append_run(data, count);
  }
}

#if 0
static inline int
GetRowTDLRNR(
  GBitmap &bit,const int row, const unsigned char *&startptr,
  const unsigned char *&stopptr)
{
  stopptr=(startptr=bit[row])+bit.columns();
  return 1;
}

static inline int
GetRowTDLRNR(
  GBitmap &bit,const int row, const unsigned char *&startptr,
  const unsigned char *&stopptr)
{
  stopptr=(startptr=bit[row])+bit.columns();
  return 1;
}

static inline int
GetRowTDRLNR(
  GBitmap &bit,const int row, const unsigned char *&startptr,
  const unsigned char *&stopptr)
{
  startptr=(stopptr=bit[row]-1)+bit.columns();
  return -1;
}
#endif  // 0

GP<GBitmap> GBitmap::rotate(int count) {
  GP<GBitmap> newbitmap = this;
  count = count & 3;
  if (count) {
    if (count & 0x01) {
      newbitmap = new GBitmap(ncolumns_, nrows_);
    } else {
      newbitmap = new GBitmap(nrows_, ncolumns_);
    }
    GMonitorLock lock(monitor());
    if (!bytes_data_) uncompress();
    GBitmap &dbitmap = *newbitmap;
    dbitmap.set_grays(grays_);
    switch (count) {
      case 3:  // rotate 90 counter clockwise
      {
        const int lastrow = dbitmap.rows() - 1;
        for (int y = 0; y < nrows_; y++) {
          const unsigned char *r = operator[](y);
          for (int x = 0, xnew = lastrow; xnew >= 0; x++, xnew--) {
            dbitmap[xnew][y] = r[x];
          }
        }
      } break;
      case 2:  // rotate 180 counter clockwise
      {
        const int lastrow = dbitmap.rows() - 1;
        const int lastcolumn = dbitmap.columns() - 1;
        for (int y = 0, ynew = lastrow; ynew >= 0; y++, ynew--) {
          const unsigned char *r = operator[](y);
          unsigned char *d = dbitmap[ynew];
          for (int xnew = lastcolumn; xnew >= 0; r++, --xnew) {
            d[xnew] = *r;
          }
        }
      } break;
      case 1:  // rotate 270 counter clockwise
      {
        const int lastcolumn = dbitmap.columns() - 1;
        for (int y = 0, ynew = lastcolumn; ynew >= 0; y++, ynew--) {
          const unsigned char *r = operator[](y);
          for (int x = 0; x < ncolumns_; x++) {
            dbitmap[x][ynew] = r[x];
          }
        }
      } break;
    }
    if (grays_ == 2) {
      compress();
      dbitmap.compress();
    }
  }
  return newbitmap;
}

#ifndef NDEBUG
void GBitmap::check_border() const {
  int col;
  if (bytes_) {
    const unsigned char *p = (*this)[-1];
    for (col = -border_; col < ncolumns_ + border_; col++)
      if (p[col]) G_THROW(ERR_MSG("GBitmap.zero_damaged"));
    for (int row = 0; row < nrows; row++) {
      p = (*this)[row];
      for (col = -border_; col < 0; col++)
        if (p[col]) G_THROW(ERR_MSG("GBitmap.left_damaged"));
      for (col = ncolumns_; col < ncolumns + border_; col++)
        if (p[col]) G_THROW(ERR_MSG("GBitmap.right_damaged"));
    }
  }
}
#endif

}  // namespace DJVU
