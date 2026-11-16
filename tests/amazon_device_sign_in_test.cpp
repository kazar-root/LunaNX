// Offline tests of the production Amazon device flow. Every HTTP request is mocked.
#include "luna/amazon_auth.hpp"
#include "luna/device_code_policy.hpp"
#include "core/http_client.hpp"
#include "core/i18n.hpp"

#include <jansson.h>

#include <cassert>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
using namespace lunanx;
std::deque<HttpResponse> replies;
std::string identity = "fr", store = "fr";
bool cancelled = false, cancel_on_registration = false;
int creates = 0, polls = 0, deregisters = 0;
std::string serial;

HttpResponse Pair(const std::string& lifetime = "30")
{
    return {200, "{\"public_code\":\"ABCDE\",\"private_code\":\"test-private\",\"expires_in\":" + lifetime +
                 ",\"polling_interval_in_seconds\":1}", {}};
}
HttpResponse Success()
{
    return {200, R"({"response":{"success":{"tokens":{"bearer":{"refresh_token":"test-refresh"},"website_cookies":[{"Name":"session-id","Value":"test-cookie","Path":"/"}]},"extensions":{"customer_info":{"given_name":"Test"}}}}})", {}};
}
void Reset()
{
    replies.clear();
    identity = store = "fr";
    cancelled = cancel_on_registration = false;
    creates = polls = deregisters = 0;
    serial.clear();
}
template<class F> void MustFail(F&& call)
{
    bool failed = false;
    try { call(); } catch (const std::runtime_error&) { failed = true; }
    assert(failed);
}
auto IsCancelled = [] { return cancelled; };
void Challenge(const lunanx::luna::DeviceCodeChallenge& challenge)
{
    assert(challenge.user_code == "ABCDE");
    assert(challenge.verification_url == "https://www.amazon." + identity + "/code");
    assert(challenge.expires_in_seconds > 0);
}
}

namespace lunanx
{
const std::string& AppHomePath()
{
    static const std::string unused = "/tmp/lunanx-auth-test-unused";
    return unused;
}
void LogRuntimeEvent(const std::string&, const std::string&, const std::string& detail)
{
    assert(detail.find("test-private") == std::string::npos);
    assert(detail.find("test-refresh") == std::string::npos);
    assert(detail.find("test-cookie") == std::string::npos);
}
HttpResponse HttpClient::Request(const std::string& method, const std::string& url,
    const std::string&, const std::vector<std::string>&, const std::string& body, long, long) const
{
    assert(method == "POST");
    assert(url.starts_with("https://api.amazon." + identity + "/auth/"));
    if (url.ends_with("/auth/token"))
        return {200, R"({"access_token":"test-access"})", {}};
    if (url.ends_with("/auth/deregister"))
    {
        ++deregisters;
        return {200, "{}", {}};
    }
    json_t* parsed = json_loads(body.c_str(), 0, nullptr);
    assert(parsed);
    if (url.ends_with("/create/codepair"))
    {
        ++creates;
        json_t* data = json_object_get(parsed, "code_data");
        serial = json_string_value(json_object_get(data, "device_serial"));
        assert(serial.size() == 40);
        assert(std::string(json_string_value(json_object_get(data, "device_type"))) == "A2CZJZGLK2JJVM");
    }
    else
    {
        assert(url.ends_with("/auth/register"));
        ++polls;
        auto* auth = json_object_get(parsed, "auth_data");
        auto* pair = json_object_get(auth, "code_pair");
        assert(std::string(json_string_value(json_object_get(pair, "private_code"))) == "test-private");
        assert(std::string(json_string_value(json_object_get(json_object_get(parsed, "registration_data"),
                                                            "device_serial"))) == serial);
        assert(std::string(json_string_value(json_object_get(json_object_get(parsed, "cookies"), "domain"))) ==
               ".amazon." + store);
        if (cancel_on_registration)
            cancelled = true;
    }
    json_decref(parsed);
    assert(!replies.empty());
    auto response = replies.front();
    replies.pop_front();
    if (response.status_code == -1)
        throw std::runtime_error("simulated network failure");
    return response;
}
HttpResponse HttpClient::Post(const std::string& url, const std::string& agent,
    const std::vector<std::string>& headers, const std::string& body) const
{
    return Request("POST", url, agent, headers, body);
}
}

int main()
{
    using namespace lunanx::luna;
    lunanx::SetUiLanguage("en");
    using Result = DeviceCodePollResult;
    assert(ClassifyDeviceCodePoll(401, "Unauthorized", false) == Result::Pending);
    assert(ClassifyDeviceCodePoll(401, "InvalidCode", false) == Result::Rejected);
    assert(ClassifyDeviceCodePoll(401, "", false) == Result::Rejected);
    assert(ClassifyDeviceCodePoll(200, "", false) == Result::Rejected);
    assert(ClassifyDeviceCodePoll(429, "", false) == Result::Retry);
    assert(ClassifyDeviceCodePoll(503, "", false) == Result::Retry);
    assert(ClassifyDeviceCodePoll(403, "Unauthorized", false) == Result::Rejected);

    Reset(); cancelled = true;
    MustFail([] { AmazonAuth::SignInWithCode("fr", Challenge, IsCancelled); });
    assert(creates == 0);

    Reset(); replies = {{200, "{}", {}}};
    MustFail([] { AmazonAuth::SignInWithCode("fr", Challenge, IsCancelled); });
    assert(polls == 0);

    Reset(); replies = {Pair("-1")};
    MustFail([] { AmazonAuth::SignInWithCode("fr", Challenge, IsCancelled); });
    assert(polls == 0);

    Reset(); replies = {Pair()};
    MustFail([] { AmazonAuth::SignInWithCode("fr", [](const auto&) { cancelled = true; }, IsCancelled); });
    assert(polls == 0);

    Reset(); replies = {Pair("1")};
    MustFail([] { AmazonAuth::SignInWithCode("fr", Challenge, IsCancelled); });
    assert(polls == 0);

    Reset(); replies = {Pair(), {401, R"({"response":{"error":{"code":"InvalidCode"}}})", {}}};
    MustFail([] { AmazonAuth::SignInWithCode("fr", Challenge, IsCancelled); });
    assert(polls == 1);

    Reset(); identity = "de"; store = "nl";
    replies = {Pair(), {401, R"({"response":{"error":{"code":"Unauthorized"}}})", {}}, Success()};
    auto registration = AmazonAuth::SignInWithCode("nl", Challenge, IsCancelled);
    assert(registration.market_code == "nl");
    assert(registration.cookies.front().domain == ".amazon.nl");
    assert(registration.customer_name == "Test");
    assert(registration.refresh_token == "test-refresh");
    assert(polls == 2 && replies.empty());

    Reset(); replies = {Pair(), {-1, "", {}}, {503, "unavailable", {}}, Success()};
    registration = AmazonAuth::SignInWithCode("fr", Challenge, IsCancelled);
    assert(polls == 3 && replies.empty());

    Reset(); cancel_on_registration = true; replies = {Pair(), Success()};
    MustFail([] { AmazonAuth::SignInWithCode("fr", Challenge, IsCancelled); });
    assert(deregisters == 1 && replies.empty());

    std::cout << "Amazon device sign-in: 9 offline scenarios passed\n";
}
