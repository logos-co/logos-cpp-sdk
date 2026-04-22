#ifndef LIDL_GEN_PROVIDER_H
#define LIDL_GEN_PROVIDER_H

#include "lidl_ast.h"
#include "c_header_parser.h"
#include <QString>
#include <QTextStream>

// Map a LIDL TypeExpr to the C++ std type string used in the pure implementation class
QString lidlTypeToStd(const TypeExpr& te);

// True if this type can be represented as a pure C++ std type (no Qt)
bool lidlIsStdConvertible(const TypeExpr& te);

// Generate the Qt glue header (plugin class + provider object with Q_INVOKABLE wrappers)
QString lidlMakeProviderHeader(const ModuleDecl& module,
                               const QString& implClass,
                               const QString& implHeader);

// Generate callMethod() + getMethods() dispatch source
QString lidlMakeProviderDispatch(const ModuleDecl& module);

// Full pipeline: parse .lidl, generate provider glue + dispatch + metadata
// Returns 0 on success, non-zero on error
int lidlGenerateProviderGlue(const QString& lidlPath,
                              const QString& implClass,
                              const QString& implHeader,
                              const QString& outputDir,
                              QTextStream& out, QTextStream& err);

// ---------------------------------------------------------------------------
// C-FFI variants: call C functions directly (no C++ impl class required)
// ---------------------------------------------------------------------------

// Generate the Qt glue header that calls C functions directly.
// cParseResult carries the C function names and string-ownership info.
// freeStringFunc is the name of the free function (e.g. "rust_example_free_string"),
// or empty if none was found (heap char* returns will be leaked with a warning comment).
QString lidlMakeProviderHeaderCFFI(const CHeaderParseResult& cParseResult);

// Generate callMethod() + getMethods() dispatch source for C-FFI mode.
QString lidlMakeProviderDispatchCFFI(const CHeaderParseResult& cParseResult);

#endif // LIDL_GEN_PROVIDER_H
