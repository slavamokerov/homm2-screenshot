// Parity driver using the rastercompat mini-Qt (WASM build). Draws the SAME
// shared scene and prints the RGBA hash + saves a PNG. Linked WITHOUT Qt.
#include <cstdio>
#include <cstdint>
#include <fstream>

#include <QImage>

#include "parity_scene.hpp"

int main( int argc, char ** argv )
{
    const QImage img = drawScene();
    const uint64_t h = hashRgba( img );
    std::printf( "hash=%016llx\n", ( unsigned long long )h );
    if ( argc > 1 ) {
        const std::string out = argv[1];
        if ( img.save( out, "PNG" ) )
            std::printf( "saved %s\n", out.c_str() );
    }
    if ( argc > 2 ) {
        const std::vector<uint8_t> rgba = img.toRgba8();
        std::ofstream f( argv[2], std::ios::binary );
        f.write( ( const char * )rgba.data(), ( std::streamsize )rgba.size() );
        std::printf( "raw %dx%d\n", img.width(), img.height() );
    }
    return 0;
}
