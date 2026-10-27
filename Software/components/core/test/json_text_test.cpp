/**
 * @file    json_text_test.cpp
 * @brief   Host tests for JSON string unescaping at the API boundary.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "core/AlarmJson.hpp"
#include "core/JsonText.hpp"
#include "core/WebRadioJson.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(bool ok, std::string_view what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

void unescapesCommonEscapes()
{
    expect(core::unescapeJsonString(R"(https:\/\/a.it\/x.mp3)")
               == "https://a.it/x.mp3",
           "\\/ -> /");
    expect(core::unescapeJsonString(R"(a\"b\\c\nd)") == "a\"b\\c\nd",
           "quote, backslash, newline");
    expect(core::unescapeJsonString(R"(café)") == "caf\xC3\xA9",
           "\\u00e9 -> UTF-8");
    expect(core::unescapeJsonString(R"(🎵)") == "\xF0\x9F\x8E\xB5",
           "surrogate pair");
    expect(core::unescapeJsonString(R"(bad\q\u12)") == R"(bad\q\u12)",
           "malformed escapes kept");
}

void webRadioAcceptsSwiftEscapedUrl()
{
    // Swift's JSONEncoder output for the app's POST /api/streaming.
    const auto parsed = core::parseWebRadioConfigJson(
        R"({"enabled":true,"url":"https:\/\/icecast.unitedradio.it\/Rtl1025.mp3"})");
    expect(parsed && parsed->enabled
               && parsed->url == "https://icecast.unitedradio.it/Rtl1025.mp3",
           "escaped slashes accepted and decoded");
}

void alarmWebUrlIsDecoded()
{
    const auto table = core::parseAlarmTableJson(
        R"({"alarms":[{"action":"on","hour":7,"minute":0,"days":1,)"
        R"("source":"web","url":"http:\/\/x.it\/s.mp3"}]})");
    expect(table && (*table)[0].url == "http://x.it/s.mp3", "alarm url");
}

} // namespace

int main()
{
    unescapesCommonEscapes();
    webRadioAcceptsSwiftEscapedUrl();
    alarmWebUrlIsDecoded();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
