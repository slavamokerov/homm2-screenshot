#include "layout_json.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fh2poster {

namespace {

layout::Side parseSide( const QString & s, bool * ok )
{
    *ok = true;
    if ( s == QLatin1String( "top" ) )
        return layout::Side::TOP;
    if ( s == QLatin1String( "bottom" ) )
        return layout::Side::BOTTOM;
    if ( s == QLatin1String( "left" ) )
        return layout::Side::LEFT;
    if ( s == QLatin1String( "right" ) )
        return layout::Side::RIGHT;
    *ok = false;
    return layout::Side::BOTTOM;
}

const char * sideName( layout::Side side )
{
    switch ( side ) {
    case layout::Side::TOP: return "top";
    case layout::Side::BOTTOM: return "bottom";
    case layout::Side::LEFT: return "left";
    case layout::Side::RIGHT: return "right";
    }
    return "bottom";
}

layout::GridParams parseGrid( const QJsonObject & obj, bool * ok, QString * error )
{
    *ok = true;
    layout::GridParams g;
    if ( obj.contains( QLatin1String( "capW" ) ) )
        g.capW = obj.value( QLatin1String( "capW" ) ).toInt();
    if ( obj.contains( QLatin1String( "floorW" ) ) )
        g.floorW = obj.value( QLatin1String( "floorW" ) ).toInt();
    if ( obj.contains( QLatin1String( "aspect" ) ) )
        g.aspect = static_cast<float>( obj.value( QLatin1String( "aspect" ) ).toDouble() );
    if ( obj.contains( QLatin1String( "maxRows" ) ) )
        g.maxRows = obj.value( QLatin1String( "maxRows" ) ).toInt();
    if ( obj.contains( QLatin1String( "maxZoneFrac" ) ) )
        g.maxZoneFrac = static_cast<float>( obj.value( QLatin1String( "maxZoneFrac" ) ).toDouble() );
    if ( obj.contains( QLatin1String( "gap" ) ) )
        g.gap = obj.value( QLatin1String( "gap" ) ).toInt();
    if ( obj.contains( QLatin1String( "padding" ) ) )
        g.padding = obj.value( QLatin1String( "padding" ) ).toInt();

    if ( g.aspect <= 0.0f ) {
        *ok = false;
        if ( error )
            *error = QStringLiteral( "grid 'aspect' must be positive" );
    }
    else if ( g.maxRows < 1 ) {
        *ok = false;
        if ( error )
            *error = QStringLiteral( "grid 'maxRows' must be >= 1" );
    }
    else if ( g.gap < 0 || g.padding < 0 ) {
        *ok = false;
        if ( error )
            *error = QStringLiteral( "grid 'gap'/'padding' must be >= 0" );
    }
    return g;
}

QJsonObject gridToJson( const layout::GridParams & g )
{
    QJsonObject o;
    o.insert( QStringLiteral( "capW" ), g.capW );
    o.insert( QStringLiteral( "floorW" ), g.floorW );
    o.insert( QStringLiteral( "aspect" ), static_cast<double>( g.aspect ) );
    o.insert( QStringLiteral( "maxRows" ), g.maxRows );
    o.insert( QStringLiteral( "maxZoneFrac" ), static_cast<double>( g.maxZoneFrac ) );
    o.insert( QStringLiteral( "gap" ), g.gap );
    o.insert( QStringLiteral( "padding" ), g.padding );
    return o;
}

} // namespace

bool layoutFromJson( const QByteArray & json, layout::LayoutParams & out, QString * error )
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson( json, &parseError );
    if ( parseError.error != QJsonParseError::NoError || !doc.isObject() ) {
        if ( error )
            *error = QStringLiteral( "invalid JSON: %1" ).arg( parseError.errorString() );
        return false;
    }

    const QJsonObject root = doc.object();
    layout::LayoutParams params;
    params.name = root.value( QLatin1String( "name" ) ).toString().toStdString();
    if ( root.contains( QLatin1String( "frame" ) ) )
        params.frame = root.value( QLatin1String( "frame" ) ).toInt();
    if ( root.contains( QLatin1String( "sideMinFrac" ) ) )
        params.sideMinFrac = static_cast<float>( root.value( QLatin1String( "sideMinFrac" ) ).toDouble() );
    if ( root.contains( QLatin1String( "sideWidth" ) ) )
        params.sideWidth = root.value( QLatin1String( "sideWidth" ) ).toInt();

    const QJsonValue zonesValue = root.value( QLatin1String( "zones" ) );
    if ( !zonesValue.isArray() ) {
        if ( error )
            *error = QStringLiteral( "missing 'zones' array" );
        return false;
    }

    const QJsonArray zones = zonesValue.toArray();
    for ( const QJsonValue & zoneValue : zones ) {
        if ( !zoneValue.isObject() ) {
            if ( error )
                *error = QStringLiteral( "each zone must be an object" );
            return false;
        }
        const QJsonObject zone = zoneValue.toObject();

        layout::ZoneDef z;
        z.id = zone.value( QLatin1String( "id" ) ).toString().toStdString();
        if ( z.id.empty() ) {
            if ( error )
                *error = QStringLiteral( "zone 'id' is required" );
            return false;
        }

        bool ok = false;
        z.side = parseSide( zone.value( QLatin1String( "side" ) ).toString(), &ok );
        if ( !ok ) {
            if ( error )
                *error = QStringLiteral( "zone '%1': 'side' must be top|bottom|left|right" ).arg( QString::fromStdString( z.id ) );
            return false;
        }

        z.single = zone.value( QLatin1String( "single" ) ).toBool();
        if ( z.single ) {
            if ( !zone.contains( QLatin1String( "singleAspect" ) ) ) {
                if ( error )
                    *error = QStringLiteral( "zone '%1': 'singleAspect' is required for single zones" ).arg( QString::fromStdString( z.id ) );
                return false;
            }
            z.singleAspect = static_cast<float>( zone.value( QLatin1String( "singleAspect" ) ).toDouble() );
            if ( z.singleAspect <= 0.0f ) {
                if ( error )
                    *error = QStringLiteral( "zone '%1': 'singleAspect' must be positive" ).arg( QString::fromStdString( z.id ) );
                return false;
            }
        }
        else {
            const QJsonValue gridValue = zone.value( QLatin1String( "grid" ) );
            if ( !gridValue.isObject() ) {
                if ( error )
                    *error = QStringLiteral( "zone '%1': missing 'grid' object" ).arg( QString::fromStdString( z.id ) );
                return false;
            }
            z.grid = parseGrid( gridValue.toObject(), &ok, error );
            if ( !ok )
                return false;
        }

        params.zones.push_back( z );
    }

    if ( params.zones.empty() ) {
        if ( error )
            *error = QStringLiteral( "at least one zone is required" );
        return false;
    }

    out = std::move( params );
    return true;
}

QByteArray layoutToJson( const layout::LayoutParams & params )
{
    QJsonArray zones;
    for ( const layout::ZoneDef & z : params.zones ) {
        QJsonObject zone;
        zone.insert( QStringLiteral( "id" ), QString::fromStdString( z.id ) );
        zone.insert( QStringLiteral( "side" ), QLatin1String( sideName( z.side ) ) );
        if ( z.single ) {
            zone.insert( QStringLiteral( "single" ), true );
            zone.insert( QStringLiteral( "singleAspect" ), static_cast<double>( z.singleAspect ) );
        }
        else {
            zone.insert( QStringLiteral( "single" ), false );
            zone.insert( QStringLiteral( "grid" ), gridToJson( z.grid ) );
        }
        zones.append( zone );
    }

    QJsonObject root;
    root.insert( QStringLiteral( "name" ), QString::fromStdString( params.name ) );
    root.insert( QStringLiteral( "frame" ), params.frame );
    root.insert( QStringLiteral( "sideMinFrac" ), static_cast<double>( params.sideMinFrac ) );
    root.insert( QStringLiteral( "sideWidth" ), params.sideWidth );
    root.insert( QStringLiteral( "zones" ), zones );

    return QJsonDocument( root ).toJson( QJsonDocument::Indented );
}

} // namespace fh2poster
