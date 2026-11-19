# SPDX-License-Identifier: GPL-3.0-only
param([string]$ProjectRoot = (Split-Path $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$testRoot = Join-Path $ProjectRoot ('work\webview-download-' + [guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($testRoot)
function Require([bool]$Value, [string]$Message) { if (!$Value) { throw $Message } }
# Exercise the actual standalone entry's helpers without executing its download body.
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $ProjectRoot 'setup.ps1'), [ref]$tokens, [ref]$errors)
Require (!$errors) 'setup.ps1 must parse in Windows PowerShell 5.1'
$names = @('Test-SetupModernUi', 'Get-SetupRuntimeVersion', 'Test-SetupWebView2', 'Select-SetupUi',
    'Install-SetupWebView2', 'Get-SetupUiMode', 'Start-SetupGraphical')
foreach ($name in $names) {
    $node = $ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name}, $true)
    Require ($null -ne $node) "Missing helper: $name"
    . ([scriptblock]::Create($node.Extent.Text))
}
function Get-SetupText($Key, $Values) { return $Key }
function Invoke-SetupProgress([scriptblock]$Action) { & $Action }
function Show-SetupStage($Status, [switch]$Completed) { }

# Build a data-only capability fixture from our native EXE. No fixture is run.
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SetupUiFixture {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr BeginUpdateResourceW(string path, bool deleteExisting);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool UpdateResourceW(IntPtr handle, IntPtr type, IntPtr name, ushort language, byte[] bytes, uint length);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool EndUpdateResourceW(IntPtr handle, bool discard);
    public static void Set(string path, byte[] bytes) {
        IntPtr handle = BeginUpdateResourceW(path, false);
        if (handle == IntPtr.Zero) throw new InvalidOperationException("Cannot open fixture");
        bool ok = UpdateResourceW(handle, new IntPtr(10), new IntPtr(102), 0, bytes, (uint)bytes.Length);
        if (!EndUpdateResourceW(handle, !ok) || !ok) throw new InvalidOperationException("Cannot write fixture");
    }
}
'@
$native = Join-Path $ProjectRoot 'work\ui-native\ChatGPTLauncherSetup.exe'
Require (!(Test-SetupModernUi $native)) 'Legacy native build must not require WebView2'
Require (Test-SetupModernUi (Join-Path $ProjectRoot 'outputs\ChatGPTLauncherSetup.exe')) 'Default build must advertise the new UI and compatibility mode'
$dual = Join-Path $testRoot 'dual.exe'
[IO.File]::Copy($native, $dual)
[SetupUiFixture]::Set($dual, [IO.File]::ReadAllBytes((Join-Path $ProjectRoot 'src\SetupUiCapabilities.json')))
Require (Test-SetupModernUi $dual) 'Resource 102 must identify a dual-UI build without executing it'
$bad = Join-Path $testRoot 'invalid.exe'
[IO.File]::Copy($native, $bad)
[SetupUiFixture]::Set($bad, [Text.Encoding]::UTF8.GetBytes('{"schemaVersion":2}'))
$rejected = $false
try { Test-SetupModernUi $bad | Out-Null } catch { $rejected = $true }
Require $rejected 'Unknown capability must fail before launch'
Write-Host 'PASS UI capability: old native EXE, dual-UI resource and invalid schema'

# No real registry access, network, installer or GUI launch below.
$script:versions = @{}
$script:runtimeReads = 0
function Get-SetupRuntimeVersion($Hive, $View) {
    $script:runtimeReads++
    $value = $script:versions["$Hive/$View"]
    if ($value -eq 'deny') { throw [UnauthorizedAccessException]::new('fixture') }
    return $value
}
$script:versions = @{'LocalMachine/Registry32'='deny'; 'CurrentUser/Registry64'='123.0.0.1'}
Require (Test-SetupWebView2) 'One inaccessible view must not hide the current user runtime'
foreach ($value in @($null, '', '0.0.0.0', 'bad-version')) {
    $script:versions = @{'LocalMachine/Registry32'=$value}
    Require (!(Test-SetupWebView2)) 'Missing or invalid runtime version was accepted'
}
foreach ($location in @('LocalMachine/Registry32', 'LocalMachine/Registry64', 'CurrentUser/Registry32', 'CurrentUser/Registry64')) {
    $script:versions = @{$location='123.0.0.1'}
    Require (Test-SetupWebView2) "Runtime not found at $location"
}
Write-Host 'PASS runtime detection: user/machine, both registry views, empty/zero/invalid versions'
$script:answers = [Collections.Generic.Queue[string]]::new()
function Read-Host($Prompt) { return $script:answers.Dequeue() }
foreach ($case in @(@('1','install'), @('2','compatibility'), @('0','cancel'), @('','cancel'))) {
    $script:answers.Enqueue('invalid'); $script:answers.Enqueue($case[0])
    Require ((Select-SetupUi) -eq $case[1]) 'Prompt selected wrong mode'
}
function Reset-Scenario {
    $script:state = @{modern=$true; choice='compatibility'; prompts=0; downloads=0; installs=0; launches=0;
        signature='Valid'; publisher='CN=Microsoft Corporation, O=Microsoft Corporation, C=US';
        exitCode=0; runtimeAfterInstall=$true; downloadFails=$false; disposed=0; lastArguments=@()}
    $script:versions = @{}; $script:runtimeReads = 0
}
function Test-SetupModernUi($Path) { return $script:state.modern }
function Select-SetupUi { $script:state.prompts++; return $script:state.choice }
function Invoke-WebRequest($Uri, $OutFile, $TimeoutSec, [switch]$UseBasicParsing) {
    Require ($Uri -ceq 'https://go.microsoft.com/fwlink/p/?LinkId=2124703') 'Installer must use the Microsoft URL'
    $script:state.downloads++
    if ($script:state.downloadFails) { throw 'isolated network failure' }
    [IO.File]::WriteAllText($OutFile, 'isolated installer substitute')
}
function Get-AuthenticodeSignature($LiteralPath) {
    if ($script:state.holdInstaller) { $script:state.reader = [IO.File]::OpenRead($LiteralPath) }
    return [pscustomobject]@{Status=$script:state.signature; SignerCertificate=[pscustomobject]@{Subject=$script:state.publisher}}
}
function Start-Process($FilePath, $WorkingDirectory, $ArgumentList, $WindowStyle, [switch]$PassThru) {
    Require $PassThru 'A process handle is required'
    $process = [pscustomobject]@{ExitCode=$script:state.exitCode}
    $process | Add-Member ScriptMethod Dispose { $script:state.disposed++ }
    if ($FilePath -eq $installer) {
        Require (($ArgumentList -join ' ') -ceq '/silent /install' -and $WindowStyle -eq 'Hidden') 'Wrong installer arguments'
        # Verified bytes must remain locked against writes while starting the installer.
        $locked = $false
        try { $write = [IO.File]::OpenWrite($FilePath); $write.Dispose() } catch { $locked = $true }
        Require $locked 'Installer was not protected after signature verification'
        $script:state.installs++
        $process | Add-Member ScriptMethod WaitForExit {
            if ($script:state.runtimeAfterInstall) { $script:versions['CurrentUser/Registry32']='123.0.0.1' }
        }
    } else {
        Require ($FilePath -eq $dual -and $WorkingDirectory -eq $testRoot) 'Unexpected GUI path'
        $script:state.launches++; $script:state.lastArguments = @($ArgumentList)
    }
    return $process
}
$installer = Join-Path $testRoot 'MicrosoftEdgeWebview2Setup.exe'
function Open-TestUi { Start-SetupGraphical $dual $testRoot $installer }
Reset-Scenario
$script:state.modern = $false
Open-TestUi | Out-Null
Require ($state.launches -eq 1 -and $state.prompts -eq 0 -and $runtimeReads -eq 0 -and $state.downloads -eq 0) 'Legacy EXE must skip all WebView checks'
Reset-Scenario
$script:versions['LocalMachine/Registry32']='123.0.0.1'
Open-TestUi | Out-Null
Require ($state.launches -eq 1 -and $state.prompts -eq 0 -and $state.downloads -eq 0) 'Installed runtime must skip the prompt'
Reset-Scenario
Open-TestUi | Out-Null
Require ($state.launches -eq 1 -and $state.prompts -eq 1 -and $state.lastArguments[0] -ceq '--compatibility' -and $state.installs -eq 0) 'Compatibility must launch without installation'
Reset-Scenario
$state.choice = 'cancel'
Require ($null -eq (Open-TestUi)) 'Cancellation must return no process'
Require ($state.launches -eq 0 -and $state.downloads -eq 0) 'Cancellation caused a side effect'
foreach ($code in @(0, 3010)) {
    Reset-Scenario; $state.choice='install'; $state.exitCode=$code
    Open-TestUi | Out-Null
    Require ($state.downloads -eq 1 -and $state.installs -eq 1 -and $state.launches -eq 1 -and $state.disposed -eq 1) 'Install-success flow incomplete'
    Require (!$state.lastArguments[0]) 'Successful runtime installation should open the modern UI'
}
foreach ($failure in @('network', 'signature', 'publisher', 'exit', 'missing')) {
    Reset-Scenario; $state.choice='install'
    switch ($failure) {
        'network' { $state.downloadFails=$true }
        'signature' { $state.signature='HashMismatch' }
        'publisher' { $state.publisher='O=Untrusted Publisher, C=US' }
        'exit' { $state.exitCode=1603 }
        'missing' { $state.runtimeAfterInstall=$false }
    }
    $failed = $false
    try { Open-TestUi | Out-Null } catch { $failed = $true }
    Require ($failed -and $state.launches -eq 0) "Failed $failure must not open the modern GUI"
    if ($failure -in @('network','signature','publisher')) { Require ($state.installs -eq 0) 'Unverified installer was executed' }
}
Reset-Scenario; $state.choice='install'; $state.holdInstaller=$true
try {
    Open-TestUi | Out-Null
    Require ($state.launches -eq 1 -and [IO.File]::Exists($installer)) 'A temporary cleanup failure blocked the GUI after installation'
} finally { if ($state.reader) { $state.reader.Dispose() } }
Reset-Scenario; $state.choice='install'
[IO.File]::WriteAllText($installer,'foreign')
$rejected = $false
try { Open-TestUi | Out-Null } catch { $rejected = $true }
Require ($rejected -and !$state.downloads -and [IO.File]::ReadAllText($installer) -eq 'foreign') 'Pre-existing installer was modified'
Write-Host 'PASS GUI dispatch: legacy, installed, compatibility, cancel, install success, signature/network/install failures'

# Run the complete entry's cancellation/cleanup path. Replace only OS discovery;
# all branching, hashing, download ownership and cleanup code stays unchanged.
$cancelEntry = [IO.File]::ReadAllText((Join-Path $ProjectRoot 'setup.ps1'))
foreach ($name in @('Test-SetupModernUi', 'Get-SetupRuntimeVersion')) {
    $node = $ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name}, $true)
    $replacement = if ($name -eq 'Test-SetupModernUi') { 'function Test-SetupModernUi { return $true }' }
        else { 'function Get-SetupRuntimeVersion { return $null }' }
    $cancelEntry = $cancelEntry.Replace($node.Extent.Text, $replacement)
}
$cancelScript = Join-Path $testRoot 'cancel-entry.ps1'
[IO.File]::WriteAllText($cancelScript, $cancelEntry, [Text.Encoding]::ASCII)
& {
    $cancelObservation = @{folder=$null}
    $oldTemp = $env:TEMP
    $hash = (Get-FileHash -LiteralPath $native -Algorithm SHA256).Hash
    function Get-ItemPropertyValue { return 'en-US' }
    function Read-Host {
        if ($cancelObservation.foreign) {
            [IO.File]::WriteAllText((Join-Path $cancelObservation.folder 'MicrosoftEdgeWebview2Setup.exe'),'unrelated')
        }
        return '0'
    }
    function Invoke-RestMethod { return @{tag_name='v1.1.3'; draft=$false; prerelease=$false} }
    function Invoke-WebRequest($Uri, $OutFile, $TimeoutSec, [switch]$UseBasicParsing) {
        $cancelObservation.folder = Split-Path $OutFile
        if ($Uri.EndsWith('/SHA256SUMS.txt')) { [IO.File]::WriteAllText($OutFile, "$hash  ChatGPTLauncherSetup.exe") }
        elseif ($Uri.EndsWith('/ChatGPTLauncherSetup.exe')) { [IO.File]::Copy($native, $OutFile) }
        else { throw 'Cancellation downloaded an installer' }
    }
    function Start-Process { throw 'Cancellation launched a process' }
    function Start-Sleep { throw 'Cancellation entered completion countdown' }
    try {
        $env:TEMP = $testRoot
        & $cancelScript
        Require ($cancelObservation.folder -and ![IO.Directory]::Exists($cancelObservation.folder)) 'Cancellation left download files behind'
        $cancelObservation.foreign = $true
        & $cancelScript
        Require ([IO.File]::ReadAllText((Join-Path $cancelObservation.folder 'MicrosoftEdgeWebview2Setup.exe')) -eq 'unrelated') 'Cancellation removed an unrelated file'
    } finally { $env:TEMP = $oldTemp }
}
Write-Host 'PASS complete entry cancellation: verified download cleaned, no installer/GUI/browser, returned to caller'
Write-Host 'All registry/network/install/GUI operations were replaced; no runtime was installed.'
