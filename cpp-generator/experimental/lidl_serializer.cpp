#include "lidl_serializer.h"
#include <QTextStream>

// Escape a description for a "..."-delimited LIDL string literal (the lexer
// decodes \\ \" \n \t). Keeps method/event docs intact across a .lidl
// round-trip so introspection (lm / getMethods) still shows them.
static QString lidlEscapeStr(QString in) {
    in.replace('\\', "\\\\");
    in.replace('"', "\\\"");
    in.replace('\n', "\\n");
    in.replace('\t', "\\t");
    return in;
}

static QString serializeTypeExpr(const TypeExpr& te) {
    switch (te.kind) {
    case TypeExpr::Primitive: case TypeExpr::Named: return te.name;
    case TypeExpr::Array: return "[" + serializeTypeExpr(te.elements[0]) + "]";
    case TypeExpr::Map: return "{" + serializeTypeExpr(te.elements[0]) + ": " + serializeTypeExpr(te.elements[1]) + "}";
    case TypeExpr::Optional: return "? " + serializeTypeExpr(te.elements[0]);
    }
    return QString();
}

static void serializeParams(QTextStream& s, const QVector<ParamDecl>& params) {
    for (int i = 0; i < params.size(); ++i) {
        s << params[i].name << ": " << serializeTypeExpr(params[i].type);
        if (i + 1 < params.size()) s << ", ";
    }
}

QString lidlSerialize(const ModuleDecl& module) {
    QString out;
    QTextStream s(&out);
    s << "module " << module.name << " {\n";
    if (!module.version.isEmpty()) s << "  version \"" << module.version << "\"\n";
    if (!module.description.isEmpty()) s << "  description \"" << lidlEscapeStr(module.description) << "\"\n";
    if (!module.category.isEmpty()) s << "  category \"" << module.category << "\"\n";
    s << "  depends [";
    for (int i = 0; i < module.depends.size(); ++i) { s << module.depends[i]; if (i + 1 < module.depends.size()) s << ", "; }
    s << "]\n";
    for (const TypeDecl& td : module.types) { s << "\n  type " << td.name << " {\n"; for (const FieldDecl& fd : td.fields) { s << "    "; if (fd.optional) s << "? "; s << fd.name << ": " << serializeTypeExpr(fd.type) << "\n"; } s << "  }\n"; }
    if (!module.methods.isEmpty()) s << "\n";
    for (const MethodDecl& md : module.methods) { s << "  method " << md.name << "("; serializeParams(s, md.params); s << ") -> " << serializeTypeExpr(md.returnType); if (!md.description.isEmpty()) s << " description \"" << lidlEscapeStr(md.description) << "\""; s << "\n"; }
    if (!module.events.isEmpty()) s << "\n";
    for (const EventDecl& ed : module.events) { s << "  event " << ed.name << "("; serializeParams(s, ed.params); s << ")"; if (!ed.description.isEmpty()) s << " description \"" << lidlEscapeStr(ed.description) << "\""; s << "\n"; }
    s << "}\n";
    return out;
}
