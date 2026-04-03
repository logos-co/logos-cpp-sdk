#ifndef C_PLUGIN_GENERATOR_H
#define C_PLUGIN_GENERATOR_H

#include "lidl_ast.h"
#include <QString>

// Generate a complete Qt plugin header (interface + plugin class with Q_INVOKABLE methods)
// that directly calls C functions via FFI.
// cHeaderInclude: the include path for the C header (e.g., "rust_calc.h")
// prefix: the C function name prefix (e.g., "rust_calc_")
QString cPluginMakeHeader(const ModuleDecl& module,
                          const QString& cHeaderInclude,
                          const QString& prefix);

// Generate the plugin implementation source that calls C functions.
QString cPluginMakeSource(const ModuleDecl& module,
                          const QString& prefix);

#endif // C_PLUGIN_GENERATOR_H
