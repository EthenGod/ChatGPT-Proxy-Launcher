# SPDX-License-Identifier: GPL-3.0-only
# Dot-source this file from a trusted extension. PowerShell 5.1 and 7 are supported.
function Read-CglContext {
    if ($env:CGL_CHAIN_PROTOCOL -cne '1' -or !$env:CGL_CONTEXT_FILE) { throw 'Not launched with chain protocol v1.' }
    $path = [IO.Path]::GetFullPath($env:CGL_CONTEXT_FILE)
    $file = [IO.File]::OpenRead($path)
    try {
        if ($file.Length -gt 65536) { throw 'Context is too large.' }
        $reader = New-Object IO.StreamReader($file, (New-Object Text.UTF8Encoding($false, $true)))
        try { $context = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    } finally { $file.Dispose() }
    if ($context.schemaVersion -ne 1 -or $context.protocolVersion -ne 1 -or
        $context.runId -cne $env:CGL_RUN_ID -or $context.entry.id -cne $env:CGL_ENTRY_ID -or
        $context.entry.attempt -ne [int]$env:CGL_ATTEMPT) { throw 'Invalid context identity.' }
    foreach ($name in @('runId','attemptId','token')) {
        if ($context.$name -cnotmatch '\A[0-9a-f]{64}\z') { throw 'Invalid context token.' }
    }
    return $context
}

function Send-CglReply {
    param(
        [Parameter(Mandatory=$true)]$Context,
        [Parameter(Mandatory=$true)][ValidateSet('ready','failed','already-running')][string]$Status,
        [string]$ErrorCode,
        [switch]$Retryable,
        [string]$Message,
        [int]$ExistingProcessId
    )
    # Call only after the extension has finished initialization, or diagnosed its failure.
    if (!$Context.PSObject.Properties['replyPath'] -or !$Context.replyPath) { throw 'This entry uses exit-code readiness.' }
    if ([DateTimeOffset]::UtcNow -ge [DateTimeOffset]$Context.deadlineUtc) { throw 'Attempt deadline has passed.' }
    $folder = [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($env:CGL_CONTEXT_FILE))
    $replyPath = [IO.Path]::GetFullPath($Context.replyPath)
    if ($replyPath -ine [IO.Path]::Combine($folder, 'reply.json')) { throw 'Unexpected reply path.' }
    $reply = [ordered]@{schemaVersion=1; runId=$Context.runId; attemptId=$Context.attemptId; token=$Context.token; status=$Status}
    if ($Status -eq 'failed') {
        if ($ErrorCode -cnotmatch '\A[a-z][a-z0-9-]{0,63}\z' -or ($Message -and $Message.Length -gt 256)) { throw 'Invalid failure description.' }
        $reply.errorCode=$ErrorCode; $reply.retryable=[bool]$Retryable
        if ($Message) { $reply.message=$Message }
    } elseif ($Status -eq 'already-running') {
        $existing = [Diagnostics.Process]::GetProcessById($ExistingProcessId)
        try {
            $reply.readyProcessId=$ExistingProcessId
            $reply.readyProcessCreationTime=$existing.StartTime.ToUniversalTime().ToFileTimeUtc().ToString('x16')
        } finally { $existing.Dispose() }
    }
    $bytes = (New-Object Text.UTF8Encoding($false)).GetBytes(($reply | ConvertTo-Json -Depth 4 -Compress))
    if ($bytes.Length -gt 4096) { throw 'Reply is too large.' }
    $temporary = $replyPath + '.tmp'
    $owned = $false
    try {
        $stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $owned = $true
        try { $stream.Write($bytes, 0, $bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
        # The protocol permits one reply per attempt. Do not overwrite an existing reply.
        [IO.File]::Move($temporary, $replyPath)
        $owned = $false
    } finally { if ($owned) { [IO.File]::Delete($temporary) } }
}
