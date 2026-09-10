#include <QByteArray>
#include <QImage>

namespace fh2poster {

// Encodes a poster as an indexed PNG (<=256 colours with per-entry alpha via
// tRNS) so the output is a fraction of the raw 32-bit size while keeping the
// limited game palette and semi-transparent card frames.
QByteArray toQuantizedPng( const QImage & img );

} // namespace fh2poster
