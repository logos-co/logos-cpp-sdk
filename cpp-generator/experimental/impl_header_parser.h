#ifndef IMPL_HEADER_PARSER_H
#define IMPL_HEADER_PARSER_H

#include "lidl_ast.h"
#include <QString>
#include <QTextStream>

// Parse a C++ impl header to extract public method signatures,
// then combine with metadata.json to build a ModuleDecl.
// This eliminates the need for a separate .lidl file.
//
// Supported C++ types → LIDL types:
//   bool                          → bool
//   int64_t                       → int
//   uint64_t                      → uint
//   double                        → float64
//   std::string / const std::string& → tstr
//   std::vector<std::string>      → [tstr]
//   std::vector<uint8_t>          → bstr
//   std::vector<int64_t>          → [int]
//   std::vector<uint64_t>         → [uint]
//   void                          → void

struct ImplParseResult {
    ModuleDecl module;
    QString error;
    bool hasError() const { return !error.isEmpty(); }
};

ImplParseResult parseImplHeader(const QString& headerPath,
                                const QString& className,
                                const QString& metadataPath,
                                QTextStream& err);

#endif // IMPL_HEADER_PARSER_H
