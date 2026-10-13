// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string>

namespace setup {
struct ProxyDetection {
    std::wstring port;
    std::wstring message = L"未能确认代理端口。请打开代理工具的「端口设置」，查看混合或对应协议端口后手动填写。";
};
// Read-only, local discovery. No network requests or changes to proxy settings.
ProxyDetection DetectProxyPort(const std::wstring& protocol, const std::wstring& host);
}
