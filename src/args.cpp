#include "args.h"

#include <cstdlib>
#include <cstring>

namespace fh2poster {

namespace {

bool isNumber( const std::string & s )
{
    if ( s.empty() )
        return false;
    size_t i = ( s[0] == '-' ) ? 1 : 0;
    if ( i == s.size() )
        return false;
    for ( ; i < s.size(); ++i )
        if ( s[i] < '0' || s[i] > '9' )
            return false;
    return true;
}

} // namespace

Args parseArgs( int argc, char ** argv )
{
    Args a;

    for ( int i = 1; i < argc; ++i ) {
        const std::string arg = argv[i];
        auto next = [&]( const char * opt ) -> std::string {
            if ( i + 1 >= argc ) {
                a.error = std::string( "missing value for " ) + opt;
                return {};
            }
            return argv[++i];
        };

        if ( arg == "--help" || arg == "-h" ) {
            a.showHelp = true;
        }
        else if ( arg == "--out" ) {
            a.out = next( "--out" );
        }
        else if ( arg == "--fog" ) {
            a.fog = next( "--fog" );
        }
        else if ( arg == "--scale" ) {
            a.scale = std::atof( next( "--scale" ).c_str() );
        }
        else if ( arg == "--dpi" ) {
            a.dpi = std::atoi( next( "--dpi" ).c_str() );
        }
        else if ( arg == "--layout" ) {
            a.layout = next( "--layout" );
        }
        else if ( arg == "--layout-priority" ) {
            a.layoutPriority = next( "--layout-priority" );
        }
        else if ( arg == "--blocks" ) {
            a.blocks = next( "--blocks" );
        }
        else if ( arg == "--chips" ) {
            a.chips = next( "--chips" );
        }
        else if ( arg == "--routes" ) {
            a.routes = next( "--routes" );
        }
        else if ( arg == "--data-dir" ) {
            a.dataDir = next( "--data-dir" );
        }
        else if ( arg == "--no-castles" ) {
            a.castlesOverride = 0;
        }
        else if ( arg == "--no-heroes" ) {
            a.heroesOverride = 0;
        }
        else if ( arg == "--castles" ) {
            a.castlesOverride = std::atoi( next( "--castles" ).c_str() );
        }
        else if ( arg == "--heroes" ) {
            a.heroesOverride = std::atoi( next( "--heroes" ).c_str() );
        }
        else if ( arg == "--crop" ) {
            // x,y,w,h in tiles
            const std::string v = next( "--crop" );
            std::sscanf( v.c_str(), "%d,%d,%d,%d", &a.crop[0], &a.crop[1], &a.crop[2], &a.crop[3] );
        }
        else if ( arg == "--castle" ) {
            a.castleIndex = std::atoi( next( "--castle" ).c_str() );
        }
        else if ( arg == "--icn" ) {
            a.icnDump = next( "--icn" );
        }
        else if ( arg == "--icnsheet" ) {
            a.icnSheet = next( "--icnsheet" );
        }
        else if ( arg == "--hero" ) {
            a.heroId = std::atoi( next( "--hero" ).c_str() );
        }
        else if ( arg == "--chip" ) {
            a.chipDump = next( "--chip" );
        }
        else if ( arg == "--dump-layout" ) {
            a.dumpLayout = next( "--dump-layout" );
        }
        else if ( arg == "--debug-map" ) {
            a.debugMap = true;
        }
        else if ( arg == "--quiet" ) {
            a.quiet = true;
        }
        else if ( !arg.empty() && arg[0] == '-' ) {
            a.error = "unknown option: " + arg;
        }
        else if ( a.savePath.empty() ) {
            a.savePath = arg;
        }
        else {
            a.error = "unexpected argument: " + arg;
        }

        if ( !a.error.empty() )
            return a;
    }

    if ( !a.showHelp && a.savePath.empty() )
        a.error = "no save file given";

    return a;
}

} // namespace fh2poster
