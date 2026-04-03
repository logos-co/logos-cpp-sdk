#ifndef C_HEADER_PARSER_H
#define C_HEADER_PARSER_H

#include "lidl_ast.h"
#include <QString>
#include <QTextStream>

struct CHeaderParseResult {
    ModuleDecl module;
    QString prefix;   // detected or supplied function prefix (e.g. "rust_calc_")
    QString error;
    bool hasError() const { return !error.isEmpty(); }
};

// Parse a C header with plain function declarations.
// Functions matching {prefix}{method_name}(args) are extracted as module methods.
// prefix: if empty, auto-derived as "{module_name}_" from metadata.json name field.
CHeaderParseResult parseCHeader(const QString& headerPath,
                                const QString& prefix,
                                const QString& metadataPath,
                                QTextStream& err);

#endif // C_HEADER_PARSER_H
