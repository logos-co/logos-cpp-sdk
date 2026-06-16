#ifndef LIDL_COMPAT_H
#define LIDL_COMPAT_H

// Bridge the cpp-generator's Qt-flavored codegen backends onto the canonical
// logos-lidl frontend. The lexer / parser / AST / serializer / validator now
// live in logos-lidl (std-typed, language-neutral); this header brings those
// types into the global scope the backends use unqualified and adds thin
// Qt-friendly shims so the existing emission code (QTextStream) keeps
// compiling. The backends themselves (impl-header parsing, gen_client,
// gen_cdylib) stay here — they are the C++/Qt-specific parts.

#include "lidl/ast.hpp"
#include "lidl/parser.hpp"
#include "lidl/serializer.hpp"
#include "lidl/validator.hpp"

#include <QString>
#include <QTextStream>
#include <string>

// The canonical AST, in the global scope the generator backends reference it
// from (they predate the logos-lidl extraction and use the unqualified names).
using lidl::TypeExpr;
using lidl::FieldDecl;
using lidl::ParamDecl;
using lidl::MethodDecl;
using lidl::EventDecl;
using lidl::TypeDecl;
using lidl::ModuleDecl;

// std::string -> QString, and let QTextStream accept std::string directly so
// emission of AST string fields (`s << md.name`) keeps compiling unchanged.
inline QString qs(const std::string& s) { return QString::fromStdString(s); }
inline QTextStream& operator<<(QTextStream& s, const std::string& v)
{
    return s << QString::fromStdString(v);
}

// Name-compatible shims over the canonical frontend so the call sites that used
// the deleted experimental lexer/parser/serializer/validator keep their shape.
using LidlParseResult = lidl::ParseResult;
using LidlValidationResult = lidl::ValidationResult;

inline lidl::ParseResult lidlParse(const QString& source)
{
    return lidl::parse(source.toStdString());
}
inline QString lidlSerialize(const ModuleDecl& module)
{
    return QString::fromStdString(lidl::serialize(module));
}
inline lidl::ValidationResult lidlValidate(const ModuleDecl& module)
{
    return lidl::validate(module);
}

#endif // LIDL_COMPAT_H
