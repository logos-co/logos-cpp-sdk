#ifndef LIDL_VALIDATOR_H
#define LIDL_VALIDATOR_H

#include "lidl_ast.h"
#include <QString>
#include <QStringList>

struct LidlValidationResult {
    QStringList errors;
    QStringList warnings;
    bool hasErrors() const { return !errors.isEmpty(); }
};

LidlValidationResult lidlValidate(const ModuleDecl& module);

#endif // LIDL_VALIDATOR_H
