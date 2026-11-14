# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param([switch]$Generate, [string]$ProjectRoot = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$ProjectRoot) { $ProjectRoot = Split-Path $PSScriptRoot }

function Read-CatalogJson([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -gt 1MB) { throw "Catalog is too large: $Path" }
    $utf8 = New-Object Text.UTF8Encoding($false, $true)
    $raw = $utf8.GetString($bytes).TrimStart([char]0xfeff)
    $value = ConvertFrom-Json -InputObject $raw
    # ConvertFrom-Json can silently accept duplicate properties. Check tokenized object keys too.
    $tokens = [regex]::Matches($raw, '"(?:[^"\\]|\\.)*"|[{}\[\]:,]')
    $objects = New-Object 'Collections.Generic.Stack[Collections.Generic.HashSet[string]]'
    for ($i = 0; $i -lt $tokens.Count; $i++) {
        $token = $tokens[$i].Value
        if ($token -eq '{') { $objects.Push((New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal))) }
        elseif ($token -eq '}') { $null = $objects.Pop() }
        elseif ($token.StartsWith('"') -and $i + 1 -lt $tokens.Count -and $tokens[$i + 1].Value -eq ':') {
            $key = ConvertFrom-Json -InputObject $token
            if (!$objects.Peek().Add($key)) { throw "Duplicate key '$key': $Path" }
        }
    }
    return $value
}
function Check-Fields($Value, [string[]]$Fields, [string]$Where) {
    foreach ($property in $Value.PSObject.Properties.Name) {
        if ($property -cnotin $Fields) { throw "Unknown field '$property': $Where" }
    }
    foreach ($field in $Fields) {
        if ($null -eq $Value.PSObject.Properties[$field]) { throw "Missing field '$field': $Where" }
    }
}
function Get-Placeholders([string]$Text) {
    $rest = [regex]::Replace($Text, '\{[a-z][a-zA-Z0-9_]*\}', '')
    if ($rest.Contains('{') -or $rest.Contains('}')) { throw "Invalid placeholder: $Text" }
    return @([regex]::Matches($Text, '\{([a-z][a-zA-Z0-9_]*)\}') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -CaseSensitive -Unique)
}
function Escape-Cpp([string]$Text) {
    return $Text.Replace('\', '\\').Replace('"', '\"').Replace("`r", '\r').Replace("`n", '\n').Replace("`t", '\t')
}
$directory = Join-Path $ProjectRoot 'locales'
$manifest = Read-CatalogJson (Join-Path $directory 'languages.json')
Check-Fields $manifest @('schemaVersion','languages') 'languages.json'
if ($manifest.schemaVersion -ne 1 -or $manifest.languages.Count -lt 1 -or $manifest.languages.Count -gt 32) { throw 'Unsupported language manifest' }
$catalogs = @(); $tags = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$strictUtf8 = New-Object Text.UTF8Encoding($false, $true)
foreach ($language in $manifest.languages) {
    Check-Fields $language @('tag','complete') 'language registration'
    if ($language.tag -isnot [string] -or $language.tag.Length -gt 84 -or $language.tag -cnotmatch '^[a-z]{2,3}(?:-[A-Za-z0-9]{2,8})*$' -or !$tags.Add($language.tag) -or $language.complete -isnot [bool]) { throw 'Invalid or duplicate language registration' }
    if ($language.tag -in @('zh-CN','en-US','ja-JP') -and !$language.complete) { throw 'Built-in languages must be complete' }
    $catalog = Read-CatalogJson (Join-Path $directory ($language.tag + '.json'))
    Check-Fields $catalog @('schemaVersion','locale','nativeName','fontFamily','strings') $language.tag
    if ($catalog.schemaVersion -ne 1 -or $catalog.locale -cne $language.tag) { throw 'Catalog identity mismatch' }
    foreach ($label in @($catalog.nativeName, $catalog.fontFamily)) {
        if ($label -isnot [string] -or !$label.Trim() -or $label.Length -gt 64 -or $label -match '[\x00-\x1f\x7f]') { throw 'Invalid language name or font' }
        $null = $strictUtf8.GetByteCount($label)
    }
    if ($catalog.fontFamily.Length -gt 31) { throw 'Font family must fit Windows LF_FACESIZE (31 characters)' }
    $catalogs += $catalog
}
foreach ($required in @('zh-CN','en-US','ja-JP')) { if (!$tags.Contains($required)) { throw "Required language is missing: $required" } }
$baseline = @($catalogs | Where-Object locale -CEQ 'zh-CN')
if ($baseline.Count -ne 1 -or $catalogs[0].locale -cne 'zh-CN') { throw 'Keep zh-CN first and include the zh-CN source catalog' }
$keys = [string[]]@($baseline[0].strings.PSObject.Properties.Name)
[Array]::Sort($keys, [StringComparer]::Ordinal)
if (!$keys.Count -or $keys.Count -gt 4096) { throw 'Invalid message count' }
foreach ($key in $keys) { if ($key -cnotmatch '^[a-z][a-z0-9]*(?:\.[a-z][a-z0-9]*)+$') { throw "Invalid message key: $key" } }
for ($index = 0; $index -lt $catalogs.Count; $index++) {
    $catalog = $catalogs[$index]; $missing = 0
    foreach ($property in $catalog.strings.PSObject.Properties) {
        if ($property.Name -cnotin $keys) { throw "Unknown message '$($property.Name)' in $($catalog.locale)" }
        if ($property.Value -isnot [string] -or $property.Value.Length -gt 8192 -or $property.Value -match '[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]') { throw "Invalid message '$($property.Name)'" }
        # JSON escapes can contain isolated UTF-16 surrogates even when the file is valid UTF-8.
        $null = $strictUtf8.GetByteCount($property.Value)
        $expected = @(Get-Placeholders $baseline[0].strings.($property.Name))
        $actual = @(Get-Placeholders $property.Value)
        if ($property.Value -and ($expected -join ',') -cne ($actual -join ',')) { throw "Placeholder mismatch: $($catalog.locale) / $($property.Name)" }
    }
    foreach ($key in $keys) { if ($null -eq $catalog.strings.PSObject.Properties[$key] -or !$catalog.strings.$key) { $missing++ } }
    if ($missing -and $manifest.languages[$index].complete) { throw "Incomplete required language: $($catalog.locale) ($missing missing)" }
    if ($missing) { Write-Warning "$($catalog.locale): $missing messages fall back to Chinese" }
}
# Check literals in production sources, including conditional keys and caption-binding tables.
$sourceDirectory = Join-Path $ProjectRoot 'src'
if (Test-Path -LiteralPath $sourceDirectory) {
    foreach ($source in Get-ChildItem -LiteralPath $sourceDirectory -File | Where-Object Extension -in '.h','.cpp') {
        $code = [IO.File]::ReadAllText($source.FullName)
        foreach ($match in [regex]::Matches($code, '(?<![A-Za-z0-9_])"((?:common|setup|help|error|proxy|startup|chain|program)\.[a-z0-9.]+)"')) {
            if ($match.Groups[1].Value -cnotin $keys) { throw "Unknown message key in $($source.Name): $($match.Groups[1].Value)" }
        }
    }
}
# Only downloader messages are embedded in the single-file PowerShell entry point.
# Unicode and apostrophes are escaped so the script stays ASCII and here-string safe.
$download = [ordered]@{}
foreach ($catalog in $catalogs) {
    $messages = [ordered]@{}
    foreach ($key in $keys) {
        if (!$key.StartsWith('download.')) { continue }
        $messages[$key] = if ($null -ne $catalog.strings.PSObject.Properties[$key] -and $catalog.strings.$key) { $catalog.strings.$key } else { $baseline[0].strings.$key }
    }
    $download[$catalog.locale] = $messages
}
$downloadJson = ConvertTo-Json -InputObject $download -Depth 5 -Compress
$downloadJson = [regex]::Replace($downloadJson, "[^\x20-\x7e]|'", [Text.RegularExpressions.MatchEvaluator]{ param($match) '\u{0:x4}' -f [int][char]$match.Value })
$generated = "    # BEGIN GENERATED LOCALES`n    `$setupLocales = ConvertFrom-Json -InputObject @'`n$downloadJson`n'@`n    # END GENERATED LOCALES"
$setupPath = Join-Path $ProjectRoot 'setup.ps1'
$setupText = [IO.File]::ReadAllText($setupPath).Replace("`r`n", "`n")
foreach ($match in [regex]::Matches($setupText, "Get-SetupText '(download\.[a-z0-9.]+)'")) {
    if ($match.Groups[1].Value -cnotin $keys) { throw "Unknown downloader message: $($match.Groups[1].Value)" }
}
$marker = '(?ms)^    # BEGIN GENERATED LOCALES.*?^    # END GENERATED LOCALES'
if ([regex]::Matches($setupText, $marker).Count -ne 1) { throw 'Expected exactly one generated localization block in setup.ps1' }
$expectedSetup = [regex]::Replace($setupText, $marker, [Text.RegularExpressions.MatchEvaluator]{ param($match) $generated })
if (!$Generate -and $expectedSetup -cne $setupText) { throw 'setup.ps1 translations are stale. Run tools/locales.ps1 -Generate.' }
if ($Generate) {
    $nativeKeys = @($keys | Where-Object { !$_.StartsWith('download.') })
    $lines = New-Object 'Collections.Generic.List[string]'
    $lines.Add('// Generated by tools/locales.ps1. Edit locales/*.json instead.')
    $lines.Add('#pragma once')
    $lines.Add('namespace l10n::data {')
    $lines.Add('struct Catalog { Language language; const wchar_t* const* text; };')
    $lines.Add('inline constexpr const char* Keys[] = {')
    foreach ($key in $nativeKeys) { $lines.Add('    "' + $key + '",') }
    $lines.Add('};')
    for ($index = 0; $index -lt $catalogs.Count; $index++) {
        $catalog = $catalogs[$index]
        $lines.Add('inline constexpr const wchar_t* Text' + $index + '[] = {')
        foreach ($key in $nativeKeys) {
            $value = if ($null -ne $catalog.strings.PSObject.Properties[$key] -and $catalog.strings.$key) { $catalog.strings.$key } else { $baseline[0].strings.$key }
            $lines.Add('    L"' + (Escape-Cpp $value) + '",')
        }
        $lines.Add('};')
    }
    $lines.Add('inline constexpr Catalog Languages[] = {')
    for ($index = 0; $index -lt $catalogs.Count; $index++) {
        $catalog = $catalogs[$index]
        $lines.Add('    {{L"' + $catalog.locale + '", L"' + (Escape-Cpp $catalog.nativeName) + '", L"' + (Escape-Cpp $catalog.fontFamily) + '"}, Text' + $index + '},')
        if ($catalog.locale -ceq 'zh-CN') { $baselineIndex = $index }
    }
    $lines.Add('};')
    $lines.Add('inline constexpr size_t Fallback = ' + $baselineIndex + ';')
    $lines.Add('}')
    $work = Join-Path $ProjectRoot 'work'
    $null = [IO.Directory]::CreateDirectory($work)
    [IO.File]::WriteAllLines((Join-Path $work 'LocalizationData.h'), $lines, (New-Object Text.UTF8Encoding($false)))
    if ($expectedSetup -cne $setupText) { [IO.File]::WriteAllText($setupPath, $expectedSetup.Replace("`n", "`r`n"), (New-Object Text.UTF8Encoding($false))) }
}
Write-Output "PASS localization: $($catalogs.Count) languages, $($keys.Count) messages; keys and placeholders validated"
