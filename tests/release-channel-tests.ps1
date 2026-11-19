# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
$entry = Join-Path (Split-Path $PSScriptRoot) 'setup.ps1'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($entry, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw $errors[0] }
$selector = $ast.Find({ param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Get-SetupReleaseTag' }, $true)
. ([scriptblock]::Create($selector.Extent.Text))
function Get-SetupText($Key) { $Key }

$tip = 'a' * 40
$calls = [Collections.Generic.List[string]]::new()
$api = 'https://api.github.com/repos/owner/repo'
$routes = @{}
function Invoke-RestMethod($Uri, $TimeoutSec) {
    if (!$Uri.StartsWith($api + '/') -or $TimeoutSec -ne 30) { throw "Unexpected request: $Uri" }
    $path = $Uri.Substring($api.Length)
    $calls.Add($path)
    if (!$routes.ContainsKey($path)) { throw "Unexpected route: $path" }
    return ,$routes[$path]
}
function Release($Name, $Date, [bool]$Pre, [long]$Id) {
    [pscustomobject]@{tag_name=$Name; prerelease=$Pre; draft=$false; published_at=$Date; id=$Id; target_commitish='master'}
}
function Check($Expected, $Channel='Stable', $Branch='', $Tag='') {
    $calls.Clear()
    $actual = Get-SetupReleaseTag 'owner/repo' $Channel $Branch $Tag
    if ($actual -cne $Expected) { throw "Expected $Expected, got $actual" }
}
function Reject($Expected, $Channel='Stable', $Branch='', $Tag='') {
    $calls.Clear()
    try { $actual = Get-SetupReleaseTag 'owner/repo' $Channel $Branch $Tag }
    catch { if ($_.Exception.Message -cne $Expected) { throw }; return }
    throw "Unexpected release accepted: $actual"
}
$stable = Release 'v1.1.3' '2026-11-01T15:00:00Z' $false 10
$beta = Release 'v1.2.0-beta.1' '2026-11-02T11:00:00Z' $true 11
$draft = Release 'v1.3.0' '2026-11-03T11:00:00Z' $false 12
$draft.draft = $true
$routes['/releases/latest'] = $stable
$routes['/releases/tags/v1.2.0-beta.1'] = $beta
$routes['/branches/refactor%2Fui'] = @{name='refactor/ui'; commit=@{sha=$tip}}
$page1 = '/releases?per_page=100&page=1'
$page2 = '/releases?per_page=100&page=2'
$routes[$page1] = @($draft, $stable, $beta)
Check 'v1.1.3'
if ($calls.Count -ne 1 -or $calls[0] -cne '/releases/latest') { throw 'Default stable path changed' }
Check 'v1.2.0-beta.1' Preview
Check 'v1.2.0-beta.1' Preview '' 'v1.2.0-beta.1'
Reject 'download.norelease' Stable '' 'v1.2.0-beta.1'
$routes['/releases/tags/v1.2.0'] = $stable
Reject 'download.norelease' Preview '' 'v1.2.0'
$routes['/releases/latest'] = $beta
Reject 'download.norelease'
$routes['/releases/latest'] = $draft
Reject 'download.norelease'
$routes['/releases/latest'] = @{tag_name='v1.2.0'; draft=$false}
Reject 'download.norelease'
$routes['/releases/latest'] = $stable
$stableCompare = '/compare/v1.1.3...' + $tip + '?per_page=1'
$betaCompare = '/compare/v1.2.0-beta.1...' + $tip + '?per_page=1'
$routes[$stableCompare] = @{status='ahead'}
$routes[$betaCompare] = @{status='identical'}
Check 'v1.2.0-beta.1' Preview 'refactor/ui'
Check 'v1.1.3' Stable 'refactor/ui'
$beta2 = Release 'v1.2.0-beta.2' '2026-11-03T09:00:00Z' $true 14
$routes[$page1] += $beta2
$routes['/releases/tags/v1.2.0-beta.2'] = $beta2
$beta2Compare = '/compare/v1.2.0-beta.2...' + $tip + '?per_page=1'
$routes[$beta2Compare] = @{status='identical'}
Check 'v1.2.0-beta.2' Preview 'refactor/ui'
Check 'v1.2.0-beta.2' Preview 'refactor/ui' 'v1.2.0-beta.2'
Check 'v1.2.0-beta.1' Preview 'refactor/ui' 'v1.2.0-beta.1'
Check 'v1.1.3' Stable 'refactor/ui'
$routes[$beta2Compare] = @{status='diverged'}
Check 'v1.2.0-beta.1' Preview 'refactor/ui'
foreach ($status in 'behind','diverged') {
    $routes[$betaCompare] = @{status=$status}
    Check 'v1.1.3' Preview 'refactor/ui'
    Reject 'download.norelease' Preview 'refactor/ui' 'v1.2.0-beta.1'
}
$routes[$betaCompare] = @{status='unknown'}
Reject 'download.invalidbranch' Preview 'refactor/ui'
$routes.Remove($betaCompare)
Reject "Unexpected route: $betaCompare" Preview 'refactor/ui'
foreach ($branch in @('../ui','test?name','#bad..','test//ui','ui.lock','ui\test','ui^','ui[2','ui@{0}','bad branch')) {
    Reject 'download.invalidbranch' Preview $branch
    if ($calls.Count) { throw 'Invalid branch reached network' }
}
foreach ($tag in 'v1.2.0/other','../master','v1.2.0?token','v1.2.0-beta..1') {
    Reject 'download.releasetag' Preview '' $tag
    if ($calls.Count) { throw 'Invalid tag reached network' }
}
$routes['/branches/refactor%2Fui'].commit.sha = 'not-a-sha'
Reject 'download.invalidbranch' Preview 'refactor/ui'
$routes['/branches/refactor%2Fui'].commit.sha = $tip
$routes[$page1] = @($draft) * 99 + @($beta)
$newStable = Release 'v1.2.0' '2026-11-03T19:00:00+08:00' $false 13
$routes[$page2] = @($newStable)
Check 'v1.2.0' Preview
if (!$calls.Contains($page2)) { throw 'Missing second page' }
$newStable.published_at = $beta.published_at
Check 'v1.2.0' Preview
$routes[$page1] = @()
Reject 'download.norelease' Preview
Write-Output 'PASS release selection: stable default, inclusive preview, published dates, pagination, exact tag, branch history and failed lookups.'
