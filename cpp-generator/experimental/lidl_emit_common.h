// Shared LIDL-type -> target-type-name mapping used by the C++/Qt code-emitting
// backends. These are the language-specific half of codegen (the canonical
// frontend — lexer/parser/AST/serializer/validator — lives in logos-lidl); each
// SDK keeps its own type-name mapping (Qt vs std vs Rust).
#pragma once

#include <QString>
#include "lidl_compat.h"

QString lidlToPascalCase(const QString& name);
QString lidlTypeToQt(const TypeExpr& te);
QString lidlTypeToStd(const TypeExpr& te);
bool lidlIsStdConvertible(const TypeExpr& te);
