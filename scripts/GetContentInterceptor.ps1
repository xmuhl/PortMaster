# Get-Content full-read interceptor (safe minimal version)
# - Overrides Get-Content in the current session to always add -ReadCount 0.
# - Affects current process only. Call Disable-ForceFullRead to restore default.
# - The original cmdlet remains available as Microsoft.PowerShell.Management\Get-Content.

Set-StrictMode -Version Latest

function __ForceGetContent {
    [CmdletBinding(DefaultParameterSetName="Path")]
    param(
        [Parameter(ParameterSetName='Path', Position=0, ValueFromPipeline=$true, ValueFromPipelineByPropertyName=$true)]
        [Alias('PSPath')]
        [string[]]$Path,

        [Parameter(ParameterSetName='LiteralPath', ValueFromPipelineByPropertyName=$true)]
        [string[]]$LiteralPath,

        [System.Text.Encoding]$Encoding,
        [switch]$Raw,
        [switch]$Wait,
        [Parameter(ValueFromRemainingArguments = $true)]
        [object[]]$All
    )

    if ($Wait) { Write-Warning 'Get-Content interceptor: -Wait not supported; returning current snapshot' }

    $targets = @()
    if ($PSCmdlet.ParameterSetName -eq 'LiteralPath' -and $LiteralPath) {
        $targets = $LiteralPath
    } elseif ($Path) {
        $targets = $Path
    }

    foreach ($t in $targets) {
        $args = @()
        if ($Encoding) { $args += @('-Encoding', $Encoding) }
        if ($Raw) { $args += '-Raw' }
        $args += @('-ReadCount','0')

        if ($PSCmdlet.ParameterSetName -eq 'LiteralPath') {
            Microsoft.PowerShell.Management\Get-Content -LiteralPath $t @args
        } else {
            Microsoft.PowerShell.Management\Get-Content -Path $t @args
        }
    }
}
function Enable-ForceFullRead {
    [CmdletBinding()] param()
    # 全局覆盖（当前进程），优先级高于 Cmdlet
    $sb = [scriptblock]::Create(${function:__ForceGetContent}.ToString())
    New-Item -Path Function:\Get-Content -Value $sb -Force | Out-Null
    Write-Host '[INFO] Enabled: Force full-read (Get-Content uses -ReadCount 0)' -ForegroundColor Green
}

function Disable-ForceFullRead {
    [CmdletBinding()] param()
    if (Test-Path Function:\Get-Content) {
        try {
            # 恢复到系统原始实现：移除函数覆盖（不会删除 Cmdlet 本体）
            Remove-Item Function:\Get-Content -ErrorAction SilentlyContinue
            Write-Host '[INFO] Disabled: Restored original Get-Content behavior' -ForegroundColor Yellow
        } catch {}
    }
}

# 兼容旧命令名
function Enable-GetContentInterceptor { Enable-ForceFullRead }
function Disable-GetContentInterceptor { Disable-ForceFullRead }
