#include <gtest/gtest.h>
#include "lidl_lexer.h"

// ---------------------------------------------------------------------------
// Basic tokenization
// ---------------------------------------------------------------------------

TEST(LidlLexer, EmptyInput)
{
    auto r = lidlTokenize("");
    ASSERT_FALSE(r.hasError());
    ASSERT_EQ(r.tokens.size(), 1);
    EXPECT_EQ(r.tokens[0].type, LidlToken::Eof);
}

TEST(LidlLexer, WhitespaceOnly)
{
    auto r = lidlTokenize("   \t\n\n  ");
    ASSERT_FALSE(r.hasError());
    ASSERT_EQ(r.tokens.size(), 1);
    EXPECT_EQ(r.tokens[0].type, LidlToken::Eof);
}

TEST(LidlLexer, Comment)
{
    auto r = lidlTokenize("; this is a comment\n");
    ASSERT_FALSE(r.hasError());
    ASSERT_EQ(r.tokens.size(), 1);
    EXPECT_EQ(r.tokens[0].type, LidlToken::Eof);
}

TEST(LidlLexer, CommentBeforeToken)
{
    auto r = lidlTokenize("; comment\nmodule");
    ASSERT_FALSE(r.hasError());
    ASSERT_EQ(r.tokens.size(), 2);
    EXPECT_EQ(r.tokens[0].type, LidlToken::Module);
    EXPECT_EQ(r.tokens[1].type, LidlToken::Eof);
}

// ---------------------------------------------------------------------------
// Keywords
// ---------------------------------------------------------------------------

TEST(LidlLexer, AllKeywords)
{
    auto r = lidlTokenize("module type method event version description category depends");
    ASSERT_FALSE(r.hasError());
    ASSERT_EQ(r.tokens.size(), 9); // 8 keywords + Eof
    EXPECT_EQ(r.tokens[0].type, LidlToken::Module);
    EXPECT_EQ(r.tokens[1].type, LidlToken::TypeKw);
    EXPECT_EQ(r.tokens[2].type, LidlToken::Method);
    EXPECT_EQ(r.tokens[3].type, LidlToken::Event);
    EXPECT_EQ(r.tokens[4].type, LidlToken::Version);
    EXPECT_EQ(r.tokens[5].type, LidlToken::Description);
    EXPECT_EQ(r.tokens[6].type, LidlToken::Category);
    EXPECT_EQ(r.tokens[7].type, LidlToken::Depends);
}

TEST(LidlLexer, IdentifierNotKeyword)
{
    auto r = lidlTokenize("modules my_module foo123 _bar");
    ASSERT_FALSE(r.hasError());
    ASSERT_EQ(r.tokens.size(), 5);
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(r.tokens[i].type, LidlToken::Ident);
    EXPECT_EQ(r.tokens[0].text, "modules");
    EXPECT_EQ(r.tokens[1].text, "my_module");
    EXPECT_EQ(r.tokens[2].text, "foo123");
    EXPECT_EQ(r.tokens[3].text, "_bar");
}

// ---------------------------------------------------------------------------
// Symbols
// ---------------------------------------------------------------------------

TEST(LidlLexer, AllSymbols)
{
    auto r = lidlTokenize("{ } ( ) [ ] : , -> ?");
    ASSERT_FALSE(r.hasError());
    ASSERT_EQ(r.tokens.size(), 11); // 10 symbols + Eof
    EXPECT_EQ(r.tokens[0].type, LidlToken::LBrace);
    EXPECT_EQ(r.tokens[1].type, LidlToken::RBrace);
    EXPECT_EQ(r.tokens[2].type, LidlToken::LParen);
    EXPECT_EQ(r.tokens[3].type, LidlToken::RParen);
    EXPECT_EQ(r.tokens[4].type, LidlToken::LBracket);
    EXPECT_EQ(r.tokens[5].type, LidlToken::RBracket);
    EXPECT_EQ(r.tokens[6].type, LidlToken::Colon);
    EXPECT_EQ(r.tokens[7].type, LidlToken::Comma);
    EXPECT_EQ(r.tokens[8].type, LidlToken::Arrow);
    EXPECT_EQ(r.tokens[9].type, LidlToken::Question);
}

// ---------------------------------------------------------------------------
// String literals
// ---------------------------------------------------------------------------

TEST(LidlLexer, SimpleString)
{
    auto r = lidlTokenize("\"hello world\"");
    ASSERT_FALSE(r.hasError());
    ASSERT_EQ(r.tokens.size(), 2);
    EXPECT_EQ(r.tokens[0].type, LidlToken::StringLit);
    EXPECT_EQ(r.tokens[0].text, "hello world");
}

TEST(LidlLexer, StringWithEscapes)
{
    auto r = lidlTokenize("\"line1\\nline2\\ttab\\\\backslash\\\"quote\"");
    ASSERT_FALSE(r.hasError());
    ASSERT_EQ(r.tokens[0].type, LidlToken::StringLit);
    EXPECT_EQ(r.tokens[0].text, "line1\nline2\ttab\\backslash\"quote");
}

TEST(LidlLexer, EmptyString)
{
    auto r = lidlTokenize("\"\"");
    ASSERT_FALSE(r.hasError());
    EXPECT_EQ(r.tokens[0].type, LidlToken::StringLit);
    EXPECT_EQ(r.tokens[0].text, "");
}

TEST(LidlLexer, UnterminatedString)
{
    auto r = lidlTokenize("\"no closing");
    ASSERT_TRUE(r.hasError());
    EXPECT_TRUE(r.error.contains("Unterminated"));
}

TEST(LidlLexer, NewlineInString)
{
    auto r = lidlTokenize("\"broken\nstring\"");
    ASSERT_TRUE(r.hasError());
    EXPECT_TRUE(r.error.contains("Unterminated"));
}

// ---------------------------------------------------------------------------
// Line/column tracking
// ---------------------------------------------------------------------------

TEST(LidlLexer, LineColumnTracking)
{
    auto r = lidlTokenize("module test {\n  method foo() -> tstr\n}");
    ASSERT_FALSE(r.hasError());
    // "module" at line 1
    EXPECT_EQ(r.tokens[0].line, 1);
    EXPECT_EQ(r.tokens[0].column, 1);
    // "test" at line 1 col 8
    EXPECT_EQ(r.tokens[1].line, 1);
    EXPECT_EQ(r.tokens[1].column, 8);
    // "{" at line 1 col 13
    EXPECT_EQ(r.tokens[2].line, 1);
    EXPECT_EQ(r.tokens[2].column, 13);
    // "method" at line 2
    EXPECT_EQ(r.tokens[3].line, 2);
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

TEST(LidlLexer, UnexpectedCharacter)
{
    auto r = lidlTokenize("module @bad");
    ASSERT_TRUE(r.hasError());
    EXPECT_TRUE(r.error.contains("Unexpected character"));
}

// ---------------------------------------------------------------------------
// Full module tokenization
// ---------------------------------------------------------------------------

TEST(LidlLexer, FullModule)
{
    QString src = R"(
        module wallet_module {
            version "1.0.0"
            description "Wallet module"
            depends [dep_a, dep_b]
            method doSomething(input: tstr) -> bool
            event onUpdate(data: tstr)
        }
    )";
    auto r = lidlTokenize(src);
    ASSERT_FALSE(r.hasError());
    // Just verify no errors and reasonable token count
    EXPECT_GT(r.tokens.size(), 20);
    EXPECT_EQ(r.tokens.back().type, LidlToken::Eof);
}
