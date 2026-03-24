#include <gtest/gtest.h>
#include <QtTest/QSignalSpy>
#include "token_manager.h"

class TokenManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        TokenManager::instance().clearAllTokens();
    }
};

TEST_F(TokenManagerTest, SaveAndGetToken)
{
    TokenManager::instance().saveToken("mod_a", "token_a");
    EXPECT_EQ(TokenManager::instance().getToken("mod_a"), "token_a");
}

TEST_F(TokenManagerTest, GetMissingTokenReturnsEmpty)
{
    EXPECT_TRUE(TokenManager::instance().getToken("nonexistent").isEmpty());
}

TEST_F(TokenManagerTest, HasToken)
{
    EXPECT_FALSE(TokenManager::instance().hasToken("key"));
    TokenManager::instance().saveToken("key", "val");
    EXPECT_TRUE(TokenManager::instance().hasToken("key"));
}

TEST_F(TokenManagerTest, RemoveToken)
{
    TokenManager::instance().saveToken("key", "val");
    EXPECT_TRUE(TokenManager::instance().removeToken("key"));
    EXPECT_FALSE(TokenManager::instance().hasToken("key"));
}

TEST_F(TokenManagerTest, RemoveNonexistentTokenReturnsFalse)
{
    EXPECT_FALSE(TokenManager::instance().removeToken("missing"));
}

TEST_F(TokenManagerTest, ClearAllTokens)
{
    TokenManager::instance().saveToken("a", "1");
    TokenManager::instance().saveToken("b", "2");
    TokenManager::instance().clearAllTokens();
    EXPECT_EQ(TokenManager::instance().tokenCount(), 0);
}

TEST_F(TokenManagerTest, TokenCount)
{
    EXPECT_EQ(TokenManager::instance().tokenCount(), 0);
    TokenManager::instance().saveToken("x", "1");
    EXPECT_EQ(TokenManager::instance().tokenCount(), 1);
    TokenManager::instance().saveToken("y", "2");
    EXPECT_EQ(TokenManager::instance().tokenCount(), 2);
}

TEST_F(TokenManagerTest, GetTokenKeys)
{
    TokenManager::instance().saveToken("alpha", "1");
    TokenManager::instance().saveToken("beta", "2");
    QList<QString> keys = TokenManager::instance().getTokenKeys();
    EXPECT_EQ(keys.size(), 2);
    EXPECT_TRUE(keys.contains("alpha"));
    EXPECT_TRUE(keys.contains("beta"));
}

TEST_F(TokenManagerTest, OverwriteToken)
{
    TokenManager::instance().saveToken("key", "old");
    TokenManager::instance().saveToken("key", "new");
    EXPECT_EQ(TokenManager::instance().getToken("key"), "new");
    EXPECT_EQ(TokenManager::instance().tokenCount(), 1);
}

TEST_F(TokenManagerTest, SignalTokenSaved)
{
    QSignalSpy spy(&TokenManager::instance(), &TokenManager::tokenSaved);
    TokenManager::instance().saveToken("k", "v");
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "k");
}

TEST_F(TokenManagerTest, SignalTokenRemoved)
{
    TokenManager::instance().saveToken("k", "v");
    QSignalSpy spy(&TokenManager::instance(), &TokenManager::tokenRemoved);
    TokenManager::instance().removeToken("k");
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "k");
}

TEST_F(TokenManagerTest, SignalAllTokensCleared)
{
    TokenManager::instance().saveToken("k", "v");
    QSignalSpy spy(&TokenManager::instance(), &TokenManager::allTokensCleared);
    TokenManager::instance().clearAllTokens();
    EXPECT_EQ(spy.count(), 1);
}
