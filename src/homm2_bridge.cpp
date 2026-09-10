// Bridge to the homm2-to-fheroes2 core (h2core) — see homm2_bridge.h.
// This is the ONLY translation unit that includes the HoMM2 converter headers.

#include <cstdio>
#include <fstream>
#include <iterator>

#include "convert.h"
#include "fheroes2_save.h"
#include "homm2_bridge.h"
#include "homm2_save.h"

namespace homm2 {

namespace {

std::vector<uint8_t> readFileBytes( const std::string & path )
{
    std::ifstream f( path, std::ios::binary );
    if ( !f )
        return {};
    return std::vector<uint8_t>( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
}

bool hasHomm2Extension( const std::string & path )
{
    if ( path.size() < 4 )
        return false;
    const std::string ext = path.substr( path.size() - 4 );
    auto eq = []( const std::string & a, const char * b ) {
        for ( int i = 0; i < 4; ++i )
            if ( ( a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>( a[i] - 'A' + 'a' ) : a[i] ) != b[i] )
                return false;
        return true;
    };
    return eq( ext, ".gm1" ) || eq( ext, ".gm2" ) || eq( ext, ".gmc" )
           || eq( ext, ".gxc" ) || eq( ext, ".gx1" ) || eq( ext, ".gx2" );
}

bool looksHomm2( const std::vector<uint8_t> & data )
{
    // The Price of Loyalty expansion marker at offset 0.
    if ( data.size() >= 4 && data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF )
        return true;
    // A standard .GM1 starts with the little-endian map width (small number),
    // never an fheroes2 version word (10032..10034 = 0x27..).
    if ( data.size() >= 4 ) {
        const uint32_t w = data[0] | ( ( uint32_t )data[1] << 8 );
        return w >= 8 && w <= 256;
    }
    return false;
}

bool convertBytes( const std::vector<uint8_t> & data, std::vector<uint8_t> & savBytes, std::string & error )
{
    h2::Save save;
    if ( !h2::parseSave( data, save ) ) {
        error = "not a supported Heroes of Might and Magic II save";
        return false;
    }

    h2conv::WorldData world;
    h2conv::ConvertOptions options;
    if ( !h2::convert( save, world, options ) ) {
        error = "HoMM2 conversion failed";
        return false;
    }

    savBytes = h2conv::buildSaveFile( save.header, world, options );
    if ( savBytes.empty() ) {
        error = "failed to build the fheroes2 save from the HoMM2 save";
        return false;
    }
    return true;
}

} // namespace

bool isHomm2Save( const std::string & path )
{
    if ( hasHomm2Extension( path ) )
        return true;
    return looksHomm2( readFileBytes( path ) );
}

bool isHomm2SaveBytes( const std::vector<uint8_t> & data )
{
    return looksHomm2( data );
}

bool loadHomm2SaveToSavBytes( const std::string & srcPath, std::vector<uint8_t> & savBytes, std::string & error )
{
    const std::vector<uint8_t> data = readFileBytes( srcPath );
    if ( data.empty() ) {
        error = "cannot read the HoMM2 save";
        return false;
    }
    return convertBytes( data, savBytes, error );
}

bool loadHomm2SaveToSavBytesFromBytes( const std::vector<uint8_t> & data, std::vector<uint8_t> & savBytes, std::string & error )
{
    return convertBytes( data, savBytes, error );
}

} // namespace homm2
