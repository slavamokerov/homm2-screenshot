// A minimal, software "Qt-compat" layer used ONLY by the headless WASM build of
// homm2-screenshot. It implements the exact subset of QtGui/QtCore that the
// shared render code (fh2resources: aggicn/assets/gamefont/textutil and
// fh2poster) uses, so those sources can be compiled verbatim under Emscripten
// without pulling in a Qt for WebAssembly install. The native CLI keeps real Qt.
//
// Semantics (ARGB32 words, SourceOver blending, nearest-neighbour scaling,
// painter scale() transform) are matched to Qt 6.x so the WASM output is
// pixel-identical to the desktop CLI; a parity test (tools/parity_*) checks this.
//
// Files named like Qt headers (<QImage> etc.) in this folder are forwarders into
// this single header. Non-trivial methods live in rastercompat.cpp.

#pragma once

// Numeric literal -> QString (runtime, wasm-friendly replacement of Qt's compile-time macro).
#ifndef QStringLiteralS
#define QStringLiteral(s) ::QString::fromUtf8(s)
#define QStringLiteralS
#endif

#include "QtGlobalCompat"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ---- Qt namespace enums -----------------------------------------------------
namespace Qt {

enum GlobalColor {
    transparent = 0,
    black,
    white,
    red,
    darkRed,
    green,
    darkGreen,
    blue,
    darkBlue,
    cyan,
    darkCyan,
    magenta,
    darkMagenta,
    yellow,
    darkYellow,
    gray,
    darkGray,
    lightGray,
    dimGray,
};

enum Orientation { Horizontal = 1, Vertical = 2 };

enum AspectRatioMode { IgnoreAspectRatio = 0, KeepAspectRatio = 1, KeepAspectRatioByExpanding = 2 };

enum TransformationMode { FastTransformation = 0, SmoothTransformation = 1 };

enum AlignmentFlag {
    AlignLeft = 0x1,
    AlignRight = 0x2,
    AlignHCenter = 0x4,
    AlignJustify = 0x8,
    AlignTop = 0x20,
    AlignBottom = 0x40,
    AlignVCenter = 0x80,
    AlignCenter = AlignHCenter | AlignVCenter,
};

enum PenStyle { NoPen = 0, SolidLine = 1, DashLine = 2 };
enum BrushStyle { NoBrush = 0, SolidPattern = 1 };

} // namespace Qt

// ---- QChar ------------------------------------------------------------------
class QChar
{
public:
    QChar() : _u( 0 ) {}
    QChar( unsigned int code ) : _u( static_cast<unsigned short>( code ) ) {}
    QChar( int code ) : _u( static_cast<unsigned short>( ( unsigned int )code ) ) {}
    explicit QChar( char c ) : _u( static_cast<unsigned short>( ( unsigned char )c ) ) {}

    unsigned short unicode() const { return _u; }
    QChar toLower() const { return QChar( _u >= 0x41 && _u <= 0x5A ? _u + 0x20 : _u ); }
    QChar toUpper() const { return QChar( _u >= 0x61 && _u <= 0x7A ? _u - 0x20 : _u ); }
    bool isSpace() const { return _u == ' ' || ( _u >= 0x09 && _u <= 0x0D ); }
    bool isNull() const { return _u == 0; }
    int toDigit( bool * ok = nullptr ) const
    {
        if ( ok )
            *ok = ( _u >= '0' && _u <= '9' );
        return ( _u >= '0' && _u <= '9' ) ? ( int )( _u - '0' ) : -1;
    }
    unsigned short toLatin1() const { return _u <= 0xFF ? _u : 0; }
    int digitValue() const { return ( _u >= '0' && _u <= '9' ) ? ( int )( _u - '0' ) : -1; }

    static QChar fromLatin1( unsigned char c ) { return QChar( c ); }
    static QChar toLatin1( unsigned short u ) { return QChar( u <= 0xFF ? u : 0 ); }

    bool operator==( const QChar & o ) const { return _u == o._u; }
    bool operator!=( const QChar & o ) const { return _u != o._u; }
    bool operator<( const QChar & o ) const { return _u < o._u; }

    unsigned short _u;
};

class QLatin1Char
{
public:
    explicit QLatin1Char( char c ) : _c( ( unsigned char )c ) {}
    QChar toQChar() const { return QChar( _c ); }
    operator QChar() const { return QChar( _c ); }
    unsigned char _c;
};

// ---- QColor -----------------------------------------------------------------
class QColor
{
public:
    QColor() : _rgba( 0 ) {}
    QColor( QRgb a ) : _rgba( a ) {}
    QColor( int r, int g, int b, int a = 255 ) : _rgba( qRgba( r, g, b, a ) ) {}
    QColor( Qt::GlobalColor c ) : _rgba( fromGlobalColor( c ) ) {}

    static QColor fromRgba( QRgb a ) { return QColor( a ); }
    static QColor fromRgb( QRgb a ) { return QColor( ( a & 0x00FFFFFFu ) | 0xFF000000u ); }
    static QColor fromRgb( int r, int g, int b ) { return QColor( r, g, b ); }

    QRgb rgba() const { return _rgba; }
    QRgb rgb() const { return ( _rgba == 0x00000000u ) ? 0u : ( _rgba & 0x00FFFFFFu ); }
    int red() const { return qRed( _rgba ); }
    int green() const { return qGreen( _rgba ); }
    int blue() const { return qBlue( _rgba ); }
    int alpha() const { return qAlpha( _rgba ); }
    bool isOpaque() const { return qAlpha( _rgba ) == 255; }
    bool isValid() const { return true; }

    void setAlpha( int a ) { _rgba = ( _rgba & 0x00FFFFFFu ) | ( ( QRgb )a << 24 ); }

    bool operator==( const QColor & o ) const { return _rgba == o._rgba; }
    bool operator!=( const QColor & o ) const { return _rgba != o._rgba; }

    QRgb _rgba;

private:
    static QRgb fromGlobalColor( Qt::GlobalColor c )
    {
        switch ( c ) {
        case Qt::transparent: return 0x00000000u;
        case Qt::black: return 0xFF000000u;
        case Qt::white: return 0xFFFFFFFFu;
        case Qt::red: return 0xFFFF0000u;
        case Qt::darkRed: return 0xFF800000u;
        case Qt::green: return 0xFF00FF00u;
        case Qt::darkGreen: return 0xFF008000u;
        case Qt::blue: return 0xFF0000FFu;
        case Qt::darkBlue: return 0xFF000080u;
        case Qt::cyan: return 0xFF00FFFFu;
        case Qt::darkCyan: return 0xFF008080u;
        case Qt::magenta: return 0xFFFF00FFu;
        case Qt::darkMagenta: return 0xFF800080u;
        case Qt::yellow: return 0xFFFFFF00u;
        case Qt::darkYellow: return 0xFF808000u;
        case Qt::gray: return 0xFFA0A0A4u;
        case Qt::darkGray: return 0xFF808080u;
        case Qt::lightGray: return 0xFFC0C0C0u;
        case Qt::dimGray: return 0xFF696969u;
        default: return 0xFF000000u;
        }
    }
};

// ---- QPoint / QRect / QPen / QBrush ---------------------------------------
class QPoint
{
public:
    QPoint() : _x( 0 ), _y( 0 ) {}
    QPoint( int x, int y ) : _x( x ), _y( y ) {}
    int x() const { return _x; }
    int y() const { return _y; }
    void setX( int x ) { _x = x; }
    void setY( int y ) { _y = y; }
    QPoint operator+( const QPoint & o ) const { return QPoint( _x + o._x, _y + o._y ); }
    QPoint operator-( const QPoint & o ) const { return QPoint( _x - o._x, _y - o._y ); }
    bool operator==( const QPoint & o ) const { return _x == o._x && _y == o._y; }

    int _x, _y;
};

class QSize
{
public:
    QSize() : _w( 0 ), _h( 0 ) {}
    QSize( int w, int h ) : _w( w ), _h( h ) {}
    int width() const { return _w; }
    int height() const { return _h; }

    int _w, _h;
};

class QRect
{
public:
    QRect() : _x( 0 ), _y( 0 ), _w( 0 ), _h( 0 ) {}
    QRect( int x, int y, int w, int h ) : _x( x ), _y( y ), _w( w ), _h( h ) {}
    QRect( const QPoint & tl, const QSize & s ) : _x( tl.x() ), _y( tl.y() ), _w( s.width() ), _h( s.height() ) {}

    int x() const { return _x; }
    int y() const { return _y; }
    int width() const { return _w; }
    int height() const { return _h; }
    int right() const { return _x + _w - 1; }
    int bottom() const { return _y + _h - 1; }
    QSize size() const { return QSize( _w, _h ); }
    bool isEmpty() const { return _w <= 0 || _h <= 0; }
    bool isNull() const { return _w == 0 && _h == 0; }
    bool contains( const QPoint & p ) const
    {
        return p.x() >= _x && p.x() < _x + _w && p.y() >= _y && p.y() < _y + _h;
    }

    QPoint topLeft() const { return QPoint( _x, _y ); }
    QPoint bottomRight() const { return QPoint( right(), bottom() ); }
    QSize sizeQ() const { return QSize( _w, _h ); }

    QRect translated( int dx, int dy ) const { return QRect( _x + dx, _y + dy, _w, _h ); }
    QRect translated( const QPoint & p ) const { return QRect( _x + p.x(), _y + p.y(), _w, _h ); }
    QRect adjusted( int dx1, int dy1, int dx2, int dy2 ) const
    {
        return QRect( _x + dx1, _y + dy1, _w + dx2 - dx1, _h + dy2 - dy1 );
    }

    bool intersects( const QRect & o ) const
    {
        return _x < o._x + o._w && o._x < _x + _w && _y < o._y + o._h && o._y < _y + _h;
    }

    bool operator==( const QRect & o ) const
    {
        return _x == o._x && _y == o._y && _w == o._w && _h == o._h;
    }

    int _x, _y, _w, _h;
};

class QPen
{
public:
    QPen() : _color( Qt::black ), _width( 0 ), _solid( true ) {}
    QPen( const QColor & c, int w = 0, Qt::PenStyle style = Qt::SolidLine )
        : _color( c ), _width( w ), _solid( style != Qt::NoPen ) {}
    const QColor & color() const { return _color; }
    int width() const { return _width; }
    bool isSolid() const { return _solid; }

    QColor _color;
    int _width;
    bool _solid;
};

class QBrush
{
public:
    QBrush() : _color( Qt::black ), _style( Qt::NoBrush ) {}
    QBrush( const QColor & c ) : _color( c ), _style( Qt::SolidPattern ) {}
    QBrush( Qt::BrushStyle s ) : _color( Qt::black ), _style( s ) {}
    const QColor & color() const { return _color; }
    Qt::BrushStyle style() const { return _style; }

    QColor _color;
    Qt::BrushStyle _style;
};

// ---- QImage -----------------------------------------------------------------
class QImage
{
public:
    enum Format {
        Format_Invalid = 0,
        Format_Mono,
        Format_Indexed8,
        Format_ARGB32,
        Format_RGB32,
        Format_RGBA8888,
        Format_Grayscale8,
    };

    QImage() : _w( 0 ), _h( 0 ), _fmt( Format_Invalid ) {}
    QImage( int w, int h, Format fmt );
    QImage( const QSize & s, Format fmt ) : QImage( s.width(), s.height(), fmt ) {}
    QImage( const QImage & o ) = default;
    QImage & operator=( const QImage & o ) = default;

    bool isNull() const { return _fmt == Format_Invalid || _w <= 0 || _h <= 0; }
    int width() const { return _w; }
    int height() const { return _h; }
    QSize size() const { return QSize( _w, _h ); }
    Format format() const { return _fmt; }
    void fill( uint32_t val ) { fillValue( val ); }
    void fill( const QColor & c ) { fillValue( c.rgba() ); }
    void fill( Qt::GlobalColor c ) { fillValue( QColor( c ).rgba() ); }

    void setPixel( int x, int y, uint32_t v );
    void setPixelColor( int x, int y, const QColor & c );
    QColor pixelColor( int x, int y ) const;
    uint32_t pixel( int x, int y ) const;
    int pixelIndex( int x, int y ) const;
    void setColor( int i, QRgb c );   // Indexed8 palette
    void setColorTable( const std::vector<QRgb> & t );
    // Access the raw RGBA8 scanlines (Format_RGBA8888) for PNG encoding.
    const uint8_t * constBits() const;
    uint8_t * bits();
    int bytesPerLine() const;

    QImage copy( int x, int y, int w, int h ) const;
    QImage copy( const QRect & r ) const { return copy( r.x(), r.y(), r.width(), r.height() ); }
    QImage mirror( bool h, bool v ) const;
    QImage mirrored( bool h, bool v ) const { return mirror( h, v ); }
    QImage flipped( Qt::Orientation o ) const { return mirror( o == Qt::Horizontal, o == Qt::Vertical ); }
    const uint8_t * scanLine( int y ) const { return isRgb() ? ( const uint8_t * )_argb.data() + ( size_t )y * _w * 4 : nullptr; }
    uint8_t * scanLine( int y ) { return isRgb() ? ( uint8_t * )_argb.data() + ( size_t )y * _w * 4 : nullptr; }
    QImage scaled( int w, int h, Qt::AspectRatioMode aspect = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation ) const;
    bool save( const std::string & fileName, const char * format = "PNG" ) const;

    // RGBA8 bytes (w*h*4), ARGB32 -> RGBA. Used by the WASM PNG envelope.
    std::vector<uint8_t> toRgba8() const;

    std::vector<uint8_t> toPng() const;

    // Used by assets (QPixmap alias).
    static QImage fromImage( const QImage & o ) { return o; }

    int _w, _h;
    Format _fmt;
    std::vector<uint32_t> _argb; // ARGB32 words (RGBA formats)
    std::vector<uint8_t> _idx;   // palette index bytes (Format_Indexed8)
    std::vector<QRgb> _ct;       // Indexed8 color table

    bool isRgb() const { return _fmt == Format_ARGB32 || _fmt == Format_RGB32 || _fmt == Format_RGBA8888; }

private:
    void fillValue( uint32_t v );
};

// ---- QPainter ---------------------------------------------------------------
class QPainter
{
public:
    enum CompositionMode {
        CompositionMode_SourceOver = 0,
        CompositionMode_Source = 1,
        CompositionMode_DestinationOver = 2,
        CompositionMode_Clear = 3,
    };
    enum RenderHint { Antialiasing = 0x01, SmoothPixmapTransform = 0x02, TextAntialiasing = 0x04 };

    QPainter() : _img( nullptr ), _sx( 1 ), _sy( 1 ), _comp( CompositionMode_SourceOver ) {}
    explicit QPainter( QImage * img ) { begin( img ); }
    ~QPainter() {}

    bool begin( QImage * img );
    bool end() { _img = nullptr; return true; }
    bool isActive() const { return _img != nullptr; }
    bool isNull() const { return _img == nullptr; }

    void setRenderHint( RenderHint, bool = true ) {}
    void setCompositionMode( CompositionMode m ) { _comp = m; }
    void save();
    void restore();
    void setPen( const QPen & p ) { _pen = p; }
    void setPen( const QColor & c ) { _pen = QPen( c ); }
    void setBrush( const QBrush & b ) { _brush = b; }
    void setBrush( const QColor & c ) { _brush = QBrush( c ); }
    void setBrush( Qt::BrushStyle s ) { _brush = QBrush( s ); }
    void scale( qreal fx, qreal fy ) { _sx *= ( double )fx; _sy *= ( double )fy; }
    void translate( qreal dx, qreal dy ) { _tx += dx * _sx; _ty += dy * _sy; }

    void drawImage( int x, int y, const QImage & img );
    void drawImage( int x, int y, const QImage & img, int sx, int sy, int sw, int sh );
    void drawImage( const QRect & r, const QImage & img );
    void drawImage( const QPoint & p, const QImage & img ) { drawImage( p.x(), p.y(), img ); }
    void drawRect( const QRect & r );
    void drawRect( int x, int y, int w, int h ) { drawRect( QRect( x, y, w, h ) ); }
    void drawLine( int x1, int y1, int x2, int y2 );
    void fillRect( const QRect & r, const QColor & c );
    void fillRect( int x, int y, int w, int h, const QColor & c ) { fillRect( QRect( x, y, w, h ), c ); }
    void fillRect( int x, int y, int w, int h, Qt::GlobalColor c ) { fillRect( QRect( x, y, w, h ), QColor( c ) ); }
    void fillRect( const QRect & r, Qt::GlobalColor c ) { fillRect( r, QColor( c ) ); }

    private:
    struct Saved {
        double sx, sy, tx, ty;
        CompositionMode comp;
        QPen pen;
        QBrush brush;
        Saved( double a, double b, double c, double d, CompositionMode e, QPen f, QBrush g )
            : sx( a ), sy( b ), tx( c ), ty( d ), comp( e ), pen( f ), brush( g ) {}
    };
    void blend( int dx, int dy, const QImage & src, int sx, int sy, int sw, int sh, int dw, int dh );
    void putPixel( int x, int y, uint32_t color );
    QImage * _img;
    QPen _pen;
    QBrush _brush;
    double _sx, _sy;
    double _tx, _ty;
    CompositionMode _comp;
    std::vector<Saved> _stack;
};

// ---- string helpers (UTF-8 in/out, matching Qt fromStdString/fromUtf8) ------
class QString; class QStringList;
inline unsigned short utf8Next( const std::string & s, size_t & i )
{
    const unsigned char c = ( unsigned char )s[i];
    size_t extra = 0;
    if ( ( c & 0x80 ) == 0 )
        extra = 0;
    else if ( ( c & 0xE0 ) == 0xC0 )
        extra = 1;
    else if ( ( c & 0xF0 ) == 0xE0 )
        extra = 2;
    else if ( ( c & 0xF8 ) == 0xF0 )
        extra = 3;
    unsigned int cp = c & ( 0x7F >> extra );
    for ( size_t k = 1; k <= extra && i + k < s.size(); ++k )
        cp = ( cp << 6 ) | ( ( unsigned char )s[i + k] & 0x3F );
    i += extra + 1;
    return cp <= 0xFFFF ? ( unsigned short )cp : 0xFFFD;
}

inline std::string utf8Encode( unsigned short u )
{
    std::string out;
    unsigned int cp = u;
    if ( cp < 0x80 )
        out += ( char )cp;
    else if ( cp < 0x800 ) {
        out += ( char )( 0xC0 | ( cp >> 6 ) );
        out += ( char )( 0x80 | ( cp & 0x3F ) );
    }
    else {
        out += ( char )( 0xE0 | ( cp >> 12 ) );
        out += ( char )( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
        out += ( char )( 0x80 | ( cp & 0x3F ) );
    }
    return out;
}

class QLatin1String
{
public:
    QLatin1String() : _s( nullptr ) {}
    explicit QLatin1String( const char * s ) : _s( s ) {}
    const char * data() const { return _s; }
    const char * _s;
};

class QString
{
public:
    QString() = default;
    QString( const QString & ) = default;
    QString & operator=( const QString & ) = default;
    QString( const char * ascii ) { appendAscii( ascii ); }
    QString( const QLatin1String & s ) { appendAscii( s.data() ); }
    QString( const QChar & c ) { _s.push_back( c.unicode() ); }
    QString( const QLatin1Char & c ) { _s.push_back( c._c ); }

    // Range-for support.
    struct const_iterator {
        std::u16string::const_iterator it;
        const_iterator( std::u16string::const_iterator i ) : it( i ) {}
        QChar operator*() const { return QChar( *it ); }
        const_iterator & operator++() { ++it; return *this; }
        bool operator!=( const const_iterator & o ) const { return it != o.it; }
    };
    const_iterator begin() const { return const_iterator( _s.begin() ); }
    const_iterator end() const { return const_iterator( _s.end() ); }

    static QString fromAscii( const char * s )
    {
        QString q;
        q.appendAscii( s );
        return q;
    }
    static QString fromLatin1( const std::string & s )
    {
        QString q;
        q._s.resize( s.size() );
        for ( size_t i = 0; i < s.size(); ++i )
            q._s[i] = ( unsigned char )s[i];
        return q;
    }
    static QString fromUtf8( const std::string & s )
    {
        QString q;
        for ( size_t i = 0; i < s.size(); ) {
            if ( ( unsigned char )s[i] < 0x80 ) {
                q._s.push_back( ( unsigned char )s[i] );
                ++i;
            }
            else
                q._s.push_back( utf8Next( s, i ) );
        }
        return q;
    }
    static QString fromStdString( const std::string & s ) { return fromUtf8( s ); }

    int size() const { return ( int )_s.size(); }
    int length() const { return ( int )_s.size(); }
    bool isEmpty() const { return _s.empty(); }
    void reserve( int n ) { _s.reserve( ( size_t )n ); }

    QChar at( int i ) const { return QChar( _s[( size_t )i] ); }
    QChar operator[]( int i ) const { return QChar( _s[( size_t )i] ); }
    QChar front() const { return QChar( _s.front() ); }
    QChar back() const { return QChar( _s.back() ); }

    QString & append( const QChar & c )
    {
        _s.push_back( c.unicode() );
        return *this;
    }
    QString & append( const QString & s )
    {
        _s += s._s;
        return *this;
    }
    QString & append( const char * s )
    {
        appendAscii( s );
        return *this;
    }
    QString & operator+=( const QString & s )
    {
        _s += s._s;
        return *this;
    }
    QString & operator+=( const QChar & c )
    {
        _s.push_back( c.unicode() );
        return *this;
    }
    QString & operator+=( const char * s )
    {
        appendAscii( s );
        return *this;
    }
    QString operator+( const QString & s ) const
    {
        QString r = *this;
        r._s += s._s;
        return r;
    }
    QString operator+( const char * s ) const
    {
        QString r = *this;
        r.append( s );
        return r;
    }

    QString left( int n ) const
    {
        QString r;
        r._s = n >= 0 ? _s.substr( 0, ( size_t )n ) : _s;
        return r;
    }
    QString mid( int pos, int n = -1 ) const
    {
        QString r;
        r._s = _s.substr( ( size_t )pos, ( size_t )n );
        return r;
    }
    QString right( int n ) const
    {
        QString r;
        r._s = n >= 0 ? _s.substr( _s.size() - ( size_t )n ) : _s;
        return r;
    }
    QStringList split( QChar sep ) const;   // defined after QStringList
    int indexOf( const QChar & c ) const
    {
        for ( size_t i = 0; i < _s.size(); ++i )
            if ( _s[i] == c.unicode() )
                return ( int )i;
        return -1;
    }
    int indexOf( const QString & s ) const
    {
        const size_t p = _s.find( s._s );
        return p == std::u16string::npos ? -1 : ( int )p;
    }
    bool contains( const QChar & c ) const { return indexOf( c ) >= 0; }
    bool contains( const QString & s ) const { return indexOf( s ) >= 0; }
    bool contains( const char * s ) const { return contains( QString( s ) ); }
    int count( const QChar & c ) const
    {
        int n = 0;
        for ( size_t i = 0; i < _s.size(); ++i )
            if ( _s[i] == c.unicode() )
                ++n;
        return n;
    }
    int compare( const QString & s ) const { return _s.compare( s._s ); }
    int compare( const char * s ) const { return _s.compare( QString( s )._s ); }

    void replace( const QChar & from, const QChar & to )
    {
        for ( size_t i = 0; i < _s.size(); ++i )
            if ( _s[i] == from.unicode() )
                _s[i] = to.unicode();
    }
    void replace( const QString & from, const QString & to )
    {
        size_t p = 0;
        while ( ( p = _s.find( from._s, p ) ) != std::u16string::npos ) {
            _s.replace( p, from._s.size(), to._s );
            p += to._s.size();
        }
    }

    std::string toStdString() const { return toUtf8(); }
    std::string toUtf8() const
    {
        std::string out;
        for ( size_t i = 0; i < _s.size(); ++i )
            out += utf8Encode( _s[i] );
        return out;
    }
    static QString number( int n ) { return fromUtf8( std::to_string( n ) ); }
    static QString number( unsigned n ) { return fromUtf8( std::to_string( n ) ); }
    static QString number( long n ) { return fromUtf8( std::to_string( n ) ); }
    static QString number( unsigned long n ) { return fromUtf8( std::to_string( n ) ); }
    static QString number( long long n ) { return fromUtf8( std::to_string( n ) ); }
    static QString number( unsigned long long n ) { return fromUtf8( std::to_string( n ) ); }
    static QString number( double n ) { return fromUtf8( std::to_string( n ) ); }

    QString arg( const QString & a, const QString & b ) const
    {
        QString r = arg( a );
        std::u16string s2 = { ( unsigned short )'%', ( unsigned short )'2' };
        size_t pos = r._s.find( s2 );
        if ( pos != std::u16string::npos )
            r._s.replace( pos, 2, b._s );
        return r;
    }

    std::string toLatin1() const
    {
        std::string out;
        for ( size_t i = 0; i < _s.size(); ++i )
            out += ( char )( _s[i] & 0xFF );
        return out;
    }

    // arg(): replace the lowest-numbered %N placeholder (Qt semantics).
    QString arg( int v ) const { return replaceArgNumber( QString::fromUtf8( std::to_string( v ) )._s ); }
    QString arg( unsigned v ) const { return replaceArgNumber( QString::fromUtf8( std::to_string( v ) )._s ); }
    QString arg( long v ) const { return replaceArgNumber( QString::fromUtf8( std::to_string( v ) )._s ); }
    QString arg( unsigned long v ) const { return replaceArgNumber( QString::fromUtf8( std::to_string( v ) )._s ); }
    QString arg( long long v ) const { return replaceArgNumber( QString::fromUtf8( std::to_string( v ) )._s ); }
    QString arg( unsigned long long v ) const { return replaceArgNumber( QString::fromUtf8( std::to_string( v ) )._s ); }
    QString arg( double v ) const
    {
        std::string s = std::to_string( v );
        while ( !s.empty() && s.back() == '0' ) s.pop_back();
        if ( !s.empty() && s.back() == '.' ) s.pop_back();
        return replaceArgNumber( QString::fromUtf8( s )._s );
    }
    QString arg( const char * v ) const { return replaceArgNumber( QString( v )._s ); }
    QString arg( const std::string & v ) const { return replaceArgNumber( QString::fromUtf8( v )._s ); }
    QString arg( const QString & v ) const { return replaceArgNumber( v._s ); }
    QString arg( const QChar & v ) const
    {
        QString s;
        s.append( v );
        return replaceArgNumber( s._s );
    }

    bool operator==( const QString & s ) const { return _s == s._s; }
    bool operator==( const char * s ) const { return _s == QString( s )._s; }
    bool operator!=( const QString & s ) const { return _s != s._s; }

    std::u16string _s;

private:
    void appendAscii( const char * s )
    {
        if ( !s )
            return;
        for ( const char * p = s; *p; ++p )
            _s.push_back( ( unsigned char )*p );
    }
    QString replaceArgNumber( const std::u16string & v ) const
    {
        QString r = *this;
        for ( unsigned short d = 1; d <= 9; ++d ) {
            const std::u16string marker = { ( unsigned short )'%', ( unsigned short )( '0' + d ) };
            const size_t p = r._s.find( marker );
            if ( p == std::u16string::npos )
                continue;
            r._s.replace( p, 2, v );
            break;
        }
        return r;
    }
};

class QStringList
{
public:
    QStringList() = default;

    int size() const { return ( int )_list.size(); }
    bool isEmpty() const { return _list.empty(); }
    QString & operator[]( int i ) { return _list[( size_t )i]; }
    const QString & at( int i ) const { return _list[( size_t )i]; }
    void append( const QString & s ) { _list.push_back( s ); }
    QString join( const QString & sep ) const
    {
        QString out;
        for ( int i = 0; i < size(); ++i ) {
            if ( i )
                out.append( sep );
            out.append( _list[( size_t )i] );
        }
        return out;
    }
    void removeFirst() { if ( !_list.empty() ) _list.erase( _list.begin() ); }
    const QString & first() const { return _list.front(); }
    void push_back( const QString & s ) { _list.push_back( s ); }
    QStringList & operator<<( const QString & s ) { _list.push_back( s ); return *this; }
    auto begin() { return _list.begin(); }
    auto end() { return _list.end(); }
    auto begin() const { return _list.cbegin(); }
    auto end() const { return _list.cend(); }

    std::vector<QString> _list;
};

inline QStringList QString::split( QChar sep ) const
{
    QStringList out;
    std::u16string cur;
    for ( size_t i = 0; i < _s.size(); ++i ) {
        if ( _s[i] == sep.unicode() ) {
            QString q;
            q._s = cur;
            out.append( q );
            cur.clear();
        }
        else
            cur.push_back( _s[i] );
    }
    QString q;
    q._s = cur;
    out.append( q );
    return out;
}

class QByteArray
{
public:
    QByteArray() = default;
    QByteArray( const char * s ) : _b( s ? s : "" ) {}
    QByteArray( const char * s, int n ) : _b( s, ( size_t )n ) {}
    QByteArray( const std::string & s ) : _b( s ) {}
    const char * data() const { return _b.data(); }
    char * data() { return _b.data(); }
    const char * constData() const { return _b.data(); }
    int size() const { return ( int )_b.size(); }
    bool isEmpty() const { return _b.empty(); }
    std::string toStdString() const { return _b; }
    QByteArray & operator+=( const QByteArray & o ) { _b += o._b; return *this; }
    char at( int i ) const { return _b[( size_t )i]; }
    std::string _b;
};


// ---- Qt-free additions: QPixmap alias + file/JSON/QSpatial stubs ------------
typedef QImage QPixmap;

class QIODevice
{
public:
    enum OpenModeFlag { NotOpen = 0, ReadOnly = 0x1, WriteOnly = 0x2, ReadWrite = ReadOnly | WriteOnly };
    virtual ~QIODevice() = default;
};

class QBuffer
{
public:
    QByteArray & buffer() { return _b; }
    const QByteArray & buffer() const { return _b; }
    void setBuffer( const QByteArray & b ) { _b = b; }
    QByteArray _b;
};

class QFile
{
public:
    explicit QFile( const QString & name ) : _name( name ) {}
    bool open( QIODevice::OpenModeFlag ) const { return true; }
    void close() const {}
    bool exists() const { return false; }
    QByteArray readAll() const { return QByteArray(); }
    QString _name;
};

class QFileInfo
{
public:
    explicit QFileInfo( const QString & file ) { exists(); ( void )file; }
    static bool exists( const QString & ) { return false; }
    bool exists() const { return false; }
    bool isFile() const { return false; }
    bool isDir() const { return false; }
};

class QStandardPaths
{
public:
    enum StandardLocation { AppDataLocation = 1, DataLocation = 2, HomeLocation = 3 };
    static QString writableLocation( StandardLocation ) { return QString(); }
    static QStringList standardLocations( StandardLocation ) { return QStringList(); }
};

// ---- QGuiApplication (stub; the wasm entry does not need a widget window) ---
class QGuiApplication
{
public:
    QGuiApplication( int &, char ** ) {}
};

// ---- minimal JSON (used by layout_json) ------------------------------------
class QJsonArray;
class QJsonObject;
class QJsonValue
{
public:
    enum Type { Null, Bool, Double, String, Array, Object };
    QJsonValue() : _t( Null ), _b( false ), _d( 0 ) {}
    QJsonValue( bool b ) : _t( Bool ), _b( b ), _d( 0 ) {}
    QJsonValue( double d ) : _t( Double ), _b( false ), _d( d ) {}
    QJsonValue( int v ) : _t( Double ), _b( false ), _d( v ) {}
    QJsonValue( const QString & s ) : _t( String ), _b( false ), _d( 0 ), _s( s ) {}
    QJsonValue( const QLatin1String & s ) : _t( String ), _b( false ), _d( 0 ), _s( s.data() ) {}
    QJsonValue( const QJsonArray & a );
    QJsonValue( const QJsonObject & o );

    Type type() const { return _t; }
    bool isNull() const { return _t == Null; }
    bool isBool() const { return _t == Bool; }
    bool isDouble() const { return _t == Double; }
    bool isString() const { return _t == String; }
    bool isArray() const { return _t == Array; }
    bool isObject() const { return _t == Object; }
    bool toBool( bool def = false ) const { return _t == Bool ? _b : def; }
    double toDouble( double def = 0 ) const { return _t == Double ? _d : def; }
    int toInt( int def = 0 ) const { return _t == Double ? ( int )_d : def; }
    QString toString( const QString & def = QString() ) const { return _t == String ? _s : def; }
    QJsonArray toArray() const;
    QJsonObject toObject() const;

    Type _t;
    bool _b;
    double _d;
    QString _s;
    std::vector<QJsonValue> _arr;
    std::map<std::string, QJsonValue> _obj;
};

class QJsonArray
{
public:
    int size() const { return ( int )_v.size(); }
    bool isEmpty() const { return _v.empty(); }
    const QJsonValue & at( int i ) const { return _v[( size_t )i]; }
    const QJsonValue & operator[]( int i ) const { return _v[( size_t )i]; }
    void append( const QJsonValue & v ) { _v.push_back( v ); }
    auto begin() const { return _v.cbegin(); }
    auto end() const { return _v.cend(); }
    std::vector<QJsonValue> _v;
};

class QJsonObject
{
public:
    bool contains( const QString & k ) const { return _o.find( k.toStdString() ) != _o.end(); }
    void insert( const QString & k, const QJsonValue & v ) { _o[k.toStdString()] = v; }
    QJsonValue value( const QString & k ) const
    {
        auto it = _o.find( k.toStdString() );
        return it == _o.end() ? QJsonValue() : it->second;
    }
    const QJsonValue operator[]( const QString & k ) const { return value( k ); }
    QStringList keys() const
    {
        QStringList l;
        for ( const auto & p : _o )
            l.append( QString::fromUtf8( p.first ) );
        return l;
    }
    int size() const { return ( int )_o.size(); }
    std::map<std::string, QJsonValue> _o;
};

class QJsonParseError
{
public:
    enum ParseError {
        NoError = 0,
        IllegalValue,
        PrematureEndOfDocument,
        UnterminatedString,
        MissingNameSeparator,
        MissingColon,
        UnterminatedArray,
        UnterminatedObject,
        UnquotedString,
        Unknown,
    };
    QJsonParseError() : error( NoError ) {}
    int error;
    QString errorString() const { return QString( "json parse error" ); }
};

class QJsonDocument
{
public:
    enum JsonFormat { Compact = 0, Indented = 1 };
    QJsonDocument() = default;
    explicit QJsonDocument( const QJsonObject & o ) { _v = QJsonValue( o ); }
    QByteArray toJson( JsonFormat = Compact ) const;
    static QJsonDocument fromJson( const QByteArray & data, QJsonParseError * err = nullptr )
    {
        QJsonDocument d;
        if ( !d._parse( data._b, err ) && err )
            err->error = QJsonParseError::Unknown;
        return d;
    }
    bool isObject() const { return _v.isObject(); }
    QJsonObject object() const { return _v.toObject(); }
    bool isNull() const { return _v.isNull(); }
    QJsonValue _v;
    bool _parse( const std::string & s, QJsonParseError * err );

private:
    size_t _pos = 0;
    const std::string * _src = nullptr;
    void skipWs();
    bool parseValue( QJsonValue & out );
    bool parseObject( QJsonValue & out );
    bool parseArray( QJsonValue & out );
    bool parseString( std::string & out );
};
