// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string>
#include "Localization.h"

namespace setup {
struct ProxyDetection {
    std::wstring port;
    std::wstring message = l10n::Text("proxy.notfound");
    std::wstring source;
};
// Read-only, local discovery. No network requests or changes to proxy settings.
ProxyDetection DetectProxyPort(const std::wstring& protocol, const std::wstring& host);
}
