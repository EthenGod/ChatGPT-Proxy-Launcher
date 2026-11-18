// SPDX-License-Identifier: GPL-3.0-only
#include "SetupProgramIcons.h"
#include "AutomationIO.h"
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <wincrypt.h>
#include <wrl/client.h>
#include <memory>

namespace setup::ui {
namespace {
using automation::Json;
using Microsoft::WRL::ComPtr;
constexpr UINT IconSize = 48;
constexpr DWORD MaxPngBytes = 16 * 1024;
struct IconDeleter { void operator()(HICON icon) const noexcept { DestroyIcon(icon); } };
using OwnedIcon = std::unique_ptr<HICON__, IconDeleter>;

std::wstring EncodeIcon(IWICImagingFactory* factory, HICON icon) {
    ComPtr<IWICBitmap> bitmap;
    if (FAILED(factory->CreateBitmapFromHICON(icon, &bitmap))) return {};
    UINT width = 0, height = 0;
    if (FAILED(bitmap->GetSize(&width, &height)) || !width || !height) return {};
    ComPtr<IWICBitmapScaler> scaler;
    IWICBitmapSource* source = bitmap.Get();
    if (width > IconSize || height > IconSize) {
        if (FAILED(factory->CreateBitmapScaler(&scaler)) ||
            FAILED(scaler->Initialize(source, IconSize, IconSize, WICBitmapInterpolationModeFant))) return {};
        source = scaler.Get();
    }
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(nullptr, 0));
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    if (!stream || FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
        FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
        FAILED(encoder->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr)) ||
        FAILED(frame->WriteSource(source, nullptr)) || FAILED(frame->Commit()) || FAILED(encoder->Commit())) return {};
    STATSTG info{};
    if (FAILED(stream->Stat(&info, STATFLAG_NONAME)) || !info.cbSize.QuadPart || info.cbSize.QuadPart > MaxPngBytes ||
        FAILED(stream->Seek({}, STREAM_SEEK_SET, nullptr))) return {};
    std::vector<BYTE> bytes(info.cbSize.LowPart);
    DWORD read = 0;
    if (FAILED(stream->Read(bytes.data(), info.cbSize.LowPart, &read)) || read != info.cbSize.LowPart) return {};
    DWORD chars = 0;
    constexpr DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
    if (!CryptBinaryToStringW(bytes.data(), read, flags, nullptr, &chars)) return {};
    std::wstring encoded(chars, L'\0');
    if (!CryptBinaryToStringW(bytes.data(), read, flags, encoded.data(), &chars)) return {};
    encoded.resize(chars);
    return encoded;
}

std::wstring LocalResource(const std::wstring& input, const std::wstring& item, const std::wstring& isolatedRoot) {
    if (input.empty()) return {};
    std::wstring path = input;
    if (path.find(L'%') != path.npos) {
        const DWORD n = ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
        if (!n || n > 32768) return {};
        path.resize(n);
        if (ExpandEnvironmentStringsW(input.c_str(), path.data(), n) != n) return {};
        path.pop_back();
    }
    if (PathIsRelativeW(path.c_str())) path = Join(automation::ParentPath(item), path);
    try {
        path = automation::FullPath(path);
        if (!isolatedRoot.empty() && (path.size() <= isolatedRoot.size() || path[isolatedRoot.size()] != L'\\' ||
            CompareStringOrdinal(path.data(), static_cast<int>(isolatedRoot.size()), isolatedRoot.data(),
                static_cast<int>(isolatedRoot.size()), TRUE) != CSTR_EQUAL)) return {};
        const auto drive = GetDriveTypeW(path.substr(0, 3).c_str());
        if (drive != DRIVE_FIXED && drive != DRIVE_REMOVABLE && drive != DRIVE_RAMDISK && drive != DRIVE_CDROM) return {};
        automation::CheckPlainPath(path);
    } catch (const automation::Invalid&) { return {}; }
      catch (const automation::IoError&) { return {}; }
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_OFFLINE))) return {};
    return path;
}

struct IconTable {
    IWICImagingFactory* factory;
    size_t budget;
    Json images = Json::Array();
    // An extension alone is a type icon key. Resource keys always contain a full path.
    std::map<std::pair<std::wstring, int>, int> indices{};

    int Lookup(const std::wstring& key, int index, bool type) {
        if (key.empty()) return -1;
        auto [slot, inserted] = indices.try_emplace({key, index}, -1);
        if (!inserted) return slot->second;
        HICON handle = nullptr;
        if (type) {
            SHFILEINFOW info{};
            const auto name = L"file" + key;
            SHGetFileInfoW(name.c_str(), FILE_ATTRIBUTE_NORMAL, &info, sizeof(info),
                SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_LARGEICON);
            handle = info.hIcon;
        } else {
            SHDefExtractIconW(key.c_str(), index, 0, &handle, nullptr, IconSize);
        }
        OwnedIcon icon(handle);
        if (!icon) return -1;
        const auto png = EncodeIcon(factory, icon.get());
        if (png.empty() || png.size() + 3 > budget) return -1;
        budget -= png.size() + 3; // quotes and comma
        slot->second = static_cast<int>(images.array.size());
        images.array.emplace_back(png);
        return slot->second;
    }
};
}

automation::Json ProgramIcons(Json& rows, const std::vector<ProgramIconSource>& sources,
    size_t remainingBytes, const std::wstring& isolatedRoot) {
    ComPtr<IWICImagingFactory> factory;
    if (rows.array.empty() || FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)))) return Json::Array();
    IconTable table{factory.Get(), remainingBytes};
    // Names and statuses have first claim on the bridge budget. Optional artwork never evicts a row.
    constexpr size_t indexBytes = 16; // ,"icon": plus the bounded row index
    for (size_t i = 0; i < rows.array.size() && table.budget >= indexBytes; ++i) {
        auto& row = rows.array[i];
        const auto& item = row.At(L"path").text;
        const auto& source = sources[i];
        const auto& target = source.target.empty() ? item : source.target;
        table.budget -= indexBytes;
        const auto custom = LocalResource(source.shortcut.path, item, isolatedRoot);
        int index = table.Lookup(custom, source.shortcut.index, false);
        PCWSTR extension = PathFindExtensionW(target.c_str());
        if (index < 0 && !_wcsicmp(extension, L".exe")) {
            index = table.Lookup(LocalResource(target, item, isolatedRoot), 0, false);
        }
        if (index < 0) {
            std::wstring type = extension;
            if (!type.empty() && type.size() <= 12) {
                CharLowerBuffW(type.data(), static_cast<DWORD>(type.size()));
                index = table.Lookup(type, 0, true);
            }
        }
        if (index >= 0) row[L"icon"] = Json::Number(index);
        else table.budget += indexBytes;
    }
    return std::move(table.images);
}
}
