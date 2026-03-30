#include "lidl_validator.h"
#include <QSet>

static const QSet<QString>& lidlBuiltinTypes() {
    static const QSet<QString> bt = { "tstr", "bstr", "int", "uint", "float64", "bool", "result", "any" };
    return bt;
}

class Validator {
public:
    explicit Validator(const ModuleDecl& mod) : m_mod(mod) { for (const TypeDecl& td : mod.types) m_declaredTypes.insert(td.name); }

    LidlValidationResult validate() {
        LidlValidationResult result;
        if (m_mod.name.isEmpty()) result.errors.append("Module name is empty");

        QSet<QString> seenTypes;
        for (const TypeDecl& td : m_mod.types) {
            if (lidlBuiltinTypes().contains(td.name)) result.errors.append(QString("Type '%1' shadows a builtin type").arg(td.name));
            if (seenTypes.contains(td.name)) result.errors.append(QString("Duplicate type definition '%1'").arg(td.name));
            seenTypes.insert(td.name);
            for (const FieldDecl& fd : td.fields) validateTypeExpr(fd.type, result);
        }

        QSet<QString> seenMethods;
        for (const MethodDecl& md : m_mod.methods) {
            if (seenMethods.contains(md.name)) result.errors.append(QString("Duplicate method definition '%1'").arg(md.name));
            seenMethods.insert(md.name);
            validateTypeExpr(md.returnType, result);
            QSet<QString> seenParams;
            for (const ParamDecl& pd : md.params) {
                validateTypeExpr(pd.type, result);
                if (seenParams.contains(pd.name)) result.errors.append(QString("Duplicate parameter '%1' in method '%2'").arg(pd.name, md.name));
                seenParams.insert(pd.name);
            }
        }

        QSet<QString> seenEvents;
        for (const EventDecl& ed : m_mod.events) {
            if (seenEvents.contains(ed.name)) result.errors.append(QString("Duplicate event definition '%1'").arg(ed.name));
            seenEvents.insert(ed.name);
            for (const ParamDecl& pd : ed.params) validateTypeExpr(pd.type, result);
        }
        return result;
    }

private:
    const ModuleDecl& m_mod;
    QSet<QString> m_declaredTypes;

    void validateTypeExpr(const TypeExpr& te, LidlValidationResult& result) {
        switch (te.kind) {
        case TypeExpr::Primitive: break;
        case TypeExpr::Named: if (!m_declaredTypes.contains(te.name)) result.errors.append(QString("Unknown type '%1'").arg(te.name)); break;
        case TypeExpr::Array: validateTypeExpr(te.elements[0], result); break;
        case TypeExpr::Map: validateTypeExpr(te.elements[0], result); validateTypeExpr(te.elements[1], result); break;
        case TypeExpr::Optional: validateTypeExpr(te.elements[0], result); break;
        }
    }
};

LidlValidationResult lidlValidate(const ModuleDecl& module) { Validator v(module); return v.validate(); }
