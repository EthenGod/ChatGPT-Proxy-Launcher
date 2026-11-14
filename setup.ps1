# SPDX-License-Identifier: GPL-3.0-only
# Keep this entry point ASCII so Windows PowerShell can execute it from an HTTP response.
[CmdletBinding()]
param([switch]$Headless, [string]$Request, [string]$Result,
    [ValidateSet('deploy', 'plan', 'info')][string]$Operation = 'deploy')
& {
    $ErrorActionPreference = 'Stop'
    # BEGIN GENERATED LOCALES
    $setupLocales = ConvertFrom-Json -InputObject @'
{"zh-CN":{"download.activity":"\u51c6\u5907 ChatGPT \u914d\u7f6e\u5668","download.browsererror":"\u65e0\u6cd5\u6253\u5f00\u6d4f\u89c8\u5668\uff0c\u8bf7\u624b\u52a8\u6253\u5f00\uff1a{url}","download.checksum":"\u6b63\u5728\u4e0b\u8f7d\u6821\u9a8c\u6587\u4ef6","download.cleaned":"\u4e34\u65f6\u6587\u4ef6\u5df2\u6e05\u7406\u3002","download.close":"\u9000\u51fa\u8bf7\u76f4\u63a5\u5173\u95ed\u6b64\u7a97\u53e3\u3002","download.headless":"\u8fd9\u4e9b\u53c2\u6570\u9700\u8981\u540c\u65f6\u6307\u5b9a -Headless\u3002","download.incompletecleanup":"\u672a\u80fd\u6e05\u7406\u5168\u90e8\u4e34\u65f6\u4e0b\u8f7d\u6587\u4ef6\u3002","download.incompleteresult":"\u914d\u7f6e\u5668\u8fd4\u56de\u7684\u7ed3\u679c\u4e0d\u5b8c\u6574\u3002","download.inconsistentresult":"\u914d\u7f6e\u5668\u8fd4\u56de\u7684\u72b6\u6001\u4e0e\u9000\u51fa\u7801\u4e0d\u4e00\u81f4\u3002","download.inforequest":"info \u64cd\u4f5c\u4e0d\u63a5\u53d7 -Request\u3002","download.invalidchecksum":"\u914d\u7f6e\u5668\u7684\u6821\u9a8c\u503c\u7f3a\u5931\u6216\u65e0\u6548\uff0c\u672a\u542f\u52a8\u914d\u7f6e\u5668\u3002","download.invalidresult":"\u914d\u7f6e\u5668\u8fd4\u56de\u7684\u7ed3\u679c\u65e0\u6548\u3002","download.keepfolder":"\u4e34\u65f6\u76ee\u5f55\u5df2\u4fdd\u7559\uff1a{path}","download.latest":"\u6b63\u5728\u83b7\u53d6\u6700\u65b0\u7248\u672c","download.managedresult":"Result \u4e0d\u80fd\u4f4d\u4e8e\u672c\u5de5\u5177\u7ba1\u7406\u7684\u76ee\u5f55\u4e2d\u3002","download.mismatch":"SHA-256 \u6821\u9a8c\u4e0d\u4e00\u81f4\uff0c\u672a\u542f\u52a8\u914d\u7f6e\u5668\u3002","download.oldversion":"\u4e0b\u8f7d\u7684\u914d\u7f6e\u5668\u7248\u672c\u4e0d\u652f\u6301\u540e\u53f0\u90e8\u7f72\u3002","download.openhome":"\u6309\u4efb\u610f\u952e\u6253\u5f00\u9879\u76ee\u4e3b\u9875\u5e76\u9000\u51fa\uff1b10 \u79d2\u5185\u65e0\u64cd\u4f5c\u5c06\u4f1a\u81ea\u52a8\u6253\u5f00\u5e76\u9000\u51fa\u3002","download.redirected":"\u4e34\u65f6\u76ee\u5f55\u88ab\u91cd\u5b9a\u5411\uff0c\u5df2\u8df3\u8fc7\u6e05\u7406\uff1a{path}","download.releasetag":"\u7248\u672c\u6807\u8bb0\u683c\u5f0f\u5f02\u5e38\uff0c\u672a\u542f\u52a8\u914d\u7f6e\u5668\u3002","download.removefile":"\u65e0\u6cd5\u5220\u9664\u4e34\u65f6\u6587\u4ef6\uff1a{path}","download.requestsize":"Request \u6587\u4ef6\u4e0d\u5b58\u5728\u6216\u8d85\u8fc7\u5927\u5c0f\u9650\u5236\u3002","download.resultpath":"Result \u5fc5\u987b\u6307\u5411\u4e00\u4e2a\u5c1a\u4e0d\u5b58\u5728\u7684 .json \u6587\u4ef6\u3002","download.resultsize":"\u914d\u7f6e\u5668\u7684\u7ed3\u679c\u6587\u4ef6\u7f3a\u5931\u6216\u8d85\u8fc7\u5927\u5c0f\u9650\u5236\u3002","download.samepath":"Request \u548c Result \u4e0d\u80fd\u662f\u540c\u4e00\u4e2a\u6587\u4ef6\u3002","download.star":"\u559c\u6b22\u672c\u9879\u76ee\u8bf7\u7ed9\u672c\u9879\u76ee\u70b9\u4e00\u4e2a Star \u8c22\u8c22\u55b5~","download.transfer":"\u6b63\u5728\u4e0b\u8f7d\u914d\u7f6e\u5668","download.verify":"\u6b63\u5728\u6821\u9a8c\u6587\u4ef6","download.version":"\u6b63\u5728\u4e0b\u8f7d ChatGPT \u914d\u7f6e\u5668 {version}\u2026"},"en-US":{"download.activity":"Preparing ChatGPT Launcher Setup","download.browsererror":"Could not open the browser. Open this address manually: {url}","download.checksum":"Downloading checksums","download.cleaned":"Temporary files have been removed.","download.close":"To exit without opening it, close this window.","download.headless":"Parameters require -Headless.","download.incompletecleanup":"Temporary download files could not be fully removed.","download.incompleteresult":"The setup tool returned an incomplete result.","download.inconsistentresult":"The setup tool\u0027s result does not match its exit code.","download.inforequest":"info does not accept -Request.","download.invalidchecksum":"Missing or invalid setup checksum. Setup was not started.","download.invalidresult":"The setup tool returned an invalid result.","download.keepfolder":"Temporary folder retained: {path}","download.latest":"Getting the latest release","download.managedresult":"Result cannot be inside the managed directory.","download.mismatch":"SHA-256 mismatch. Setup was not started.","download.oldversion":"Downloaded setup does not support headless deployment.","download.openhome":"Press any key to open the project page and exit, or wait 10 seconds to open it automatically.","download.redirected":"The temporary folder was redirected; cleanup was skipped: {path}","download.releasetag":"Unexpected release tag. Setup was not started.","download.removefile":"Could not remove temporary file: {path}","download.requestsize":"The Request file is missing or exceeds the size limit.","download.resultpath":"Result must be a new .json file.","download.resultsize":"The setup result file is missing or exceeds the size limit.","download.samepath":"Request and Result must differ.","download.star":"If you like this project, please give it a Star. Thank you, meow~","download.transfer":"Downloading the setup tool","download.verify":"Verifying the download","download.version":"Downloading ChatGPT Launcher Setup {version}\u2026"},"ja-JP":{"download.activity":"ChatGPT \u8a2d\u5b9a\u30c4\u30fc\u30eb\u3092\u6e96\u5099\u4e2d","download.browsererror":"\u30d6\u30e9\u30a6\u30b6\u30fc\u3092\u958b\u3051\u307e\u305b\u3093\u3067\u3057\u305f\u3002\u6b21\u306e URL \u3092\u624b\u52d5\u3067\u958b\u3044\u3066\u304f\u3060\u3055\u3044\uff1a{url}","download.checksum":"\u30c1\u30a7\u30c3\u30af\u30b5\u30e0\u3092\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u4e2d","download.cleaned":"\u4e00\u6642\u30d5\u30a1\u30a4\u30eb\u3092\u524a\u9664\u3057\u307e\u3057\u305f\u3002","download.close":"\u30da\u30fc\u30b8\u3092\u958b\u304b\u305a\u306b\u7d42\u4e86\u3059\u308b\u5834\u5408\u306f\u3001\u3053\u306e\u30a6\u30a3\u30f3\u30c9\u30a6\u3092\u9589\u3058\u3066\u304f\u3060\u3055\u3044\u3002","download.headless":"\u3053\u308c\u3089\u306e\u30d1\u30e9\u30e1\u30fc\u30bf\u30fc\u306b\u306f -Headless \u304c\u5fc5\u8981\u3067\u3059\u3002","download.incompletecleanup":"\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u3057\u305f\u4e00\u6642\u30d5\u30a1\u30a4\u30eb\u3092\u3059\u3079\u3066\u524a\u9664\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.incompleteresult":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u304c\u8fd4\u3057\u305f\u7d50\u679c\u306b\u4e0d\u8db3\u304c\u3042\u308a\u307e\u3059\u3002","download.inconsistentresult":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306e\u72b6\u614b\u3068\u7d42\u4e86\u30b3\u30fc\u30c9\u304c\u4e00\u81f4\u3057\u307e\u305b\u3093\u3002","download.inforequest":"info \u64cd\u4f5c\u3067\u306f -Request \u3092\u6307\u5b9a\u3067\u304d\u307e\u305b\u3093\u3002","download.invalidchecksum":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306e\u30c1\u30a7\u30c3\u30af\u30b5\u30e0\u304c\u306a\u3044\u304b\u7121\u52b9\u306a\u305f\u3081\u3001\u8d77\u52d5\u3057\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.invalidresult":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u304c\u8fd4\u3057\u305f\u7d50\u679c\u306f\u7121\u52b9\u3067\u3059\u3002","download.keepfolder":"\u4e00\u6642\u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u6b8b\u3057\u307e\u3057\u305f\uff1a{path}","download.latest":"\u6700\u65b0\u30d0\u30fc\u30b8\u30e7\u30f3\u3092\u53d6\u5f97\u4e2d","download.managedresult":"Result \u3092\u672c\u30c4\u30fc\u30eb\u306e\u7ba1\u7406\u30d5\u30a9\u30eb\u30c0\u30fc\u5185\u306b\u4fdd\u5b58\u3059\u308b\u3053\u3068\u306f\u3067\u304d\u307e\u305b\u3093\u3002","download.mismatch":"SHA-256 \u304c\u4e00\u81f4\u3057\u306a\u3044\u305f\u3081\u3001\u8a2d\u5b9a\u30c4\u30fc\u30eb\u3092\u8d77\u52d5\u3057\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.oldversion":"\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u3057\u305f\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306f\u30d0\u30c3\u30af\u30b0\u30e9\u30a6\u30f3\u30c9\u3067\u306e\u914d\u7f6e\u306b\u5bfe\u5fdc\u3057\u3066\u3044\u307e\u305b\u3093\u3002","download.openhome":"\u4f55\u304b\u30ad\u30fc\u3092\u62bc\u3059\u3068\u30d7\u30ed\u30b8\u30a7\u30af\u30c8\u306e\u30da\u30fc\u30b8\u3092\u958b\u3044\u3066\u7d42\u4e86\u3057\u307e\u3059\u300210 \u79d2\u5f8c\u306b\u81ea\u52d5\u3067\u958b\u3044\u3066\u7d42\u4e86\u3057\u307e\u3059\u3002","download.redirected":"\u4e00\u6642\u30d5\u30a9\u30eb\u30c0\u30fc\u304c\u5225\u306e\u5834\u6240\u306b\u8ee2\u9001\u3055\u308c\u305f\u305f\u3081\u3001\u524a\u9664\u3092\u884c\u3044\u307e\u305b\u3093\u3067\u3057\u305f\uff1a{path}","download.releasetag":"\u30ea\u30ea\u30fc\u30b9\u30bf\u30b0\u306e\u5f62\u5f0f\u304c\u4e0d\u6b63\u306a\u305f\u3081\u3001\u8a2d\u5b9a\u30c4\u30fc\u30eb\u3092\u8d77\u52d5\u3057\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.removefile":"\u4e00\u6642\u30d5\u30a1\u30a4\u30eb\u3092\u524a\u9664\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\uff1a{path}","download.requestsize":"Request \u30d5\u30a1\u30a4\u30eb\u304c\u5b58\u5728\u3057\u306a\u3044\u304b\u3001\u30b5\u30a4\u30ba\u5236\u9650\u3092\u8d85\u3048\u3066\u3044\u307e\u3059\u3002","download.resultpath":"Result \u306b\u306f\u3001\u307e\u3060\u5b58\u5728\u3057\u306a\u3044 .json \u30d5\u30a1\u30a4\u30eb\u3092\u6307\u5b9a\u3057\u3066\u304f\u3060\u3055\u3044\u3002","download.resultsize":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306e\u7d50\u679c\u30d5\u30a1\u30a4\u30eb\u304c\u306a\u3044\u304b\u3001\u30b5\u30a4\u30ba\u5236\u9650\u3092\u8d85\u3048\u3066\u3044\u307e\u3059\u3002","download.samepath":"Request \u3068 Result \u306b\u306f\u5225\u306e\u30d5\u30a1\u30a4\u30eb\u3092\u6307\u5b9a\u3057\u3066\u304f\u3060\u3055\u3044\u3002","download.star":"\u3053\u306e\u30d7\u30ed\u30b8\u30a7\u30af\u30c8\u304c\u6c17\u306b\u5165\u3063\u305f\u3089\u3001Star \u3092\u304a\u9858\u3044\u3057\u307e\u3059\u3002\u3042\u308a\u304c\u3068\u3046\u306b\u3083\u3093\uff5e","download.transfer":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u3092\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u4e2d","download.verify":"\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u3057\u305f\u30d5\u30a1\u30a4\u30eb\u3092\u691c\u8a3c\u4e2d","download.version":"ChatGPT \u8a2d\u5b9a\u30c4\u30fc\u30eb {version} \u3092\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u4e2d\u2026"}}
'@
    # END GENERATED LOCALES
    function Get-SetupLanguage {
        try { $preference = Get-ItemPropertyValue -LiteralPath 'HKCU:\Software\Community\ChatGPTLauncherSetup' -Name Language -ErrorAction Stop } catch { $preference = '' }
        if ($preference -and $null -ne $setupLocales.PSObject.Properties[[string]$preference]) { return [string]$preference }
        try { $candidate = (Get-UICulture).Name } catch { $candidate = '' }
        if (!$candidate) { $candidate = [Globalization.CultureInfo]::InstalledUICulture.Name }
        if ($candidate) {
            if ($null -ne $setupLocales.PSObject.Properties[$candidate]) { return $candidate }
            $primary = $candidate.Split('-')[0]
            foreach ($tag in $setupLocales.PSObject.Properties.Name) { if ($tag.Split('-')[0] -ieq $primary) { return $tag } }
        }
        return 'zh-CN'
    }
    function Get-SetupText([string]$Key, [hashtable]$Values = @{}) {
        $pattern = $setupLocales.$setupLanguage.$Key
        # Replace once; braces in paths or other caller values stay literal.
        return [regex]::Replace($pattern, '\{([a-z][a-zA-Z0-9_]*)\}', [Text.RegularExpressions.MatchEvaluator]{
            param($match)
            $name = $match.Groups[1].Value
            if ($Values.ContainsKey($name)) { return [string]$Values[$name] }
            return $match.Value
        })
    }
    $setupLanguage = Get-SetupLanguage
    # Enable native transfer progress only in this script's scope.
    $ProgressPreference = if ($Headless) { 'SilentlyContinue' } else { 'Continue' }
    $progress = @{ Id = 1; Activity = (Get-SetupText 'download.activity') }
    function Invoke-SetupProgress([scriptblock]$Action) {
        if ($Headless) { & $Action; return }
        $savedView = $null
        # Minimal progress can count CJK characters as one cell and leave wrapped rows behind.
        if ($PSVersionTable.PSVersion -ge [version]'7.2') {
            $savedView = $PSStyle.Progress.View
            $PSStyle.Progress.View = 'Classic'
        }
        try { & $Action }
        finally {
            try { Write-Progress @progress -Completed }
            finally {
                if ($null -ne $savedView) { $PSStyle.Progress.View = $savedView }
            }
        }
    }
    function Show-SetupStage([string]$Status, [switch]$Completed) {
        if (!$Headless) {
            if ($Completed) { Write-Progress @progress -Completed }
            else { Write-Progress @progress -Status $Status }
        }
    }
    function Get-SetupLocalPath([string]$Path) {
        if ($Path -notmatch '\A[A-Za-z]:\\' -or $Path.IndexOfAny([char[]]'"<>|?*/') -ge 0 -or
            $Path -match '[\x00-\x1f]' -or $Path.Substring(2).Contains(':')) { throw 'invalid-local-path' }
        $full = [IO.Path]::GetFullPath($Path)
        foreach ($component in $full.Substring(3).Split('\')) {
            if ($component -match '[. ]$' -or $component -match '\A(?:CON|PRN|AUX|NUL|COM[1-9\u00b9\u00b2\u00b3]|LPT[1-9\u00b9\u00b2\u00b3])(?:\.|$)') { throw 'invalid-path-component' }
        }
        $part = $full
        while ($part) {
            if ([IO.File]::Exists($part) -or [IO.Directory]::Exists($part)) {
                if (([IO.File]::GetAttributes($part) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'redirected-path' }
            }
            $part = [IO.Path]::GetDirectoryName($part)
        }
        return $full
    }
    function Read-SetupKey {
        # Redirected input and non-console hosts may not support keyboard input.
        try {
            if (![Console]::IsInputRedirected -and [Console]::KeyAvailable) {
                return [Console]::ReadKey($true)
            }
        } catch { }
        return $null
    }
    function Show-SetupCompletion {
        Write-Host ''
        # The user may have changed language in the setup window.
        $setupLanguage = Get-SetupLanguage
        Write-Host ((Get-SetupText 'download.cleaned'))
        Write-Host ((Get-SetupText 'download.star')) -ForegroundColor Cyan
        $url = 'https://github.com/EthenGod/ChatGPT-Proxy-Launcher'
        Write-Host $url
        Write-Host ((Get-SetupText 'download.openhome'))
        Write-Host ((Get-SetupText 'download.close'))
        for ($tick = 0; $tick -lt 100; $tick++) {
            if ($null -ne (Read-SetupKey)) { break }
            Start-Sleep -Milliseconds 100
        }
        try { Start-Process -FilePath $url }
        catch { Write-Warning (Get-SetupText 'download.browsererror' @{url=$url}) }
    }
    $repo = 'EthenGod/ChatGPT-Proxy-Launcher'
    $folder = $null; $exe = $null; $sums = $null; $nativeResult = $null; $resultStream = $null
    $started = $false; $ownedFolder = $false; $cleanupComplete = $true; $phase = 'parse'; $tag = $null
    $outcome = [ordered]@{schemaVersion=1; operation=$Operation; status='error'; exitCode=20; code='invalid-request';
        committed=$false; stage='parse'; nativeError=0; warnings=@(); actionsRequired=@(); data=@{}}
    try {
        if ($Request -or $Result -or $Operation -ne 'deploy') {
            if (!$Headless) { throw (Get-SetupText 'download.headless') }
        }
        if ($Headless) {
            $Result = Get-SetupLocalPath $Result
            if ([IO.Path]::GetExtension($Result) -ine '.json') { throw (Get-SetupText 'download.resultpath') }
            $managedRoot = [IO.Path]::Combine([Environment]::GetFolderPath('LocalApplicationData'), 'ChatGPTProxyLauncher') + '\'
            if ($Result.StartsWith($managedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw (Get-SetupText 'download.managedresult') }
            if ($Operation -ne 'info') { $Request = Get-SetupLocalPath $Request }
            elseif ($Request) { throw (Get-SetupText 'download.inforequest') }
            if ($Request -and $Request -ieq $Result) { throw (Get-SetupText 'download.samepath') }
            $phase = 'result'
            $resultStream = [IO.File]::Open($Result, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            $phase = 'transport'
            if ($Request -and (![IO.File]::Exists($Request) -or (Get-Item -LiteralPath $Request).Length -gt 65536)) { throw (Get-SetupText 'download.requestsize') }
        }
        $tag = Invoke-SetupProgress {
            Show-SetupStage ((Get-SetupText 'download.latest'))
            $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -TimeoutSec 30
            $tag = [string]$release.tag_name
            if ($tag -cnotmatch '\Av[0-9]+(?:\.[0-9]+){2,3}\z') { throw (Get-SetupText 'download.releasetag') }
            return $tag
        }
        $base = "https://github.com/$repo/releases/download/$tag"
        $name = 'ChatGPTLauncherSetup.exe'
        $folder = Join-Path $env:TEMP ('ChatGPTLauncherSetup-' + [guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path $folder
        $ownedFolder = $true
        $sums = Join-Path $folder 'SHA256SUMS.txt'
        $exe = Join-Path $folder $name
        $nativeResult = Join-Path $folder 'native-result.json'
        if (!$Headless) { Write-Host (Get-SetupText 'download.version' @{version=$tag}) }
        $setupProcess = Invoke-SetupProgress {
            Show-SetupStage ((Get-SetupText 'download.checksum'))
            Invoke-WebRequest -UseBasicParsing -Uri "$base/SHA256SUMS.txt" -OutFile $sums -TimeoutSec 60
            $entries = @([IO.File]::ReadAllLines($sums) -cmatch '\s\*?ChatGPTLauncherSetup\.exe$')
            if ($entries.Count -ne 1 -or $entries[0] -cnotmatch '^([a-fA-F0-9]{64})[ \t]+\*?ChatGPTLauncherSetup\.exe$') {
                throw (Get-SetupText 'download.invalidchecksum')
            }
            $expected = $Matches[1]
            Show-SetupStage ((Get-SetupText 'download.transfer'))
            Invoke-WebRequest -UseBasicParsing -Uri "$base/$name" -OutFile $exe -TimeoutSec 120
            Show-SetupStage ((Get-SetupText 'download.verify'))
            $stream = [IO.File]::OpenRead($exe)
            try {
                $sha = [Security.Cryptography.SHA256]::Create()
                try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
                finally { $sha.Dispose() }
                if ($actual -ine $expected) { throw (Get-SetupText 'download.mismatch') }
                Show-SetupStage -Completed
                if ($Headless) {
                    $version = [version](Get-Item -LiteralPath $exe).VersionInfo.FileVersion
                    if ($version -lt [version]'1.1.0') { throw (Get-SetupText 'download.oldversion') }
                    $arguments = @('--headless', $Operation, '--result', ('"' + $nativeResult + '"'))
                    if ($Request) { $arguments += @('--request', ('"' + $Request + '"')) }
                    Start-Process -FilePath $exe -WorkingDirectory $folder -ArgumentList $arguments -WindowStyle Hidden -PassThru
                } else { Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru }
            } finally { $stream.Dispose() }
        }
        $started = $true
        try { $setupProcess.WaitForExit(); $nativeExit = $setupProcess.ExitCode } finally { $setupProcess.Dispose() }
        if ($Headless) {
            if (![IO.File]::Exists($nativeResult) -or (Get-Item -LiteralPath $nativeResult).Length -gt 1MB) { throw (Get-SetupText 'download.resultsize') }
            $utf8 = New-Object Text.UTF8Encoding($false, $true)
            $native = $utf8.GetString([IO.File]::ReadAllBytes($nativeResult)) | ConvertFrom-Json
            if ($native.schemaVersion -ne 1 -or $native.operation -cne $Operation -or $native.exitCode -ne $nativeExit -or
                $native.status -cnotin @('success','success-with-warnings','error') -or
                $null -eq $native.PSObject.Properties['committed'] -or !$native.code) { throw (Get-SetupText 'download.invalidresult') }
            if (($null -ne $native.committed -and $native.committed -isnot [bool]) -or
                $native.exitCode -notin @(0,10,20,21,30,31,40,41,50,70) -or
                ($native.exitCode -eq 0 -and $native.status -cne 'success') -or
                ($native.exitCode -eq 10 -and $native.status -cne 'success-with-warnings') -or
                ($native.exitCode -gt 10 -and $native.status -cne 'error')) { throw (Get-SetupText 'download.inconsistentresult') }
            foreach ($key in @('schemaVersion','operation','status','exitCode','code','committed','stage','nativeError','warnings','actionsRequired','data')) {
                if ($null -eq $native.PSObject.Properties[$key]) { throw (Get-SetupText 'download.incompleteresult') }
                $outcome[$key] = $native.$key
            }
        }
    } catch {
        if (!$Headless) { throw }
        $outcome.status = 'error'; $outcome.stage = $phase
        $outcome.exitCode = if ($started) { 72 } elseif ($phase -eq 'result') { 70 } elseif ($phase -eq 'parse') { 20 } else { 71 }
        $outcome.code = if ($started) { 'native-result-unavailable' } elseif ($phase -eq 'result') { 'result-unavailable' } elseif ($phase -eq 'parse') { 'invalid-request' } else { 'download-failed' }
        $outcome.committed = if ($started) { $null } else { $false }
    } finally {
        # Only the downloads and headless result are ours. Do not remove other contents.
        if ($ownedFolder -and $folder -and [IO.Directory]::Exists($folder)) {
            if (([IO.File]::GetAttributes($folder) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                $cleanupComplete = $false
                if (!$Headless) { Write-Warning (Get-SetupText 'download.redirected' @{path=$folder}) }
            } else {
                try { [IO.File]::Delete($exe) } catch { $cleanupComplete = $false; if (!$Headless) { Write-Warning (Get-SetupText 'download.removefile' @{path=$exe}) } }
                try { [IO.File]::Delete($sums) } catch { $cleanupComplete = $false; if (!$Headless) { Write-Warning (Get-SetupText 'download.removefile' @{path=$sums}) } }
                if ($Headless -and $nativeResult) { try { [IO.File]::Delete($nativeResult) } catch { $cleanupComplete = $false } }
                try { [IO.Directory]::Delete($folder, $false) } catch { $cleanupComplete = $false; if (!$Headless) { Write-Warning (Get-SetupText 'download.keepfolder' @{path=$folder}) } }
            }
        }
    }
    if ($Headless) {
        $outcome.transport = @{ releaseTag=$tag; cleanupComplete=$cleanupComplete }
        if (!$cleanupComplete) {
            $outcome.warnings = @($outcome.warnings) + @(@{code='cleanup-incomplete'; message=(Get-SetupText 'download.incompletecleanup')})
            if ($outcome.exitCode -in @(0,10)) { $outcome.exitCode=10; $outcome.status='success-with-warnings'; $outcome.code='cleanup-incomplete' }
        }
        if ($resultStream) {
            try {
                $utf8 = New-Object Text.UTF8Encoding($false)
                $bytes = $utf8.GetBytes(($outcome | ConvertTo-Json -Depth 20))
                $resultStream.Write($bytes, 0, $bytes.Length); $resultStream.Flush($true)
            } catch { $outcome.exitCode=70; $outcome.status='error'; $outcome.code='result-unavailable'; $outcome.stage='result' }
            finally { $resultStream.Dispose() }
        }
        return [pscustomobject]$outcome
    }
    if ($cleanupComplete) {
        Show-SetupCompletion
        # With the documented irm | iex command, exit only this PowerShell session.
        exit 0
    }
}
