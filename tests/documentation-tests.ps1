# SPDX-License-Identifier: GPL-3.0-only
param([string]$ProjectRoot = '')
$ErrorActionPreference = 'Stop'
if (!$ProjectRoot) { $ProjectRoot = Split-Path $PSScriptRoot }
$ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
$suffixes = @('', '.en', '.ja')
$localeTags = @('zh-CN', 'en-US', 'ja-JP')
$groups = @('README', 'docs/translating', 'docs/taskbar-repair', 'docs/release-upload', 'assets/README')
$repository = 'https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/'
$releaseScript = [IO.File]::ReadAllText((Join-Path $ProjectRoot 'release.ps1'))
$count = 0
foreach ($group in $groups) {
    $baseName = ($group -split '/')[-1]
    $baseCode = $null
    $baseDownloads = $null
    foreach ($suffix in $suffixes) {
        $name = $group + $suffix + '.md'
        $path = Join-Path $ProjectRoot $name
        $text = [IO.File]::ReadAllText($path)
        if (!$releaseScript.Contains("'" + $name + "'")) { throw "Document missing from release sources: $name" }
        $firstLine = ($text -split '\r?\n', 2)[0]
        $navigation = @([regex]::Matches($firstLine, '\]\(([^)\r\n]+)\)'))
        if ($navigation.Count -ne $suffixes.Count) { throw "Missing language links: $name" }
        for ($i = 0; $i -lt $suffixes.Count; ++$i) {
            $expected = $baseName + $suffixes[$i] + '.md'
            if ($group -eq 'docs/release-upload') { $expected = $repository + $group + $suffixes[$i] + '.md' }
            if ($navigation[$i].Groups[1].Value -cne $expected) { throw "Incorrect language navigation: $name" }
        }
        foreach ($match in [regex]::Matches($text, '\]\(([^)\r\n]+)\)')) {
            $target = $match.Groups[1].Value
            if ($target -match '^(https?://|#|mailto:)') { continue }
            $relative = [Uri]::UnescapeDataString(($target -split '#', 2)[0])
            $resolved = [IO.Path]::GetFullPath((Join-Path (Split-Path $path) $relative))
            if (!$resolved.StartsWith($ProjectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
                !(Test-Path -LiteralPath $resolved -PathType Leaf)) { throw "Broken local link in ${name}: $target" }
        }
        # Commands must remain identical across translations, including quotes and arguments.
        $blocks = @([regex]::Matches($text, '(?ms)^[ \t]*```[^\r\n]*\r?\n(.*?)^[ \t]*```'))
        $code = @($blocks | ForEach-Object { ($_.Groups[1].Value -split '\r?\n' | ForEach-Object { $_.Trim() }) -join "`n" }) -join "`n---`n"
        if ($suffix -eq '') { $baseCode = $code }
        elseif ($code -cne $baseCode) { throw "Translated command example differs: $name" }
        if ($group -eq 'README') {
            $tag = $localeTags[[Array]::IndexOf($suffixes, $suffix)]
            $catalog = ConvertFrom-Json ([IO.File]::ReadAllText((Join-Path $ProjectRoot ('locales/' + $tag + '.json'))))
            foreach ($key in @('setup.window', 'setup.language.system', 'setup.option.startup', 'setup.option.desktop',
                'setup.option.proxy', 'setup.option.codex', 'setup.section.preview', 'setup.action.advanced',
                'setup.action.add', 'setup.action.openstartup', 'setup.action.deploy', 'setup.action.save')) {
                if (!$text.Contains($catalog.strings.$key)) { throw "README no longer matches UI caption ${key}: $name" }
            }
        }
        if ($group -eq 'docs/release-upload') {
            $downloads = @([regex]::Matches($text, 'https://github.com/[^)\s]+/releases/download/[^)\s]+') | ForEach-Object Value) -join "`n"
            if (!$downloads) { throw "Missing direct download: $name" }
            if ($suffix -eq '') { $baseDownloads = $downloads }
            elseif ($downloads -cne $baseDownloads) { throw "Release download versions differ: $name" }
        }
        ++$count
    }
}
Write-Host "PASS documentation: $count documents, language navigation, local links, UI captions, identical commands/downloads and source-package inclusion"
