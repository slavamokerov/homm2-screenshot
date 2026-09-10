// Parity driver using REAL Qt (CLI reference). Draws the shared parity scene and
// prints the RGBA hash + saves a PNG. Linked against Qt6 via the project CMake.
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>

#include <QImage>
#include <QString>

#include "parity_scene.hpp"

int main( int argc, char ** argv )
{
    const QImage img = drawScene();
    const uint64_t h = hashRgba( img );
    std::printf( "hash=%016llx\n", ( unsigned long long )h );
    if ( argc > 1 ) {
        const std::string out = argv[1];
        if ( img.save( QString::fromStdString( out ), "PNG" ) )
            std::printf( "saved %s\n", out.c_str() );
    }
    if ( argc > 2 ) {
        const QImage r = img.convertToFormat( QImage::Format_RGBA8888 );
        const uint8_t * p = r.constBits();
        std::ofstream f( argv[2], std::ios::binary );
        f.write( ( const char * )p, ( std::streamsize )( r.width() * r.height() * 4 ) );
        std::printf( "raw %dx%d\n", r.width(), r.height() );
    }
    return 0;
}
