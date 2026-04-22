#ifndef C_HEADER_PARSER_H
#define C_HEADER_PARSER_H

#include "lidl_ast.h"
#include <QString>
#include <QTextStream>

// Result of parsing a plain C header for Logos module generation.
// Extends ModuleDecl with C-FFI-specific metadata needed by the code generator.
struct CHeaderMethod {
    MethodDecl decl;
    QString cFunctionName;   // original C function name (e.g. "rust_example_add")
    bool returnsHeapString;  // true when return type is char* (non-const) → needs free
};

struct CHeaderParseResult {
    ModuleDecl module;
    QVector<CHeaderMethod> methods; // parallel to module.methods, carries C names + ownership
    QString freeStringFunc;         // e.g. "rust_example_free_string" (empty if not found)
    QString cHeaderInclude;         // the #include name to embed in generated code
    QString error;
    bool hasError() const { return !error.isEmpty(); }
};

// Parse a C header file and extract function declarations that start with `prefix`.
// The prefix is stripped to derive Logos method names.
// Module metadata (name, version, description, category, dependencies) is read from metadataPath.
// cHeaderInclude is the string used in #include "..." in generated files.
//
// Auto-detects the free-string function: any function matching
//   void  <prefix>free_string(char*)
// is removed from the method list and stored in freeStringFunc.
//
// Reserved method names (name, version, initLogos) are automatically renamed to
// libName, libVersion, libInitLogos.
CHeaderParseResult parseCHeader(const QString& headerPath,
                                const QString& prefix,
                                const QString& metadataPath,
                                const QString& cHeaderInclude,
                                QTextStream& err);

// Derive the default prefix from a module name:
//   "rust_example_module" → "rust_example_"
//   "my_module"           → "my_"
QString defaultPrefixFromModuleName(const QString& moduleName);

#endif // C_HEADER_PARSER_H
