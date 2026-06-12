// Shared emit helpers used by every code-emitting backend (the Qt-free
// client/wrapper generation here in cpp-sdk AND the Qt glue generation in
// logos-qt-sdk's logos-qt-generator). Distributed with the LIDL frontend
// sources (share/lidl-frontend/) so external generators compile them in.
#pragma once

#include <QString>
#include "lidl_ast.h"

QString lidlToPascalCase(const QString& name);
QString lidlTypeToQt(const TypeExpr& te);
QString lidlTypeToStd(const TypeExpr& te);
bool lidlIsStdConvertible(const TypeExpr& te);
