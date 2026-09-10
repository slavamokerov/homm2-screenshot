// Software implementation of the raster/compat types (see rastercompat.hpp).
// Compiled only for the headless WASM build. A self-contained PNG encoder uses
// zlib (linked via -s USE_ZLIB=1). No Qt, no DOM, no filesystem needed.

#include "rastercompat.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <zlib.h>

namespace {

std::vector<uint8_t> encodePng( const uint8_t * rgba, int w, int h );
std::vector<uint8_t> encodeIndexedPng( const std::vector<uint8_t> & idx, const std::vector<QRgb> & ct, int w, int h );

inline uint32_t blendPixel( uint32_t a, uint32_t b ) // Qt-compatible source-over (premultiplied, per-product rounding)
{
    const uint32_t sa = a >> 24;
    if ( sa == 0 )
        return b;
    if ( sa == 255 )
        return a;
    const uint32_t ba = b >> 24;
    const uint32_t inv = 255 - sa;

    const uint32_t psr = ( ( a >> 16 & 0xFF ) * sa + 127 ) / 255;
    const uint32_t psg = ( ( a >> 8 & 0xFF ) * sa + 127 ) / 255;
    const uint32_t psb = ( ( a & 0xFF ) * sa + 127 ) / 255;
    const uint32_t pdr = ( ( b >> 16 & 0xFF ) * ba + 127 ) / 255;
    const uint32_t pdg = ( ( b >> 8 & 0xFF ) * ba + 127 ) / 255;
    const uint32_t pdb = ( ( b & 0xFF ) * ba + 127 ) / 255;

    const uint32_t pr = psr + ( pdr * inv + 127 ) / 255;
    const uint32_t pg = psg + ( pdg * inv + 127 ) / 255;
    const uint32_t pb = psb + ( pdb * inv + 127 ) / 255;
    const uint32_t outA = sa + ( ba * inv + 127 ) / 255;

    if ( outA == 0 )
        return b;
    const uint32_t r = std::min<uint32_t>( 255, ( pr * 255 + outA / 2 ) / outA );
    const uint32_t g = std::min<uint32_t>( 255, ( pg * 255 + outA / 2 ) / outA );
    const uint32_t bl = std::min<uint32_t>( 255, ( pb * 255 + outA / 2 ) / outA );
    return ( outA << 24 ) | ( r << 16 ) | ( g << 8 ) | bl;
}

inline uint32_t clampTo( int v, int lo, int hi )
{
    return v < lo ? ( uint32_t )lo : ( v > hi ? ( uint32_t )hi : ( uint32_t )v );
}

bool writePng( const uint8_t * rgba, int w, int h, const std::string & fileName )
{
    const std::vector<uint8_t> out = encodePng( rgba, w, h );
    if ( out.empty() )
        return false;
    FILE * f = std::fopen( fileName.c_str(), "wb" );
    if ( !f )
        return false;
    const size_t wr = std::fwrite( out.data(), 1, out.size(), f );
    std::fclose( f );
    return wr == out.size();
}

std::vector<uint8_t> encodePng( const uint8_t * rgba, int w, int h )
{
    // Assemble raw scanlines with a leading filter byte (0 = none).
    std::vector<uint8_t> raw;
    raw.reserve( ( size_t )w * 4 * h + h );
    for ( int y = 0; y < h; ++y ) {
        raw.push_back( 0 );
        raw.insert( raw.end(), rgba + ( size_t )y * w * 4, rgba + ( size_t )y * w * 4 + ( size_t )w * 4 );
    }

    uLongf zlen = compressBound( ( uLong )raw.size() );
    std::vector<uint8_t> z( zlen );
    if ( compress2( z.data(), &zlen, raw.data(), ( uLong )raw.size(), 9 ) != Z_OK )
        return {};

    auto chunk = [&]( uint8_t * dst, const char type[4], const uint8_t * data, size_t n ) {
        uint32_t len = ( uint32_t )n;
        dst[0] = ( uint8_t )( len >> 24 );
        dst[1] = ( uint8_t )( len >> 16 );
        dst[2] = ( uint8_t )( len >> 8 );
        dst[3] = ( uint8_t )len;
        std::memcpy( dst + 4, type, 4 );
        if ( n )
            std::memcpy( dst + 8, data, n );
        uint32_t crc = ( uint32_t )0xFFFFFFFFu;
        for ( size_t i = 0; i < n + 4; ++i ) {
            crc ^= ( i < 4 ? ( uint8_t )type[i] : data[i - 4] );
            for ( int k = 0; k < 8; ++k )
                crc = ( crc >> 1 ) ^ ( 0xEDB88320u & -( crc & 1 ) );
        }
        crc = crc ^ 0xFFFFFFFFu;
        dst[8 + n] = ( uint8_t )( crc >> 24 );
        dst[9 + n] = ( uint8_t )( crc >> 16 );
        dst[10 + n] = ( uint8_t )( crc >> 8 );
        dst[11 + n] = ( uint8_t )crc;
    };

    std::vector<uint8_t> out;
    out.reserve( z.size() + 1024 );
    const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    out.insert( out.end(), sig, sig + 8 );

    uint8_t ihdr[13];
    ihdr[0] = ( uint8_t )( w >> 24 ); ihdr[1] = ( uint8_t )( w >> 16 ); ihdr[2] = ( uint8_t )( w >> 8 ); ihdr[3] = ( uint8_t )w;
    ihdr[4] = ( uint8_t )( h >> 24 ); ihdr[5] = ( uint8_t )( h >> 16 ); ihdr[6] = ( uint8_t )( h >> 8 ); ihdr[7] = ( uint8_t )h;
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 6;   // color type: RGBA
    ihdr[10] = 0;  // compression
    ihdr[11] = 0;  // filter
    ihdr[12] = 0;  // interlace
    uint8_t c[25];
    chunk( c, "IHDR", ihdr, 13 );
    out.insert( out.end(), c, c + 25 );
    std::vector<uint8_t> d( 8 + zlen + 4 );
    chunk( d.data(), "IDAT", z.data(), zlen );
    out.insert( out.end(), d.begin(), d.end() );
    uint8_t e[12];
    chunk( e, "IEND", nullptr, 0 );
    out.insert( out.end(), e, e + 12 );
    return out;
}

std::vector<uint8_t> encodeIndexedPng( const std::vector<uint8_t> & idx, const std::vector<QRgb> & ct, int w, int h )
{
    if ( w <= 0 || h <= 0 || ct.empty() )
        return {};

    // Raw scanlines with a leading filter byte (0 = none), 8-bit indexed.
    std::vector<uint8_t> raw;
    raw.reserve( ( size_t )w * h + h );
    for ( int y = 0; y < h; ++y ) {
        raw.push_back( 0 );
        raw.insert( raw.end(), idx.begin() + ( size_t )y * w, idx.begin() + ( size_t )y * w + w );
    }

    uLongf zlen = compressBound( ( uLong )raw.size() );
    std::vector<uint8_t> z( zlen );
    if ( compress2( z.data(), &zlen, raw.data(), ( uLong )raw.size(), 9 ) != Z_OK )
        return {};

    const int n = ct.size() > 256 ? 256 : ( int )ct.size();
    std::vector<uint8_t> plte( static_cast<size_t>( n ) * 3 );
    std::vector<uint8_t> trns;
    bool anyAlpha = false;
    for ( int i = 0; i < n; ++i ) {
        const QRgb c = ct[i];
        plte[static_cast<size_t>( i ) * 3 + 0] = ( uint8_t )( c >> 16 & 0xFF );
        plte[static_cast<size_t>( i ) * 3 + 1] = ( uint8_t )( c >> 8 & 0xFF );
        plte[static_cast<size_t>( i ) * 3 + 2] = ( uint8_t )( c & 0xFF );
        const uint8_t a = ( uint8_t )( c >> 24 & 0xFF );
        if ( a != 0xFF ) {
            anyAlpha = true;
            trns.push_back( a );
        }
    }
    // PNG tRNS needs one alpha per palette entry, padded to 255.
    if ( anyAlpha ) {
        trns.resize( n, 0xFF );
    }
    else {
        trns.clear();
    }

    auto chunk = []( std::vector<uint8_t> & out, const char type[4], const uint8_t * data, size_t n ) {
        uint8_t hdr[8];
        const uint32_t len = ( uint32_t )n;
        hdr[0] = ( uint8_t )( len >> 24 );
        hdr[1] = ( uint8_t )( len >> 16 );
        hdr[2] = ( uint8_t )( len >> 8 );
        hdr[3] = ( uint8_t )len;
        std::memcpy( hdr + 4, type, 4 );
        out.insert( out.end(), hdr, hdr + 8 );
        if ( n )
            out.insert( out.end(), data, data + n );
        uint32_t crc = ( uint32_t )0xFFFFFFFFu;
        for ( size_t i = 0; i < n + 4; ++i ) {
            crc ^= ( i < 4 ? ( uint8_t )type[i] : data[i - 4] );
            for ( int k = 0; k < 8; ++k )
                crc = ( crc >> 1 ) ^ ( 0xEDB88320u & -( crc & 1 ) );
        }
        crc = crc ^ 0xFFFFFFFFu;
        const uint8_t tail[4] = { ( uint8_t )( crc >> 24 ), ( uint8_t )( crc >> 16 ), ( uint8_t )( crc >> 8 ), ( uint8_t )crc };
        out.insert( out.end(), tail, tail + 4 );
    };

    std::vector<uint8_t> out;
    out.reserve( z.size() + 1024 );
    const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    out.insert( out.end(), sig, sig + 8 );

    uint8_t ihdr[13];
    ihdr[0] = ( uint8_t )( w >> 24 ); ihdr[1] = ( uint8_t )( w >> 16 ); ihdr[2] = ( uint8_t )( w >> 8 ); ihdr[3] = ( uint8_t )w;
    ihdr[4] = ( uint8_t )( h >> 24 ); ihdr[5] = ( uint8_t )( h >> 16 ); ihdr[6] = ( uint8_t )( h >> 8 ); ihdr[7] = ( uint8_t )h;
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 3;   // color type: indexed
    ihdr[10] = 0;  // compression
    ihdr[11] = 0;  // filter
    ihdr[12] = 0;  // interlace
    chunk( out, "IHDR", ihdr, 13 );
    chunk( out, "PLTE", plte.data(), plte.size() );
    if ( anyAlpha )
        chunk( out, "tRNS", trns.data(), trns.size() );
    chunk( out, "IDAT", z.data(), zlen );
    uint8_t e[12];
    chunk( out, "IEND", nullptr, 0 );
    return out;
}

} // namespace

// ---- QImage -----------------------------------------------------------------
QImage::QImage( int w, int h, Format fmt )
    : _w( w < 0 ? 0 : w ), _h( h < 0 ? 0 : h ), _fmt( fmt )
{
    const size_t n = ( size_t )_w * _h;
    if ( isRgb() ) {
        _argb.assign( n, 0 ); // transparent ARGB32
        _idx.clear();
    }
    else if ( _fmt == Format_Indexed8 ) {
        _idx.assign( n, 0 );
        _ct.assign( 256, 0xFFFFFFFFu );
        _argb.clear();
    }
    else {
        _fmt = Format_Invalid;
        _idx.clear();
        _argb.clear();
    }
}

void QImage::fillValue( uint32_t v )
{
    if ( isRgb() )
        std::fill( _argb.begin(), _argb.end(), v );
    else if ( _fmt == Format_Indexed8 )
        std::fill( _idx.begin(), _idx.end(), ( uint8_t )v );
}

void QImage::setPixel( int x, int y, uint32_t v )
{
    if ( x < 0 || y < 0 || x >= _w || y >= _h )
        return;
    if ( isRgb() )
        _argb[( size_t )y * _w + x] = v;
    else if ( _fmt == Format_Indexed8 )
        _idx[( size_t )y * _w + x] = ( uint8_t )v;
}

void QImage::setPixelColor( int x, int y, const QColor & c )
{
    setPixel( x, y, c.rgba() );
}

QColor QImage::pixelColor( int x, int y ) const
{
    if ( x < 0 || y < 0 || x >= _w || y >= _h )
        return QColor( 0, 0, 0, 0 );
    if ( isRgb() )
        return QColor::fromRgba( _argb[( size_t )y * _w + x] );
    if ( _fmt == Format_Indexed8 ) {
        const uint8_t i = _idx[( size_t )y * _w + x];
        return QColor::fromRgb( _ct.size() > i ? _ct[i] : 0xFF000000u );
    }
    return QColor( 0, 0, 0, 0 );
}

uint32_t QImage::pixel( int x, int y ) const
{
    return pixelColor( x, y ).rgba();
}

int QImage::pixelIndex( int x, int y ) const
{
    if ( x < 0 || y < 0 || x >= _w || y >= _h )
        return 0;
    if ( _fmt == Format_Indexed8 )
        return _idx[( size_t )y * _w + x];
    return 0;
}

void QImage::setColor( int i, QRgb c )
{
    if ( _fmt == Format_Indexed8 && i >= 0 && i < ( int )_ct.size() )
        _ct[i] = c;
}

void QImage::setColorTable( const std::vector<QRgb> & t )
{
    if ( _fmt == Format_Indexed8 )
        _ct = t;
}

const uint8_t * QImage::constBits() const { return isRgb() ? ( const uint8_t * )_argb.data() : nullptr; }
uint8_t * QImage::bits() { return isRgb() ? ( uint8_t * )_argb.data() : nullptr; }
int QImage::bytesPerLine() const { return _w * 4; }

QImage QImage::copy( int x, int y, int w, int h ) const
{
    QImage out;
    if ( x < 0 ) { w += x; x = 0; }
    if ( y < 0 ) { h += y; y = 0; }
    if ( w <= 0 || h <= 0 || x >= _w || y >= _h )
        return out;
    if ( x + w > _w ) w = _w - x;
    if ( y + h > _h ) h = _h - y;
    w = std::max( 0, w );
    h = std::max( 0, h );
    out = QImage( w, h, _fmt );
    for ( int yy = 0; yy < h; ++yy ) {
        if ( isRgb() )
            memcpy( out._argb.data() + ( size_t )yy * w, _argb.data() + ( size_t )( y + yy ) * _w + x, sizeof( uint32_t ) * w );
        else if ( _fmt == Format_Indexed8 )
            memcpy( out._idx.data() + ( size_t )yy * w, _idx.data() + ( size_t )( y + yy ) * _w + x, w );
    }
    if ( _fmt == Format_Indexed8 )
        out._ct = _ct;
    return out;
}

QImage QImage::mirror( bool h, bool v ) const
{
    if ( isNull() )
        return *this;
    QImage out( _w, _h, _fmt );
    for ( int yy = 0; yy < _h; ++yy ) {
        const int srcY = v ? _h - 1 - yy : yy;
        for ( int xx = 0; xx < _w; ++xx ) {
            const int srcX = h ? _w - 1 - xx : xx;
            if ( isRgb() )
                out._argb[( size_t )yy * _w + xx] = _argb[( size_t )srcY * _w + srcX];
            else if ( _fmt == Format_Indexed8 )
                out._idx[( size_t )yy * _w + xx] = _idx[( size_t )srcY * _w + srcX];
        }
    }
    if ( _fmt == Format_Indexed8 )
        out._ct = _ct;
    return out;
}

QImage QImage::scaled( int w, int h, Qt::AspectRatioMode aspect, Qt::TransformationMode /*mode*/ ) const
{
    if ( isNull() || w <= 0 || h <= 0 )
        return *this;
    int dw = w;
    int dh = h;
    if ( aspect == Qt::KeepAspectRatio || aspect == Qt::KeepAspectRatioByExpanding ) {
        const double srcA = ( double )_w / _h;
        const double winA = ( double )w / h;
        if ( aspect == Qt::KeepAspectRatio ) {
            if ( winA > srcA ) { dw = ( int )( h * srcA + 0.5 ); dh = h; }
            else { dw = w; dh = ( int )( w / srcA + 0.5 ); }
        }
        else {
            if ( winA > srcA ) { dw = w; dh = ( int )( w / srcA + 0.5 ); }
            else { dw = ( int )( h * srcA + 0.5 ); dh = h; }
        }
    }
    dw = std::max( 1, dw );
    dh = std::max( 1, dh );
    QImage out( dw, dh, _fmt );
    if ( _fmt == Format_Indexed8 )
        out._ct = _ct;
    // Nearest neighbour: pick the source pixel at the centre of each dest cell.
    for ( int yy = 0; yy < dh; ++yy ) {
        const int sy = std::min( _h - 1, ( int )( ( ( double )yy + 0.5 ) * _h / dh ) );
        for ( int xx = 0; xx < dw; ++xx ) {
            const int sx = std::min( _w - 1, ( int )( ( ( double )xx + 0.5 ) * _w / dw ) );
            if ( isRgb() )
                out._argb[( size_t )yy * dw + xx] = _argb[( size_t )sy * _w + sx];
            else if ( _fmt == Format_Indexed8 )
                out._idx[( size_t )yy * dw + xx] = _idx[( size_t )sy * _w + sx];
        }
    }
    return out;
}

std::vector<uint8_t> QImage::toRgba8() const
{
    const size_t n = ( size_t )_w * _h;
    std::vector<uint8_t> rgba( n * 4 );
    for ( size_t i = 0; i < n; ++i ) {
        const uint32_t c = _argb[i];
        rgba[i * 4 + 0] = ( uint8_t )( c >> 16 & 0xFF );
        rgba[i * 4 + 1] = ( uint8_t )( c >> 8 & 0xFF );
        rgba[i * 4 + 2] = ( uint8_t )( c & 0xFF );
        rgba[i * 4 + 3] = ( uint8_t )( c >> 24 & 0xFF );
    }
    return rgba;
}

bool QImage::save( const std::string & fileName, const char * /*format*/ ) const
{
    if ( isNull() )
        return false;
    const std::vector<uint8_t> rgba = toRgba8();
    return writePng( rgba.data(), _w, _h, fileName );
}

// ---- QPainter ---------------------------------------------------------------
bool QPainter::begin( QImage * img )
{
    _img = img;
    _sx = _sy = 1.0;
    _tx = _ty = 0.0;
    _comp = CompositionMode_SourceOver;
    return img != nullptr;
}

void QPainter::blend( int dx, int dy, const QImage & src, int sx, int sy, int sw, int sh, int dw, int dh )
{
    if ( !_img || src.isNull() || dw <= 0 || dh <= 0 )
        return;
    QImage * dst = _img;
    const QImage sub = src.copy( sx, sy, sw, sh );   // honour the source sub-rect
    const QImage s2 = sub.scaled( dw, dh, Qt::IgnoreAspectRatio, Qt::FastTransformation );
    for ( int y = 0; y < dh; ++y ) {
        const int ty = dy + y;
        if ( ty < 0 || ty >= dst->_h )
            continue;
        for ( int x = 0; x < dw; ++x ) {
            if ( !dst->isRgb() )
                return;
            const int tx = dx + x;
            if ( tx < 0 || tx >= dst->_w )
                continue;
            const uint32_t s = s2._argb[( size_t )y * dw + x];
            uint32_t & d = dst->_argb[( size_t )ty * dst->_w + tx];
            d = ( _comp == CompositionMode_Source ) ? s : blendPixel( s, d );
        }
    }
}

void QPainter::drawImage( int x, int y, const QImage & img )
{
    if ( !_img || img.isNull() )
        return;
    const int dx = ( int )std::lround( x * _sx + _tx );
    const int dy = ( int )std::lround( y * _sy + _ty );
    const int dw = std::max( 1, ( int )std::lround( img.width() * _sx ) );
    const int dh = std::max( 1, ( int )std::lround( img.height() * _sy ) );
    blend( dx, dy, img, 0, 0, img.width(), img.height(), dw, dh );
}

void QPainter::drawImage( int x, int y, const QImage & img, int sx, int sy, int sw, int sh )
{
    if ( !_img || img.isNull() || sw <= 0 || sh <= 0 )
        return;
    const int dx = ( int )std::lround( x * _sx + _tx );
    const int dy = ( int )std::lround( y * _sy + _ty );
    const int dw = std::max( 1, ( int )std::lround( sw * _sx ) );
    const int dh = std::max( 1, ( int )std::lround( sh * _sy ) );
    blend( dx, dy, img, sx, sy, sw, sh, dw, dh );
}

void QPainter::drawImage( const QRect & r, const QImage & img )
{
    if ( !_img || img.isNull() )
        return;
    const int dx = ( int )std::lround( r.x() * _sx + _tx );
    const int dy = ( int )std::lround( r.y() * _sy + _ty );
    const int dw = std::max( 1, r.width() );
    const int dh = std::max( 1, r.height() );
    blend( dx, dy, img, 0, 0, img.width(), img.height(), dw, dh );
}

void QPainter::fillRect( const QRect & r, const QColor & c )
{
    if ( !_img )
        return;
    const int dx = ( int )std::lround( r.x() * _sx + _tx );
    const int dy = ( int )std::lround( r.y() * _sy + _ty );
    const int dw = std::max( 1, ( int )std::lround( r.width() * _sx ) );
    const int dh = std::max( 1, ( int )std::lround( r.height() * _sy ) );
    for ( int y = dy; y < dy + dh; ++y )
        for ( int x = dx; x < dx + dw; ++x )
            putPixel( x, y, c.rgba() );
}

void QPainter::drawRect( const QRect & r )
{
    if ( !_img || r.width() <= 0 || r.height() <= 0 )
        return;
    // Fill interior with the brush (if any), under the painter transform.
    if ( _brush.style() != Qt::NoBrush )
        fillRect( r, _brush.color() );
    const int x0 = ( int )std::lround( r.x() * _sx + _tx );
    const int y0 = ( int )std::lround( r.y() * _sy + _ty );
    const int x1 = x0 + std::max( 1, ( int )std::lround( r.width() * _sx ) ); // exclusive edge (Qt)
    const int y1 = y0 + std::max( 1, ( int )std::lround( r.height() * _sy ) );
    const QColor pc = _pen.color();
    for ( int x = x0; x <= x1; ++x ) {
        putPixel( x, y0, pc.rgba() );
        if ( y1 != y0 )
            putPixel( x, y1, pc.rgba() );
    }
    for ( int y = y0; y <= y1; ++y ) {
        if ( y == y0 || y == y1 )
            continue;
        putPixel( x0, y, pc.rgba() );
        if ( x1 != x0 )
            putPixel( x1, y, pc.rgba() );
    }
}

void QPainter::putPixel( int x, int y, uint32_t color )
{
    if ( !_img || !_img->isRgb() || x < 0 || y < 0 || x >= _img->width() || y >= _img->height() )
        return;
    uint32_t & d = _img->_argb[( size_t )y * _img->width() + x];
    d = ( _comp == CompositionMode_Source ) ? color : blendPixel( color, d );
}

void QPainter::drawLine( int x1, int y1, int x2, int y2 )
{
    const uint32_t c = _pen.color().rgba();
    const int dx = std::abs( x2 - x1 );
    const int dy = -std::abs( y2 - y1 );
    const int sx = x1 < x2 ? 1 : -1;
    const int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for ( ;; ) {
        putPixel( x1, y1, c );
        if ( x1 == x2 && y1 == y2 )
            break;
        const int e2 = 2 * err;
        if ( e2 >= dy ) { err += dy; x1 += sx; }
        if ( e2 <= dx ) { err += dx; y1 += sy; }
    }
}

// ---- QJson (minimal parser, used by layout_json) ---------------------------
QJsonValue::QJsonValue( const QJsonArray & a )
    : _t( Array ), _b( false ), _d( 0 ), _arr( a._v ) {}

QJsonValue::QJsonValue( const QJsonObject & o )
    : _t( Object ), _b( false ), _d( 0 ), _obj( o._o ) {}

QJsonArray QJsonValue::toArray() const
{
    QJsonArray a;
    if ( _t == Array )
        a._v = _arr;
    return a;
}

QJsonObject QJsonValue::toObject() const
{
    QJsonObject o;
    if ( _t == Object )
        o._o = _obj;
    return o;
}

void QJsonDocument::skipWs()
{
    while ( _pos < _src->size() ) {
        const char c = ( *_src )[_pos];
        if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' )
            ++_pos;
        else
            break;
    }
}

bool QJsonDocument::parseString( std::string & out )
{
    if ( _pos >= _src->size() || ( *_src )[_pos] != '"' )
        return false;
    ++_pos;
    while ( _pos < _src->size() ) {
        const char c = ( *_src )[_pos++];
        if ( c == '"' )
            return true;
        if ( c == '\\' && _pos < _src->size() ) {
            const char e = ( *_src )[_pos++];
            switch ( e ) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                unsigned int cp = 0;
                for ( int k = 0; k < 4 && _pos < _src->size(); ++k ) {
                    const char h = ( *_src )[_pos++];
                    int d = ( h >= '0' && h <= '9' ) ? h - '0' : ( ( h >= 'a' && h <= 'f' ) ? h - 'a' + 10 : ( h >= 'A' && h <= 'F' ? h - 'A' + 10 : 0 ) );
                    cp = ( cp << 4 ) | ( unsigned int )d;
                }
                // Encode as UTF-8 into the UTF-8 std::string.
                if ( cp < 0x80 ) out += ( char )cp;
                else if ( cp < 0x800 ) { out += ( char )( 0xC0 | ( cp >> 6 ) ); out += ( char )( 0x80 | ( cp & 0x3F ) ); }
                else { out += ( char )( 0xE0 | ( cp >> 12 ) ); out += ( char )( 0x80 | ( ( cp >> 6 ) & 0x3F ) ); out += ( char )( 0x80 | ( cp & 0x3F ) ); }
                break;
            }
            default: out += e; break;
            }
        }
        else
            out += c;
    }
    return false;
}

bool QJsonDocument::parseValue( QJsonValue & out )
{
    skipWs();
    if ( _pos >= _src->size() )
        return false;
    const char c = ( *_src )[_pos];
    if ( c == '{' )
        return parseObject( out );
    if ( c == '[' )
        return parseArray( out );
    if ( c == '"' ) {
        std::string s;
        if ( !parseString( s ) )
            return false;
        out = QJsonValue( QString::fromUtf8( s ) );
        return true;
    }
    if ( c == 't' || c == 'f' ) {
        out = QJsonValue( ( _src->substr( _pos, 4 ) == "true" ) );
        _pos += 4;
        return true;
    }
    if ( c == 'n' ) {
        out = QJsonValue();
        _pos += 4;
        return true;
    }
    // number
    size_t start = _pos;
    while ( _pos < _src->size() && ( ( *_src )[_pos] == '-' || ( *_src )[_pos] == '+' || ( *_src )[_pos] == '.' || ( ( *_src )[_pos] >= '0' && ( *_src )[_pos] <= '9' ) || ( *_src )[_pos] == 'e' || ( *_src )[_pos] == 'E' ) )
        ++_pos;
    if ( _pos == start )
        return false;
    const double d = std::atof( _src->substr( start, _pos - start ).c_str() );
    out = QJsonValue( ( int )d ); // numeric layout values are integers
    return true;
}

bool QJsonDocument::parseObject( QJsonValue & out )
{
    ++_pos; // '{'
    QJsonObject obj;
    skipWs();
    if ( _pos < _src->size() && ( *_src )[_pos] == '}' ) {
        ++_pos;
        out = QJsonValue( obj );
        return true;
    }
    while ( _pos < _src->size() ) {
        skipWs();
        if ( _pos >= _src->size() || ( *_src )[_pos] != '"' )
            return false;
        std::string key;
        if ( !parseString( key ) )
            return false;
        skipWs();
        if ( _pos >= _src->size() || ( *_src )[_pos] != ':' )
            return false;
        ++_pos; // ':'
        QJsonValue val;
        if ( !parseValue( val ) )
            return false;
        obj._o[key] = val;
        skipWs();
        if ( _pos >= _src->size() )
            return false;
        if ( ( *_src )[_pos] == ',' ) {
            ++_pos;
            continue;
        }
        if ( ( *_src )[_pos] == '}' ) {
            ++_pos;
            out = QJsonValue( obj );
            return true;
        }
        return false;
    }
    return false;
}

bool QJsonDocument::parseArray( QJsonValue & out )
{
    ++_pos; // '['
    QJsonArray arr;
    skipWs();
    if ( _pos < _src->size() && ( *_src )[_pos] == ']' ) {
        ++_pos;
        out = QJsonValue( arr );
        return true;
    }
    while ( _pos < _src->size() ) {
        QJsonValue val;
        if ( !parseValue( val ) )
            return false;
        arr._v.push_back( val );
        skipWs();
        if ( _pos >= _src->size() )
            return false;
        if ( ( *_src )[_pos] == ',' ) {
            ++_pos;
            continue;
        }
        if ( ( *_src )[_pos] == ']' ) {
            ++_pos;
            out = QJsonValue( arr );
            return true;
        }
        return false;
    }
    return false;
}

bool QJsonDocument::_parse( const std::string & s, QJsonParseError * )
{
    _src = &s;
    _pos = 0;
    QJsonValue v;
    if ( !parseValue( v ) )
        return false;
    _v = v;
    return true;
}

// ---- QJson serialization ----------------------------------------------------
namespace {
void jsonEscape( const std::string & s, std::string & out )
{
    out += '"';
    for ( char c : s ) {
        switch ( c ) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if ( ( unsigned char )c < 0x20 ) {
                char b[8];
                std::snprintf( b, sizeof b, "\\u%04x", ( unsigned char )c );
                out += b;
            }
            else
                out += c;
        }
    }
    out += '"';
}

void jsonValueStr( const QJsonValue & v, bool indent, int depth, std::string & out )
{
    const std::string nl = indent ? "\n" : "";
    const std::string pad = indent ? std::string( depth * 2, ' ' ) : "";
    const std::string pad2 = indent ? std::string( depth * 2 + 2, ' ' ) : "";
    switch ( v.type() ) {
    case QJsonValue::Null: out += "null"; break;
    case QJsonValue::Bool: out += v.toBool() ? "true" : "false"; break;
    case QJsonValue::Double: {
        char b[32];
        std::snprintf( b, sizeof b, "%.0f", v.toDouble() );
        out += b;
        break;
    }
    case QJsonValue::String: jsonEscape( v.toString().toStdString(), out ); break;
    case QJsonValue::Array: {
        out += "[";
        const QJsonArray a = v.toArray();
        for ( int i = 0; i < a.size(); ++i ) {
            if ( indent ) out += nl + pad2;
            else if ( i ) out += ",";
            jsonValueStr( a.at( i ), indent, depth + 1, out );
            if ( !indent && i + 1 < a.size() ) out += ",";
        }
        if ( indent ) out += nl + pad;
        out += "]";
        break;
    }
    case QJsonValue::Object: {
        out += "{";
        const QJsonObject o = v.toObject();
        bool first = true;
        for ( const auto & kv : o._o ) {
            if ( indent ) out += ( first ? nl + pad2 : ",\n" + pad2 );
            else if ( !first ) out += ",";
            jsonEscape( kv.first, out );
            out += ":";
            if ( indent ) out += " ";
            jsonValueStr( kv.second, indent, depth + 1, out );
            first = false;
        }
        if ( indent ) out += nl + pad;
        out += "}";
        break;
    }
    }
}
} // namespace

QByteArray QJsonDocument::toJson( JsonFormat fmt ) const
{
    std::string s;
    jsonValueStr( _v, fmt == Indented, 0, s );
    return QByteArray( s );
}

std::vector<uint8_t> QImage::toPng() const
{
    if ( isNull() )
        return {};
    if ( _fmt == Format_Indexed8 )
        return encodeIndexedPng( _idx, _ct, _w, _h );
    const std::vector<uint8_t> rgba = toRgba8();
    return encodePng( rgba.data(), _w, _h );
}
#include "rastercompat.hpp"
void QPainter::save() { _stack.push_back( Saved( _sx, _sy, _tx, _ty, _comp, _pen, _brush ) ); }
void QPainter::restore() { if ( !_stack.empty() ) { Saved s = _stack.back(); _stack.pop_back(); _sx = s.sx; _sy = s.sy; _tx = s.tx; _ty = s.ty; _comp = s.comp; _pen = s.pen; _brush = s.brush; } }
