#include <gtest/gtest.h>
#include "logos_instance.h"

class LogosInstanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Clear env so we get a fresh ID
        qunsetenv("LOGOS_INSTANCE_ID");
    }
    void TearDown() override
    {
        qunsetenv("LOGOS_INSTANCE_ID");
    }
};

TEST_F(LogosInstanceTest, IdReturnsNonEmpty)
{
    QString id = LogosInstance::id();
    EXPECT_FALSE(id.isEmpty());
}

TEST_F(LogosInstanceTest, IdIs12Chars)
{
    QString id = LogosInstance::id();
    EXPECT_EQ(id.length(), 12);
}

TEST_F(LogosInstanceTest, IdIsStableAcrossCalls)
{
    QString id1 = LogosInstance::id();
    QString id2 = LogosInstance::id();
    EXPECT_EQ(id1, id2);
}

TEST_F(LogosInstanceTest, IdSetsEnvVar)
{
    QString id = LogosInstance::id();
    QByteArray env = qgetenv("LOGOS_INSTANCE_ID");
    EXPECT_EQ(QString::fromUtf8(env), id);
}

TEST_F(LogosInstanceTest, IdRespectsExistingEnvVar)
{
    qputenv("LOGOS_INSTANCE_ID", "custom_id_123");
    QString id = LogosInstance::id();
    EXPECT_EQ(id, "custom_id_123");
}

TEST_F(LogosInstanceTest, RegistryUrlFormat)
{
    qputenv("LOGOS_INSTANCE_ID", "abc123def456");
    QString url = LogosInstance::id("my_module");
    EXPECT_EQ(url, "local:logos_my_module_abc123def456");
}

TEST_F(LogosInstanceTest, RegistryUrlTruncatesLongModuleNames)
{
    qputenv("LOGOS_INSTANCE_ID", "abc123def456");
    const QString longName =
        "liblogos_execution_zone_wallet_module_with_extra_suffix";
    QString url = LogosInstance::id(longName);

    ASSERT_TRUE(url.startsWith("local:"));
    const QString socketName = url.mid(QString("local:").size());

    EXPECT_LE(socketName.toUtf8().size(), 40);
    EXPECT_EQ(url, LogosInstance::id(longName));

    EXPECT_NE(url, LogosInstance::id(longName + "_different"));
}

// Golden test: pin the exact derived name so refactors don't silently change
// the socket naming scheme.
TEST_F(LogosInstanceTest, RegistryUrlGoldenLongName)
{
    qputenv("LOGOS_INSTANCE_ID", "abc123def456");
    const QString longName =
        "liblogos_execution_zone_wallet_module_with_extra_suffix";
    EXPECT_EQ(LogosInstance::id(longName),
              QStringLiteral("local:logos_libl_dc0d0a2280b70b17_abc123def456"));
}

TEST_F(LogosInstanceTest, RegistryUrlTruncatesLongInstanceId)
{
    const QByteArray longInstanceId =
        "instance_id_with_extra_suffix_that_is_deliberately_long_for_socket_names";
    const QString longName =
        "liblogos_execution_zone_wallet_module_with_extra_suffix";

    qputenv("LOGOS_INSTANCE_ID", longInstanceId);
    const QString url = LogosInstance::id(longName);

    ASSERT_TRUE(url.startsWith("local:"));
    const QString socketName = url.mid(QString("local:").size());
    EXPECT_LE(socketName.toUtf8().size(), 40);
    EXPECT_EQ(url, LogosInstance::id(longName));

    qputenv(
        "LOGOS_INSTANCE_ID",
        "instance_id_with_extra_suffix_that_is_deliberately_long_for_socket_names_changed");
    EXPECT_NE(url, LogosInstance::id(longName));
}

TEST_F(LogosInstanceTest, RegistryUrlTruncatesByUtf8Bytes)
{
    qputenv("LOGOS_INSTANCE_ID", "abc123def456");
    const QString unicodeLongName = QString::fromUtf8(
        "模块_пример_モジュール_الوحدة_가나다라마바사아자차카타파하");

    const QString url = LogosInstance::id(unicodeLongName);
    ASSERT_TRUE(url.startsWith("local:"));
    const QString socketName = url.mid(QString("local:").size());

    EXPECT_LE(socketName.toUtf8().size(), 40);
    EXPECT_EQ(url, LogosInstance::id(unicodeLongName));
}
