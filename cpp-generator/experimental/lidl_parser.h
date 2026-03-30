#ifndef LIDL_PARSER_H
#define LIDL_PARSER_H

#include "lidl_ast.h"
#include <QString>

struct LidlParseResult {
    ModuleDecl module;
    QString error;
    int errorLine = 0;
    int errorColumn = 0;
    bool hasError() const { return !error.isEmpty(); }
};

LidlParseResult lidlParse(const QString& source);

#endif // LIDL_PARSER_H
