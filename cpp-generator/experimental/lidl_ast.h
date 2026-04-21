#ifndef LIDL_AST_H
#define LIDL_AST_H

#include <QString>
#include <QStringList>
#include <QVector>

struct TypeExpr {
    enum Kind { Primitive, Array, Map, Optional, Named };
    Kind kind = Primitive;
    QString name; // primitive name ("tstr","int",...) or custom type name
    QVector<TypeExpr> elements; // Array: [0]=elem, Map: [0]=key [1]=val, Optional: [0]=inner

    bool operator==(const TypeExpr& o) const {
        return kind == o.kind && name == o.name && elements == o.elements;
    }
    bool operator!=(const TypeExpr& o) const { return !(*this == o); }
};

struct FieldDecl {
    QString name;
    TypeExpr type;
    bool optional = false;

    bool operator==(const FieldDecl& o) const {
        return name == o.name && type == o.type && optional == o.optional;
    }
};

struct ParamDecl {
    QString name;
    TypeExpr type;

    bool operator==(const ParamDecl& o) const {
        return name == o.name && type == o.type;
    }
};

struct MethodDecl {
    QString name;
    QVector<ParamDecl> params;
    TypeExpr returnType;
    // True when the impl returns LogosMap or LogosList (nlohmann::json).
    // The generator will emit nlohmann→Qt conversion code in the glue layer.
    bool jsonReturn = false;
    // True when the impl returns StdLogosResult.
    // The generator will emit a stdResultToQt() conversion in the glue layer.
    bool resultReturn = false;

    bool operator==(const MethodDecl& o) const {
        return name == o.name && params == o.params && returnType == o.returnType
            && jsonReturn == o.jsonReturn && resultReturn == o.resultReturn;
    }
};

struct EventDecl {
    QString name;
    QVector<ParamDecl> params;

    bool operator==(const EventDecl& o) const {
        return name == o.name && params == o.params;
    }
};

struct TypeDecl {
    QString name;
    QVector<FieldDecl> fields;

    bool operator==(const TypeDecl& o) const {
        return name == o.name && fields == o.fields;
    }
};

struct ModuleDecl {
    QString name;
    QString version;
    QString description;
    QString category;
    QStringList depends;
    QVector<TypeDecl> types;
    QVector<MethodDecl> methods;
    QVector<EventDecl> events;
    // True when the impl header declares a public
    // std::function<void(const std::string&, const std::string&)> emitEvent;
    // The generator will wire it to LogosProviderBase::emitEvent in the glue.
    bool hasEmitEvent = false;

    bool operator==(const ModuleDecl& o) const {
        return name == o.name && version == o.version
            && description == o.description && category == o.category
            && depends == o.depends && types == o.types
            && methods == o.methods && events == o.events
            && hasEmitEvent == o.hasEmitEvent;
    }
};

#endif // LIDL_AST_H
