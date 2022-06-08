//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------

#ifndef _DJVUIMAGE_H
#define _DJVUIMAGE_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


/** @name DjVuImage.h

    Files #"DjVuImage.h"# and #"DjVuImage.cpp"# implement \Ref{DjVuImage}
    class produced as a result of decoding of DjVu files. In the previous
    version of the library both the rendering {\bf and} decoding have been
    handled by \Ref{DjVuImage}. This is no longer true. Now the
    \Ref{DjVuDocument} and \Ref{DjVuFile} classes handle decoding of both
    single page and multi page documents.

    For compatibility reasons though, we still support old-style decoding
    interface through the \Ref{DjVuImage} class for single page documents
    {\em only}. As before, the display programs can call the decoding
    function from a separate thread.  The user interface thread may call
    the rendering functions at any time. Rendering will be performed using
    the most recent data generated by the decoding thread. This multithreaded
    capability enables progressive display of remote images.

    {\bf Creating DjVu images} --- Class \Ref{DjVuImage} does not provide a
    direct way to create a DjVu image.  The recommended procedure consists of
    directly writing the required chunks into an \Ref{IFFByteStream} as
    demonstrated in program \Ref{djvumake}.  Dealing with too many encoding
    issues (such as chunk ordering and encoding quality) would indeed make the
    decoder unnecessarily complex.

    {\bf ToDo: Layered structure} --- Class #DjVuImage# currently contains an
    unstructured collection of smart pointers to various data structures.
    Although it simplifies the rendering routines, this choice does not
    reflect the layered structure of DjVu images and does not leave much room
    for evolution.  We should be able to do better.

    @memo
    Decoding DjVu and IW44 images.
    @author
    L\'eon Bottou <leonb@research.att.com> - initial implementation
    Andrei Erofeev <eaf@geocities.com> - multipage support
*/
//@{



#include "DjVuFile.h"
#include "DjVuAnno.h"
#include "GRect.h"

namespace DJVU {

/* Obsolete class included for backward compatibility. */

class DjVuInterface
{
public:
  virtual ~DjVuInterface();
  virtual void notify_chunk(const char *chkid, const char *msg) = 0;
  virtual void notify_relayout(void) = 0;
  virtual void notify_redisplay(void) = 0;
};


/** Main DjVu Image data structure.  This class defines the internal
    representation of a DjVu image.  This representation consists of a few
    pointers referencing the various components of the DjVu image.  These
    components are created and populated by the decoding function.  The
    rendering functions then can use the available components to compute a
    pixel representation of the desired segment of the DjVu image. */

class DJVUAPI DjVuImage : public DjVuPort
{
protected:
  DjVuImage(void);
public:
  enum { NOINFO, NOTEXT=1, NOMAP=4, NOMETA=8 };
  // CONSTRUCTION
  /** @name Construction. */
  //@{
  /** Creates an empty DjVu image. After the image has been constructed,
      it may be connected to an existing \Ref{DjVuFile} or left as is.

      In the former case #DjVuImage# will look for its decoded components
      (like #Sjbz# or #BG44#) by decending the hierarchy of \Ref{DjVuFile}s
      starting from the one passed to \Ref{connect}().

      In the latter case you can use \Ref{decode}() function to decode
      {\bf single-page} DjVu documents in the old-style way. */
  static GP<DjVuImage> create(void) {return new DjVuImage();}

  /** Connects this #DjVuImage# to the passed \Ref{DjVuFile}. The #DjVuImage#
      will use this \Ref{DjVuFile} to retrieve components necessary for
      decoding. It will also connect itself to \Ref{DjVuFile} using the
      communication mechanism provided by \Ref{DjVuPort} and \Ref{DjVuPortcaster}.
      This will allow it to receive and relay messages and requests generated
      by the passed \Ref{DjVuFile} and any file included into it. */
  void		connect(const GP<DjVuFile> & file);

  /** This combines the above two steps for simplier code operations. */
  static GP<DjVuImage> create(const GP<DjVuFile> &file)
  { const GP<DjVuImage> retval=create(); retval->connect(file); return retval; }
      
  //@}

  // COMPONENTS
  /** @name Components. */
  //@{
  /** Returns a pointer to a DjVu information component.
      This function returns a null pointer until the decoder
      actually processes an #"INFO"# chunk. */
  GP<DjVuInfo>    get_info() const;
  /** Returns a pointer to the IW44 encoded background component of a DjVu
      image.  This function returns a null pointer until the decoder actually
      processes an #"BG44"# chunk. */
  GP<IW44Image>    get_bg44() const;
  /** Returns a pointer to the raw background component of a DjVu image. The
      background component is used for JPEG encoded backgrounds.  This
      function returns a null pointer until the decoder actually processes an
      #"BGjp"# chunk. */
  GP<GPixmap>     get_bgpm() const;
  /** Returns a pointer to the mask of the foreground component of a DjVu
      image. The mask of the foreground component is always a JB2 image in
      this implementation. This function returns a null pointer until the
      decoder actually processes an #"Sjbz"# chunk. */
  GP<JB2Image>    get_fgjb() const;
  /** Returns a pointer to the colors of the foreground component of a DjVu
      image. The mask of the foreground component is always a small pixmap in
      this implementation. This function returns a null pointer until the
      decoder actually processes an #"FG44"# chunk. */
  GP<GPixmap>     get_fgpm() const;
  /** Returns a pointer to a palette object containing colors for the 
      foreground components of a DjVu image.  These colors are only
      pertinent with respect to the JB2Image. */
  GP<DjVuPalette> get_fgbc() const;
  /** Returns a pointer to a ByteStream containing all the annotation
      chunks collected so far for this image.  Individual chunks can be
      retrieved using \Ref{IFFByteStream}. Returns NULL if no chunks have been
      collected yet. */
  GP<ByteStream> get_anno() const;
  /** Returns a pointer to a ByteStream containing all the hidden text.
      Returns NULL if no chunks have been collected yet. */
  GP<ByteStream> get_text() const;
  /** Returns a pointer to a ByteStream containing all the metadata.
      Returns NULL if no chunks have been collected yet. */
  GP<ByteStream> get_meta() const;
  //@}

  // NEW STYLE DECODING
  /** @name New style decoding. */
  //@{
  /** The decoder is now started when the image is created
      by function \Ref{DjVuDocument::get_page} in \Ref{DjVuDocument}. 
      This function waits until the decoding thread terminates
      and returns TRUE if the image has been successfully decoded. */
  bool wait_for_complete_decode(void);
  //@}
  
  // OLD STYLE DECODING
  /** @name Old style decoding (backward compatibility). */
  //@{
  /** This function is here for backward compatibility. Now, with the
      introduction of multipage DjVu documents, the decoding is handled
      by \Ref{DjVuFile} and \Ref{DjVuDocument} classes. For single page
      documents though, we still have this wrapper. */
  void decode(ByteStream & str, DjVuInterface *notifier=0);
  //@}
  
  // UTILITIES
  /** @name Utilities */
  //@{
  /** Returns the width of the DjVu image. This function just extracts this
      information from the DjVu information component. It returns zero if such
      a component is not yet available. This gives rotated width if there is any
      rotation of image. If you need real width, use #get_real_width()#.*/
  int get_width() const;
  /** Returns the height of the DjVu image. This function just extracts this
      information from the DjVu information component. It returns zero if such
      a component is not yet available. This gives rotated height if there is any
      rotation of image. If you need real width, use #get_real_height()#.*/
  int get_height() const;
  /** Returns the width of the DjVu image. This function just extracts this
      information from the DjVu information component. It returns zero if such
      a component is not yet available.*/
  int get_real_width() const;
  /** Returns the height of the DjVu image. This function just extracts this
      information from the DjVu information component. It returns zero if such
      a component is not yet available.*/
  int get_real_height() const;
  /** Returns the format version the DjVu data. This function just extracts
      this information from the DjVu information component. It returns zero if
      such a component is not yet available.  This version number should
      be compared with the \Ref{DjVu version constants}. */
  int get_version() const;
  /** Returns the resolution of the DjVu image. This information is given in
      pixels per 2.54 cm.  Display programs can use this information to
      determine the natural magnification to use for rendering a DjVu
      image. */
  int get_dpi() const;
  /** Same as \Ref{get_dpi}() but instead of precise value returns the closest
      "standard" one: 25, 50, 75, 100, 150, 300, 600. If dpi is greater than
      700, it's returned as is. */
  int get_rounded_dpi() const;
  /** Returns the gamma coefficient of the display for which the image was
      designed.  The rendering functions can use this information in order to
      perform color correction for the intended display device. */
  double get_gamma() const;
  /** Returns a MIME type string describing the DjVu data.  This information
      is auto-sensed by the decoder.  The MIME type can be #"image/djvu"# or
      #"image/iw44"# depending on the data stream. */
  GUTF8String get_mimetype() const;
  /** Returns a short string describing the DjVu image.
      Example: #"2500x3223 in 23.1 Kb"#. */
  GUTF8String get_short_description() const;
  /** Returns a verbose description of the DjVu image.  This description lists
      all the chunks with their size and a brief comment, as shown in the
      following example.
      \begin{verbatim}
      DJVU Image (2325x3156) version 17:
       0.0 Kb   'INFO'  Page information.
       17.3 Kb  'Sjbz'  JB2 foreground mask (2325x3156)
       2.5 Kb   'BG44'  IW44 background (775x1052)
       1.0 Kb   'FG44'  IW44 foreground colors (194x263)
       3.0 Kb   'BG44'  IW44 background (part 2).
       0.9 Kb   'BG44'  IW44 background (part 3).
       7.1 Kb   'BG44'  IW44 background (part 4).
      Compression ratio: 676 (31.8 Kb)
      \end{verbatim} */
  GUTF8String get_long_description() const;
  /** Returns pointer to \Ref{DjVuFile} which contains this image in
      compressed form. */
  GP<DjVuFile> get_djvu_file(void) const;
  /// Write out a DjVuXML object tag and map tag.
  void writeXML(ByteStream &str_out,const GURL &doc_url, const int flags=0) const;
  /// Write out a DjVuXML object tag and map tag.
  void writeXML(ByteStream &str_out) const;
  /// Get a DjVuXML object tag and map tag.
  GUTF8String get_XML(const GURL &doc_url, const int flags=0) const;
  /// Get a DjVuXML object tag and map tag.
  GUTF8String get_XML(void) const;
  //@}

  // CHECKING
  /** @name Checking for legal DjVu files. */
  //@{
  /** This function returns true if this object contains a well formed {\em
      Photo DjVu Image}. Calling function #get_pixmap# on a well formed photo
      image should always return a non zero value.  Note that function
      #get_pixmap# works as soon as sufficient information is present,
      regardless of the fact that the image follows the rules or not. */
  int is_legal_photo() const;
  /** This function returns true if this object contains a well formed {\em
      Bilevel DjVu Image}.  Calling function #get_bitmap# on a well formed
      bilevel image should always return a non zero value.  Note that function
      #get_bitmap# works as soon as a foreground mask component is present,
      regardless of the fact that the image follows the rules or not. */
  int is_legal_bilevel() const;
  /** This function returns true if this object contains a well formed {\em
      Compound DjVu Image}.  Calling function #get_bitmap# or #get_pixmap# on
      a well formed compound DjVu image should always return a non zero value.
      Note that functions #get_bitmap# or #get_pixmap# works as soon as
      sufficient information is present, regardless of the fact that the image
      follows the rules or not.  */
  int is_legal_compound() const;
  //@}

  // RENDERING 
  /** @name Rendering.  
      All these functions take two rectangles as argument.  Conceptually,
      these function first render the whole image into a rectangular area
      defined by rectangle #all#.  The relation between this rectangle and the
      image size define the appropriate scaling.  The rendering function then
      extract the subrectangle #rect# and return the corresponding pixels as a
      #GPixmap# or #GBitmap# object.  The #all# and #rect# should take the any
      rotation in to effect, The actual implementation performs these
      two operation simultaneously for obvious efficiency reasons.  The best
      rendering speed is achieved by making sure that the size of rectangle
      #all# and the size of the DjVu image are related by an integer ratio. */
  //@{
  /** Renders the image and returns a color pixel image.  Rectangles #rect#
      and #all# are used as explained above. Color correction is performed
      according to argument #gamma#, which represents the gamma coefficient of
      the display device on which the pixmap will be rendered.  The default
      value, zero, means that no color correction should be performed. 
      This function returns a null pointer if there is not enough information
      in the DjVu image to properly render the desired image. */
  GP<GPixmap>  get_pixmap(const GRect &rect, const GRect &all, 
                          double gamma, GPixel white) const;
  GP<GPixmap>  get_pixmap(const GRect &rect, const GRect &all, 
                          double gamma=0) const;
  /** Renders the mask of the foreground layer of the DjVu image.  This
      functions is a wrapper for \Ref{JB2Image::get_bitmap}.  Argument #align#
      specified the alignment of the rows of the returned images.  Setting
      #align# to #4#, for instance, will adjust the bitmap border in order to
      make sure that each row of the returned image starts on a word (four
      byte) boundary.  This function returns a null pointer if there is not
      enough information in the DjVu image to properly render the desired
      image. */
  GP<GBitmap>  get_bitmap(const GRect &rect, const GRect &all, 
                          int align = 1) const;
  /** Renders the background layer of the DjVu image.  Rectangles #rect# and
      #all# are used as explained above. Color correction is performed
      according to argument #gamma#, which represents the gamma coefficient of
      the display device on which the pixmap will be rendered.  The default
      value, zero, means that no color correction should be performed.  This
      function returns a null pointer if there is not enough information in
      the DjVu image to properly render the desired image. */
  GP<GPixmap>  get_bg_pixmap(const GRect &rect, const GRect &all, 
                             double gamma, GPixel white) const;
  GP<GPixmap>  get_bg_pixmap(const GRect &rect, const GRect &all, 
                             double gamma=0) const;
  /** Renders the foreground layer of the DjVu image.  Rectangles #rect# and
      #all# are used as explained above. Color correction is performed
      according to argument #gamma#, which represents the gamma coefficient of
      the display device on which the pixmap will be rendered.  The default
      value, zero, means that no color correction should be performed.  This
      function returns a null pointer if there is not enough information in
      the DjVu image to properly render the desired image. */
  GP<GPixmap>  get_fg_pixmap(const GRect &rect, const GRect &all, 
                             double gamma, GPixel white) const;
  GP<GPixmap>  get_fg_pixmap(const GRect &rect, const GRect &all, 
                             double gamma=0) const;


  /** set the rotation count(angle) in counter clock wise for the image
    values (0,1,2,3) correspond to (0,90,180,270) degree rotation*/
  void set_rotate(int count=0);
  /** returns the rotation count*/
  int get_rotate() const;
  /** returns decoded annotations in DjVuAnno object in which all hyperlinks
      and hilighted areas are rotated as per rotation setting*/
  GP<DjVuAnno> get_decoded_anno();
  /** maps the given #rect# from rotated co-ordinates to unrotated document 
      co-ordinates*/
  void map(GRect &rect) const;
  /** unmaps the given #rect# from unrotated document co-ordinates to rotated  
      co-ordinates*/
  void unmap(GRect &rect) const;
  /** maps the given #x#, #y# from rotated co-ordinates to unrotated document 
      co-ordinates*/
  void map(int &x, int &y) const;
  /** unmaps the given #x#, #y# from unrotated document co-ordinates to rotated  
      co-ordinates*/
  void unmap(int &x, int &y) const;



  //@}

  // Inherited from DjVuPort.
  virtual void notify_chunk_done(const DjVuPort *, const GUTF8String &name);

  // SUPERSEDED
  GP<GPixmap>  get_pixmap(const GRect &r, int s=1, double g=0) const;
  GP<GPixmap>  get_pixmap(const GRect &r, int s, double g, GPixel w) const;
  GP<GBitmap>  get_bitmap(const GRect &r, int s=1, int align = 1) const;
  GP<GPixmap>  get_bg_pixmap(const GRect &r, int s=1, double g=0) const;
  GP<GPixmap>  get_bg_pixmap(const GRect &r, int s, double g, GPixel w) const;
  GP<GPixmap>  get_fg_pixmap(const GRect &r, int s=1, double g=0) const;
  GP<GPixmap>  get_fg_pixmap(const GRect &r, int s, double g, GPixel w) const;
private:
  GP<DjVuFile>		file;
  int			rotate_count;
  bool			relayout_sent;
  
  // HELPERS
  int stencil(GPixmap *pm, const GRect &rect, int s, double g, GPixel w) const;
  GP<DjVuInfo>		get_info(const GP<DjVuFile> & file) const;
  GP<IW44Image>		get_bg44(const GP<DjVuFile> & file) const;
  GP<GPixmap>		get_bgpm(const GP<DjVuFile> & file) const;
  GP<JB2Image>		get_fgjb(const GP<DjVuFile> & file) const;
  GP<GPixmap>		get_fgpm(const GP<DjVuFile> & file) const;
  GP<DjVuPalette>      get_fgbc(const GP<DjVuFile> & file) const;
  void init_rotate(const DjVuInfo &info);
};


inline GP<DjVuFile>
DjVuImage::get_djvu_file(void) const
{
   return file;
}

//@}



// ----- THE END

}
using namespace DJVU;
#endif
