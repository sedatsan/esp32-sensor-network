#include <gtest/gtest.h>

#include <array>
#include <string>

#include "sn/app/gateway_settings.hpp"

using namespace sn::app;

TEST(UrlDecode, HandlesPlusAndPercentEscapes) {
    std::array<char, 32> buf{};
    const auto len = urlDecode("my+wifi%21%2F%c3%bc", buf);
    ASSERT_TRUE(len);
    EXPECT_EQ(std::string_view(buf.data(), *len), "my wifi!/\xc3\xbc");
}

TEST(UrlDecode, RejectsBrokenEscapes) {
    std::array<char, 32> buf{};
    for (const char* bad : {"%", "%4", "abc%", "%zz", "%4g"}) {
        const auto len = urlDecode(bad, buf);
        ASSERT_FALSE(len) << bad;
        EXPECT_EQ(len.error(), SettingsError::BadEncoding) << bad;
    }
}

TEST(UrlDecode, RejectsOverflow) {
    std::array<char, 3> buf{};
    EXPECT_TRUE(urlDecode("a%41c", buf));
    const auto len = urlDecode("abcd", buf);
    ASSERT_FALSE(len);
    EXPECT_EQ(len.error(), SettingsError::FieldTooLong);
}

TEST(SetupForm, ParsesAllFields) {
    const auto s = parseSetupForm(
        "ssid=Home+Net&wifi_pass=secret%26123&broker_uri=mqtts%3A%2F%2Fabc.hivemq.cloud%3A8883"
        "&broker_user=gw&broker_pass=p%40ss&submit=Save");
    ASSERT_TRUE(s);
    EXPECT_EQ(s->wifiSsid, "Home Net");
    EXPECT_EQ(s->wifiPassword, "secret&123");
    EXPECT_EQ(s->brokerUri, "mqtts://abc.hivemq.cloud:8883");
    EXPECT_EQ(s->brokerUser, "gw");
    EXPECT_EQ(s->brokerPassword, "p@ss");
    EXPECT_TRUE(usesTls(*s));
}

TEST(SetupForm, AllowsOpenNetworkAndPlainLocalBroker) {
    const auto s = parseSetupForm("ssid=cafe&wifi_pass=&broker_uri=mqtt%3A%2F%2F192.168.1.5");
    ASSERT_TRUE(s);
    EXPECT_TRUE(s->wifiPassword.empty());
    EXPECT_FALSE(usesTls(*s));
}

TEST(SetupForm, ReportsTheFirstProblem) {
    const auto expectError = [](const char* body, SettingsError expected) {
        const auto s = parseSetupForm(body);
        ASSERT_FALSE(s) << body;
        EXPECT_EQ(s.error(), expected) << body;
        EXPECT_FALSE(settingsErrorText(s.error()).empty());
    };
    expectError("wifi_pass=12345678&broker_uri=mqtts%3A%2F%2Fh", SettingsError::MissingSsid);
    expectError("ssid=x&wifi_pass=short&broker_uri=mqtts%3A%2F%2Fh",
                SettingsError::BadWifiPassword);
    expectError("ssid=x&broker_uri=http%3A%2F%2Fh", SettingsError::BadBrokerUri);
    expectError("ssid=x&broker_uri=mqtts%3A%2F%2F%3A8883", SettingsError::BadBrokerUri);
    expectError("ssid=x&broker_uri=mqtts%3A%2F%2Fh&broker_user=%G1", SettingsError::BadEncoding);
    expectError(("ssid=" + std::string(33, 'a') + "&broker_uri=mqtts%3A%2F%2Fh").c_str(),
                SettingsError::FieldTooLong);
}

TEST(SetupForm, AcceptsMaximumLengths) {
    const std::string body = "ssid=" + std::string(32, 's') + "&wifi_pass=" + std::string(63, 'p') +
                             "&broker_uri=mqtts%3A%2F%2Fh&broker_user=" + std::string(64, 'u');
    const auto s = parseSetupForm(body);
    ASSERT_TRUE(s);
    EXPECT_EQ(s->wifiSsid.size(), 32U);
    EXPECT_EQ(s->wifiPassword.size(), 63U);
    EXPECT_EQ(s->brokerUser.size(), 64U);
}
