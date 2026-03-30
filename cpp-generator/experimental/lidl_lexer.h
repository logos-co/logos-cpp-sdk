#ifndef LIDL_LEXER_H
#define LIDL_LEXER_H

#include <QString>
#include <QVector>

struct LidlToken {
    enum Type {
        // Keywords
        Module, TypeKw, Method, Event,
        Version, Description, Category, Depends,
        // Literals
        Ident, StringLit,
        // Symbols
        LBrace, RBrace, LParen, RParen, LBracket, RBracket,
        Colon, Comma, Arrow, Question,
        // Special
        Eof, Error
    };

    Type type = Eof;
    QString text;
    int line = 0;
    int column = 0;
};

struct LidlLexResult {
    QVector<LidlToken> tokens;
    QString error;
    int errorLine = 0;
    int errorColumn = 0;
    bool hasError() const { return !error.isEmpty(); }
};

LidlLexResult lidlTokenize(const QString& source);

#endif // LIDL_LEXER_H
