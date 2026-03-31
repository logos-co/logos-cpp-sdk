#include "impl_header_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>

// ---------------------------------------------------------------------------
// C++ type string → LIDL TypeExpr
// ---------------------------------------------------------------------------

static TypeExpr cppTypeToLidl(const QString& raw)
{
    // Normalize: strip const, &, leading/trailing whitespace
    QString t = raw.trimmed();
    t.remove(QRegularExpression("^const\\s+"));
    t.remove(QRegularExpression("\\s*&$"));
    t = t.trimmed();

    // Primitives
    if (t == "bool")     return { TypeExpr::Primitive, "bool", {} };
    if (t == "int64_t")  return { TypeExpr::Primitive, "int", {} };
    if (t == "uint64_t") return { TypeExpr::Primitive, "uint", {} };
    if (t == "double")   return { TypeExpr::Primitive, "float64", {} };
    if (t == "void")     return { TypeExpr::Primitive, "void", {} };

    // std::string
    if (t == "std::string")
        return { TypeExpr::Primitive, "tstr", {} };

    // std::vector<T>
    static QRegularExpression vecRe("^std::vector\\s*<\\s*(.+)\\s*>$");
    QRegularExpressionMatch m = vecRe.match(t);
    if (m.hasMatch()) {
        QString inner = m.captured(1).trimmed();
        if (inner == "std::string") {
            TypeExpr elem = { TypeExpr::Primitive, "tstr", {} };
            return { TypeExpr::Array, "", { elem } };
        }
        if (inner == "uint8_t") {
            return { TypeExpr::Primitive, "bstr", {} };
        }
        if (inner == "int64_t") {
            TypeExpr elem = { TypeExpr::Primitive, "int", {} };
            return { TypeExpr::Array, "", { elem } };
        }
        if (inner == "uint64_t") {
            TypeExpr elem = { TypeExpr::Primitive, "uint", {} };
            return { TypeExpr::Array, "", { elem } };
        }
        if (inner == "double") {
            TypeExpr elem = { TypeExpr::Primitive, "float64", {} };
            return { TypeExpr::Array, "", { elem } };
        }
        if (inner == "bool") {
            TypeExpr elem = { TypeExpr::Primitive, "bool", {} };
            return { TypeExpr::Array, "", { elem } };
        }
    }

    // Fallback: treat as opaque
    return { TypeExpr::Primitive, "any", {} };
}

// ---------------------------------------------------------------------------
// Parse a single method declaration line
// ---------------------------------------------------------------------------

static bool parseMethodLine(const QString& line, MethodDecl& out)
{
    // Find the parameter list: everything between the last '(' and ')'
    int parenOpen = -1;
    int parenClose = -1;
    int depth = 0;
    for (int i = line.size() - 1; i >= 0; --i) {
        if (line[i] == ')') {
            if (parenClose < 0) parenClose = i;
            depth++;
        } else if (line[i] == '(') {
            depth--;
            if (depth == 0) {
                parenOpen = i;
                break;
            }
        }
    }
    if (parenOpen < 0 || parenClose < 0)
        return false;

    QString paramStr = line.mid(parenOpen + 1, parenClose - parenOpen - 1).trimmed();

    // Everything before '(' is "returnType methodName"
    QString prefix = line.left(parenOpen).trimmed();

    // The method name is the last identifier token in prefix
    int nameEnd = prefix.size();
    while (nameEnd > 0 && prefix[nameEnd - 1].isSpace())
        nameEnd--;
    int nameStart = nameEnd;
    while (nameStart > 0 && (prefix[nameStart - 1].isLetterOrNumber() || prefix[nameStart - 1] == '_'))
        nameStart--;

    if (nameStart >= nameEnd)
        return false;

    out.name = prefix.mid(nameStart, nameEnd - nameStart);
    QString retTypeStr = prefix.left(nameStart).trimmed();
    out.returnType = cppTypeToLidl(retTypeStr);

    // Parse parameters
    out.params.clear();
    if (!paramStr.isEmpty()) {
        // Split by comma, respecting template depth
        QStringList parts;
        int start = 0;
        int tdepth = 0;
        for (int i = 0; i < paramStr.size(); ++i) {
            if (paramStr[i] == '<') tdepth++;
            else if (paramStr[i] == '>') tdepth--;
            else if (paramStr[i] == ',' && tdepth == 0) {
                parts.append(paramStr.mid(start, i - start).trimmed());
                start = i + 1;
            }
        }
        parts.append(paramStr.mid(start).trimmed());

        for (const QString& part : parts) {
            if (part.isEmpty()) continue;
            QString p = part.trimmed();
            int pNameEnd = p.size();
            while (pNameEnd > 0 && p[pNameEnd - 1].isSpace())
                pNameEnd--;
            int pNameStart = pNameEnd;
            while (pNameStart > 0 && (p[pNameStart - 1].isLetterOrNumber() || p[pNameStart - 1] == '_'))
                pNameStart--;

            if (pNameStart >= pNameEnd) continue;

            ParamDecl pd;
            pd.name = p.mid(pNameStart, pNameEnd - pNameStart);
            pd.type = cppTypeToLidl(p.left(pNameStart));
            out.params.append(pd);
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

ImplParseResult parseImplHeader(const QString& headerPath,
                                const QString& className,
                                const QString& metadataPath,
                                QTextStream& err)
{
    ImplParseResult result;

    // --- Read metadata.json ---
    {
        QFile mf(metadataPath);
        if (!mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.error = "Failed to open metadata file: " + metadataPath;
            return result;
        }
        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(mf.readAll(), &pe);
        if (pe.error != QJsonParseError::NoError) {
            result.error = "Failed to parse metadata JSON: " + pe.errorString();
            return result;
        }
        QJsonObject obj = doc.object();
        result.module.name = obj.value("name").toString();
        result.module.version = obj.value("version").toString();
        result.module.description = obj.value("description").toString();
        result.module.category = obj.value("category").toString();
        QJsonArray deps = obj.value("dependencies").toArray();
        for (const QJsonValue& v : deps)
            result.module.depends.append(v.toString());
    }

    // --- Read and parse header ---
    QFile hf(headerPath);
    if (!hf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = "Failed to open header file: " + headerPath;
        return result;
    }
    QString source = QString::fromUtf8(hf.readAll());
    hf.close();

    QStringList lines = source.split('\n');

    // State machine: find "class <className>", then collect public methods
    enum State { LookingForClass, InClass, InPublic, InPrivate };
    State state = LookingForClass;
    int braceDepth = 0;

    QRegularExpression classRe("\\bclass\\s+" + QRegularExpression::escape(className) + "\\b");
    QRegularExpression accessRe("^\\s*(public|private|protected)\\s*:");
    QRegularExpression ctorDtorRe("^\\s*~?" + QRegularExpression::escape(className) + "\\s*\\(");

    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();

        switch (state) {
        case LookingForClass:
            if (classRe.match(line).hasMatch()) {
                state = InClass;
                for (QChar c : line) {
                    if (c == '{') braceDepth++;
                    else if (c == '}') braceDepth--;
                }
            }
            break;

        case InClass:
        case InPublic:
        case InPrivate:
            for (QChar c : line) {
                if (c == '{') braceDepth++;
                else if (c == '}') braceDepth--;
            }

            if (braceDepth <= 0) {
                state = LookingForClass;
                goto done;
            }

            {
                QRegularExpressionMatch am = accessRe.match(line);
                if (am.hasMatch()) {
                    QString spec = am.captured(1);
                    if (spec == "public") state = InPublic;
                    else state = InPrivate;
                    break;
                }
            }

            if (state != InPublic) break;

            if (line.isEmpty() || line.startsWith("//") || line.startsWith("#")
                || line.startsWith("/*") || line.startsWith("*"))
                break;

            if (ctorDtorRe.match(line).hasMatch())
                break;

            if (line.startsWith("typedef") || line.startsWith("using")
                || line.startsWith("friend") || line.startsWith("enum")
                || line.startsWith("struct"))
                break;

            if (line.endsWith(';')) {
                QString decl = line.left(line.size() - 1).trimmed();
                MethodDecl md;
                if (parseMethodLine(decl, md)) {
                    result.module.methods.append(md);
                }
            }
            break;
        }
    }

done:
    if (result.module.methods.isEmpty()) {
        err << "Warning: no public methods found in class " << className
            << " in " << headerPath << "\n";
    }

    return result;
}
