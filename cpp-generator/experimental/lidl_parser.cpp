#include "lidl_parser.h"
#include "lidl_lexer.h"

#include <QSet>

static const QSet<QString>& lidlBuiltinTypes()
{
    static const QSet<QString> bt = {
        "tstr", "bstr", "int", "uint", "float64", "bool", "result", "any"
    };
    return bt;
}

class Parser {
public:
    explicit Parser(const QVector<LidlToken>& tokens) : m_tokens(tokens) {}

    LidlParseResult parse() {
        LidlParseResult result;
        if (!parseModule(result.module)) {
            result.error = m_error; result.errorLine = m_errorLine; result.errorColumn = m_errorColumn;
        }
        return result;
    }

private:
    const QVector<LidlToken>& m_tokens;
    int m_pos = 0;
    QString m_error;
    int m_errorLine = 0, m_errorColumn = 0;

    const LidlToken& current() const { return m_tokens[m_pos < m_tokens.size() ? m_pos : m_tokens.size() - 1]; }
    bool at(LidlToken::Type type) const { return current().type == type; }
    bool consume(LidlToken::Type type) { if (!at(type)) return false; ++m_pos; return true; }

    bool expect(LidlToken::Type type, const QString& context) {
        if (consume(type)) return true;
        error(QString("Expected %1 in %2, got '%3'").arg(tokenTypeName(type), context, current().text));
        return false;
    }

    void error(const QString& msg) {
        if (!m_error.isEmpty()) return;
        m_error = msg; m_errorLine = current().line; m_errorColumn = current().column;
    }

    static QString tokenTypeName(LidlToken::Type t) {
        switch (t) {
        case LidlToken::Module: return "'module'"; case LidlToken::TypeKw: return "'type'";
        case LidlToken::Method: return "'method'"; case LidlToken::Event: return "'event'";
        case LidlToken::Version: return "'version'"; case LidlToken::Description: return "'description'";
        case LidlToken::Category: return "'category'"; case LidlToken::Depends: return "'depends'";
        case LidlToken::Ident: return "identifier"; case LidlToken::StringLit: return "string literal";
        case LidlToken::LBrace: return "'{'"; case LidlToken::RBrace: return "'}'";
        case LidlToken::LParen: return "'('"; case LidlToken::RParen: return "')'";
        case LidlToken::LBracket: return "'['"; case LidlToken::RBracket: return "']'";
        case LidlToken::Colon: return "':'"; case LidlToken::Comma: return "','";
        case LidlToken::Arrow: return "'->'"; case LidlToken::Question: return "'?'";
        case LidlToken::Eof: return "end of input"; case LidlToken::Error: return "error";
        }
        return "unknown";
    }

    bool parseModule(ModuleDecl& mod) {
        if (!expect(LidlToken::Module, "module declaration")) return false;
        if (!at(LidlToken::Ident)) { error("Expected module name"); return false; }
        mod.name = current().text; ++m_pos;
        if (!expect(LidlToken::LBrace, "module declaration")) return false;
        if (!parseModuleBody(mod)) return false;
        if (!expect(LidlToken::RBrace, "module declaration")) return false;
        if (!at(LidlToken::Eof)) { error("Unexpected content after module closing '}'"); return false; }
        return true;
    }

    bool parseModuleBody(ModuleDecl& mod) {
        while (!at(LidlToken::RBrace) && !at(LidlToken::Eof)) {
            switch (current().type) {
            case LidlToken::Version: case LidlToken::Description: case LidlToken::Category: case LidlToken::Depends:
                if (!parseMetadata(mod)) return false; break;
            case LidlToken::TypeKw: if (!parseTypeDef(mod)) return false; break;
            case LidlToken::Method: if (!parseMethodDef(mod)) return false; break;
            case LidlToken::Event: if (!parseEventDef(mod)) return false; break;
            default: error(QString("Unexpected token '%1' in module body").arg(current().text)); return false;
            }
        }
        return true;
    }

    bool parseMetadata(ModuleDecl& mod) {
        if (at(LidlToken::Version)) { ++m_pos; if (!at(LidlToken::StringLit)) { error("Expected string after 'version'"); return false; } mod.version = current().text; ++m_pos; return true; }
        if (at(LidlToken::Description)) { ++m_pos; if (!at(LidlToken::StringLit)) { error("Expected string after 'description'"); return false; } mod.description = current().text; ++m_pos; return true; }
        if (at(LidlToken::Category)) { ++m_pos; if (!at(LidlToken::StringLit)) { error("Expected string after 'category'"); return false; } mod.category = current().text; ++m_pos; return true; }
        if (at(LidlToken::Depends)) {
            ++m_pos;
            if (!expect(LidlToken::LBracket, "depends list")) return false;
            if (!at(LidlToken::RBracket)) {
                if (!at(LidlToken::Ident)) { error("Expected identifier in depends list"); return false; }
                mod.depends.append(current().text); ++m_pos;
                while (consume(LidlToken::Comma)) {
                    if (!at(LidlToken::Ident)) { error("Expected identifier after ',' in depends list"); return false; }
                    mod.depends.append(current().text); ++m_pos;
                }
            }
            if (!expect(LidlToken::RBracket, "depends list")) return false;
            return true;
        }
        error("Expected metadata keyword"); return false;
    }

    bool parseTypeDef(ModuleDecl& mod) {
        if (!expect(LidlToken::TypeKw, "type definition")) return false;
        TypeDecl td;
        if (!at(LidlToken::Ident)) { error("Expected type name"); return false; }
        td.name = current().text; ++m_pos;
        if (!expect(LidlToken::LBrace, "type definition")) return false;
        while (!at(LidlToken::RBrace) && !at(LidlToken::Eof)) { FieldDecl fd; if (!parseFieldDef(fd)) return false; td.fields.append(fd); }
        if (!expect(LidlToken::RBrace, "type definition")) return false;
        mod.types.append(td); return true;
    }

    bool parseFieldDef(FieldDecl& fd) {
        fd.optional = consume(LidlToken::Question);
        if (!at(LidlToken::Ident)) { error("Expected field name"); return false; }
        fd.name = current().text; ++m_pos;
        if (!expect(LidlToken::Colon, "field definition")) return false;
        return parseTypeExpr(fd.type);
    }

    bool parseMethodDef(ModuleDecl& mod) {
        if (!expect(LidlToken::Method, "method definition")) return false;
        MethodDecl md;
        if (!at(LidlToken::Ident)) { error("Expected method name"); return false; }
        md.name = current().text; ++m_pos;
        if (!expect(LidlToken::LParen, "method parameters")) return false;
        if (!parseParams(md.params)) return false;
        if (!expect(LidlToken::RParen, "method parameters")) return false;
        if (!expect(LidlToken::Arrow, "method return type")) return false;
        if (!parseTypeExpr(md.returnType)) return false;
        mod.methods.append(md); return true;
    }

    bool parseEventDef(ModuleDecl& mod) {
        if (!expect(LidlToken::Event, "event definition")) return false;
        EventDecl ed;
        if (!at(LidlToken::Ident)) { error("Expected event name"); return false; }
        ed.name = current().text; ++m_pos;
        if (!expect(LidlToken::LParen, "event parameters")) return false;
        if (!parseParams(ed.params)) return false;
        if (!expect(LidlToken::RParen, "event parameters")) return false;
        mod.events.append(ed); return true;
    }

    bool parseParams(QVector<ParamDecl>& params) {
        if (at(LidlToken::RParen)) return true;
        ParamDecl p; if (!parseParam(p)) return false; params.append(p);
        while (consume(LidlToken::Comma)) { ParamDecl p2; if (!parseParam(p2)) return false; params.append(p2); }
        return true;
    }

    bool parseParam(ParamDecl& p) {
        if (!at(LidlToken::Ident)) { error("Expected parameter name"); return false; }
        p.name = current().text; ++m_pos;
        if (!expect(LidlToken::Colon, "parameter")) return false;
        return parseTypeExpr(p.type);
    }

    bool parseTypeExpr(TypeExpr& te) {
        if (consume(LidlToken::Question)) { te.kind = TypeExpr::Optional; te.elements.resize(1); return parseTypeExpr(te.elements[0]); }
        if (consume(LidlToken::LBracket)) { te.kind = TypeExpr::Array; te.elements.resize(1); if (!parseTypeExpr(te.elements[0])) return false; return expect(LidlToken::RBracket, "array type"); }
        if (consume(LidlToken::LBrace)) { te.kind = TypeExpr::Map; te.elements.resize(2); if (!parseTypeExpr(te.elements[0])) return false; if (!expect(LidlToken::Colon, "map type")) return false; if (!parseTypeExpr(te.elements[1])) return false; return expect(LidlToken::RBrace, "map type"); }
        if (at(LidlToken::Ident)) {
            const QString& name = current().text;
            te.kind = lidlBuiltinTypes().contains(name) ? TypeExpr::Primitive : TypeExpr::Named;
            te.name = name; ++m_pos; return true;
        }
        error(QString("Expected type expression, got '%1'").arg(current().text)); return false;
    }
};

LidlParseResult lidlParse(const QString& source) {
    LidlLexResult lexResult = lidlTokenize(source);
    if (lexResult.hasError()) { LidlParseResult pr; pr.error = lexResult.error; pr.errorLine = lexResult.errorLine; pr.errorColumn = lexResult.errorColumn; return pr; }
    Parser parser(lexResult.tokens);
    return parser.parse();
}
