# SPDX-License-Identifier: GPL-3.0-only
param([string]$ProjectRoot = '')
$ErrorActionPreference = 'Stop'
if (!$ProjectRoot) { $ProjectRoot = Split-Path $PSScriptRoot }
$tool = Join-Path $ProjectRoot 'tools\locales.ps1'
& $tool
$fixture = Join-Path $ProjectRoot ('work\locales-tests-' + [guid]::NewGuid().ToString('N'))
$directory = Join-Path $fixture 'locales'
$null = [IO.Directory]::CreateDirectory($directory)
$manifestPath = Join-Path $ProjectRoot 'locales\languages.json'
$manifestText = [IO.File]::ReadAllText($manifestPath)
$manifest = ConvertFrom-Json $manifestText
$utf8 = New-Object Text.UTF8Encoding($false)
foreach ($item in $manifest.languages) {
    [IO.File]::Copy((Join-Path $ProjectRoot ('locales\' + $item.tag + '.json')), (Join-Path $directory ($item.tag + '.json')))
}
[IO.File]::WriteAllText((Join-Path $directory 'languages.json'), $manifestText, $utf8)
[IO.File]::Copy((Join-Path $ProjectRoot 'setup.ps1'), (Join-Path $fixture 'setup.ps1'))
$englishPath = Join-Path $directory 'en-US.json'
$english = [IO.File]::ReadAllText($englishPath)
function Reject-Catalog([string]$Text, [string]$Expected) {
    [IO.File]::WriteAllText($englishPath, $Text, $utf8)
    $caught = $false
    try { & $tool -ProjectRoot $fixture | Out-Null }
    catch { if ($_.Exception.Message -notlike ('*' + $Expected + '*')) { throw }; $caught = $true }
    finally { [IO.File]::WriteAllText($englishPath, $english, $utf8) }
    if (!$caught) { throw "Invalid catalog accepted: $Expected" }
}
Reject-Catalog ($english.Replace('"common.close": "Close",', '"common.close": "Close", "common.close": "Again",')) 'Duplicate'
Reject-Catalog ($english.Replace('Setup v{version}', 'Setup v{wrong}')) 'Placeholder'
Reject-Catalog ($english.Replace('"common.close": "Close",', '')) 'Incomplete'
Reject-Catalog ($english.Replace('"common.close": "Close",', '"unknown.message": "Close",')) 'Unknown message'
Reject-Catalog ($english.Replace('Setup v{version}', 'Setup {')) 'Invalid placeholder'
Reject-Catalog ($english.Replace('"locale": "en-US"', '"locale": "en-GB"')) 'identity mismatch'

# Community translations may be partial. Generated native and PowerShell tables must use Chinese fallback.
$manifest.languages += [pscustomobject]@{tag='fr-FR'; complete=$false}
[IO.File]::WriteAllText((Join-Path $directory 'languages.json'), ($manifest | ConvertTo-Json -Depth 5), $utf8)
$french = @{schemaVersion=1;locale='fr-FR';nativeName='Francais';fontFamily='Segoe UI';strings=@{'common.close'='Fermer'}}
[IO.File]::WriteAllText((Join-Path $directory 'fr-FR.json'), ($french | ConvertTo-Json -Depth 5), $utf8)
& $tool -ProjectRoot $fixture -Generate | Out-Null
& $tool -ProjectRoot $fixture | Out-Null
$header = [IO.File]::ReadAllText((Join-Path $fixture 'work\LocalizationData.h'))
if (!$header.Contains('L"Fermer"') -or !$header.Contains('L"fr-FR"')) { throw 'Community language was not generated' }
$chinese = ConvertFrom-Json ([IO.File]::ReadAllText((Join-Path $directory 'zh-CN.json')))
$fallbackText = $chinese.strings.'setup.option.startup'
$frenchTable = [regex]::Match($header, '(?s)const wchar_t\* Text3\[\] = \{(.*?)\};').Groups[1].Value
if (!$frenchTable.Contains($fallbackText)) { throw 'Native Chinese fallback missing' }
$launcher = [IO.File]::ReadAllText((Join-Path $fixture 'work\LauncherLocalizationData.h'))
$launcherKeys = @([regex]::Matches($launcher, '(?m)^    "([^"]+)",') | ForEach-Object { $_.Groups[1].Value })
$expectedKeys = @($chinese.strings.PSObject.Properties.Name | Where-Object { $_ -like 'common.*' -or $_ -like 'chain.*' })
if (Compare-Object $expectedKeys $launcherKeys) { throw 'Unexpected launcher catalog keys' }
$launcherFrench = [regex]::Match($launcher, '(?s)const wchar_t\* Text3\[\] = \{(.*?)\};').Groups[1].Value
if (!$launcherFrench.Contains('L"Fermer"') -or !$launcherFrench.Contains($chinese.strings.'chain.timeout')) {
    throw 'Launcher community translation or Chinese fallback missing'
}

# Execute just the generated data and pure language helpers; never download, launch or change user settings.
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $fixture 'setup.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Generated PowerShell parse error' }
$data = $ast.Find({param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -eq '$setupLocales'}, $true)
Invoke-Expression $data.Extent.Text
foreach ($name in @('Get-SetupLanguage','Get-SetupText')) {
    $function = $ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name}, $true)
    Invoke-Expression $function.Extent.Text
}
function Get-ItemPropertyValue { return $preference }
function Get-UICulture { return [Globalization.CultureInfo]::GetCultureInfo($culture) }
$preference = ''; $culture = 'en-GB'
if ((Get-SetupLanguage) -ne 'en-US') { throw 'System language family did not match' }
$culture = 'ja-JP'
if ((Get-SetupLanguage) -ne 'ja-JP') { throw 'Japanese system match failed' }
$preference = 'zh-CN'
if ((Get-SetupLanguage) -ne 'zh-CN') { throw 'Manual language must override the system' }
$preference = 'auto'; $culture = 'en-US'
if ((Get-SetupLanguage) -ne 'en-US') { throw 'Auto selection must follow the system' }
$culture = 'es-ES'
if ((Get-SetupLanguage) -ne 'zh-CN') { throw 'Unsupported system languages must fall back to Chinese' }
$setupLanguage = 'fr-FR'
if ((Get-SetupText 'download.cleaned') -cne $chinese.strings.'download.cleaned') { throw 'PowerShell Chinese fallback missing' }
$setupLanguage = 'en-US'
if ((Get-SetupText 'download.version' @{version='literal {version}'}) -notlike '*literal {version}*') { throw 'Replacement values were interpreted recursively' }
foreach ($tag in @('zh-CN','en-US','ja-JP')) {
    $source = ConvertFrom-Json ([IO.File]::ReadAllText((Join-Path $directory ($tag + '.json'))))
    $setupLanguage = $tag
    if ((Get-SetupText 'download.star') -cne $source.strings.'download.star') { throw "Wrong download translation: $tag" }
}
Write-Output 'PASS localization tooling: strict catalogs, complete built-ins, placeholder validation, community fallback, standalone ASCII PowerShell, shared preference and system matching'
