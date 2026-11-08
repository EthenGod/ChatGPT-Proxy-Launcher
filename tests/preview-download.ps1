# SPDX-License-Identifier: GPL-3.0-only
# Manual preview: real Invoke-WebRequest progress with throttled loopback data.
param([ValidateRange(2, 30)][int]$Seconds = 15)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'Continue'
$root = Split-Path $PSScriptRoot
$entry = Join-Path $root 'setup.ps1'
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($entry, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw 'Could not parse setup.ps1' }
$wrapper = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-SetupProgress' }, $true)
$settings = $ast.Find({ param($node) $node -is [Management.Automation.Language.AssignmentStatementAst] -and $node.Left.Extent.Text -eq '$progress' }, $true)
if (!$wrapper -or !$settings) { throw 'Setup progress implementation was not found' }
# Load only the production progress helper and caption, never the deployment/download entry point.
Invoke-Expression $settings.Extent.Text
Invoke-Expression $wrapper.Extent.Text

$folder = Join-Path $root ('work\progress-preview-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $folder -Force
$output = Join-Path $folder 'preview.bin'
$totalBytes = 2MB
$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
$server = [PowerShell]::Create()
$pending = $null
try {
    $listener.Start()
    $port = $listener.LocalEndpoint.Port
    $serve = {
        param($Listener, $TotalBytes, $DelayMs)
        $client = $null; $stream = $null; $reader = $null
        try {
            $client = $Listener.AcceptTcpClient()
            $client.ReceiveTimeout = 5000
            $client.SendTimeout = 5000
            $stream = $client.GetStream()
            $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::ASCII, $false, 1024, $true)
            if ($reader.ReadLine() -notmatch '^GET /preview.bin HTTP/1\.[01]$') { throw 'Unexpected local request' }
            $headerBytes = 0
            while ($line = $reader.ReadLine()) {
                $headerBytes += $line.Length
                if ($headerBytes -gt 16384) { throw 'Local request headers too large' }
            }
            $header = "HTTP/1.1 200 OK`r`nContent-Type: application/octet-stream`r`nContent-Length: $TotalBytes`r`nConnection: close`r`n`r`n"
            $bytes = [Text.Encoding]::ASCII.GetBytes($header)
            $stream.Write($bytes, 0, $bytes.Length)
            $chunk = [byte[]]::new(32768)
            for ($sent = 0; $sent -lt $TotalBytes; $sent += $chunk.Length) {
                $stream.Write($chunk, 0, [Math]::Min($chunk.Length, $TotalBytes - $sent))
                $stream.Flush()
                [Threading.Thread]::Sleep($DelayMs)
            }
        } finally {
            if ($reader) { $reader.Dispose() }
            if ($stream) { $stream.Dispose() }
            if ($client) { $client.Dispose() }
        }
    }
    $null = $server.AddScript($serve.ToString()).AddArgument($listener).AddArgument($totalBytes).AddArgument([int][Math]::Ceiling($Seconds * 1000 / 64))
    $pending = $server.BeginInvoke()
    # Bypass proxy settings for this local fixture only.
    $session = [Microsoft.PowerShell.Commands.WebRequestSession]::new()
    $session.Proxy = [Net.WebProxy]::new()
    Write-Host ([regex]::Unescape('\u672c\u5730\u4e0b\u8f7d\u9884\u89c8\uff0c\u7ea6 {0} \u79d2\u3002\u53ef\u7f29\u7a84\u7a97\u53e3\u68c0\u67e5\u6362\u884c\u3002') -f $Seconds)
    Invoke-SetupProgress {
        Write-Progress @progress -Status ([regex]::Unescape('\u6b63\u5728\u4e0b\u8f7d\u914d\u7f6e\u5668'))
        Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$port/preview.bin" -WebSession $session -OutFile $output -TimeoutSec ($Seconds + 15)
        Write-Progress @progress -Status ([regex]::Unescape('\u6b63\u5728\u6821\u9a8c\u6587\u4ef6'))
        if ((Get-Item -LiteralPath $output).Length -ne $totalBytes) { throw 'Local download was incomplete' }
        Start-Sleep -Milliseconds 800
    }
    $null = $server.EndInvoke($pending)
    if ($server.HadErrors) { throw $server.Streams.Error[0] }
    Write-Host ([regex]::Unescape('\u9884\u89c8\u5b8c\u6210\u3002\u8fdb\u5ea6\u6761\u5e94\u5df2\u6d88\u5931\uff0c\u7a97\u53e3\u4f1a\u4fdd\u7559\u3002'))
} finally {
    $listener.Stop()
    if ($pending -and !$pending.IsCompleted) { $server.Stop() }
    $server.Dispose()
}
