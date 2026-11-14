# SPDX-License-Identifier: GPL-3.0-only
param([string]$ProjectRoot = (Split-Path $PSScriptRoot))
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $ProjectRoot 'docs\examples\companion-context.ps1')
$root = Join-Path $ProjectRoot ('work\protocol-example-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $root
$names = @('CGL_CHAIN_PROTOCOL','CGL_CONTEXT_FILE','CGL_RUN_ID','CGL_ENTRY_ID','CGL_ATTEMPT')
$saved = @{}
foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $env:CGL_CHAIN_PROTOCOL='1'; $env:CGL_RUN_ID='a' * 64; $env:CGL_ENTRY_ID='org.example.test'; $env:CGL_ATTEMPT='1'
    foreach ($status in @('ready','failed','already-running')) {
        $folder = Join-Path $root $status; $null = New-Item -ItemType Directory -Path $folder
        $env:CGL_CONTEXT_FILE=Join-Path $folder 'context.json'
        $context = [ordered]@{schemaVersion=1;protocolVersion=1;runId=$env:CGL_RUN_ID;attemptId=('b'*64);token=('c'*64);
            deadlineUtc=[DateTime]::UtcNow.AddSeconds(10).ToString('o');replyPath=(Join-Path $folder 'reply.json');entry=@{id=$env:CGL_ENTRY_ID;attempt=1}}
        [IO.File]::WriteAllText($env:CGL_CONTEXT_FILE, ($context | ConvertTo-Json))
        $read = Read-CglContext
        $null = Send-CglReply -Context $read -Status $status -ErrorCode 'invalid-config' -ExistingProcessId $PID
        $bytes = [IO.File]::ReadAllBytes($context.replyPath)
        if ($bytes.Length -gt 4096 -or $bytes[0] -ne 123 -or [IO.File]::Exists($context.replyPath+'.tmp')) { throw 'Invalid atomic reply.' }
        $reply = [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
        if ($reply.status -cne $status -or $reply.token -cne $context.token) { throw 'Reply mismatch.' }
        if ($status -eq 'failed' -and $reply.retryable) { throw 'Default failure must not request retry.' }
        if ($status -eq 'already-running' -and ($reply.readyProcessId -ne $PID -or $reply.readyProcessCreationTime -cnotmatch '\A[0-9a-f]{16}\z')) { throw 'Invalid process identity.' }
        $before=[IO.File]::ReadAllText($context.replyPath); $rejected=$false
        try { Send-CglReply -Context $read -Status 'ready' } catch { $rejected=$true }
        if (!$rejected -or [IO.File]::ReadAllText($context.replyPath) -cne $before) { throw 'Existing reply changed.' }
        if ([IO.File]::Exists($context.replyPath+'.tmp')) { throw 'Failed write left its temporary file.' }
        $read.deadlineUtc = [DateTime]::UtcNow.AddSeconds(-1).ToString('o'); $rejected=$false
        try { Send-CglReply -Context $read -Status 'ready' } catch { $rejected=$true }
        if (!$rejected) { throw 'Late reply was accepted.' }
    }
    $read.PSObject.Properties.Remove('replyPath'); $rejected=$false
    try { Send-CglReply -Context $read -Status 'ready' } catch { $rejected=$_.Exception.Message -eq 'This entry uses exit-code readiness.' }
    if (!$rejected) { throw 'Exit-code context was not handled under StrictMode.' }
    'PASS protocol example: UTF-8 atomic replies, ready/failure/existing identity, no overwrite and deadline.'
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
}
