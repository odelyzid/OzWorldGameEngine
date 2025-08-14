param(
    [int]$Display = 0,
    [switch]$NoFirewallRule,
    [switch]$NoMinimize,
    [switch]$DebugRun,
    [switch]$UseWgl,
    [switch]$UseWglSoftware
)

$ErrorActionPreference = 'Stop'

$vcxsrvPathCandidates = @(
    "$Env:ProgramFiles\VcXsrv\vcxsrv.exe",
    "$Env:ProgramFiles(x86)\VcXsrv\vcxsrv.exe"
)
$vcxsrvExe = $vcxsrvPathCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcxsrvExe) {
    Write-Error "VcXsrv not found. Install from: https://sourceforge.net/projects/vcxsrv/"
}

# Kill existing instances
Get-Process -Name vcxsrv -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# Open firewall for TCP 6000 unless opted out
if (-not $NoFirewallRule) {
    try {
        $ruleName = "VcXsrv X11 (6000)"
        $existing = (netsh advfirewall firewall show rule name="$ruleName" dir=in) 2>$null
        if (-not $existing -or ($existing -join "") -match "No rules match the specified criteria") {
            netsh advfirewall firewall add rule name="$ruleName" dir=in action=allow protocol=TCP localport=6000 program="$vcxsrvExe" | Out-Null
        }
    } catch {
        Write-Warning "Could not ensure firewall rule. Run PowerShell as Administrator."
    }
}

# Build arguments
$displayArg = ":$Display"
$args = @(
    $displayArg,
    '-multiwindow',
    '-clipboard',
    '-primary',
    '-ac'            # disable access control
)
if ($UseWgl) {
    # Enable GLX via native WGL (can yield accelerated D3D12 Mesa)
    $args += @('+iglx', '-wgl')
} elseif ($UseWglSoftware) {
    # Enable GLX via WGL but force software path
    $args += @('+iglx', '-wgl', '-swrastwgl')
} else {
    # Disable native OpenGL; rely on indirect GLX
    $args += @('+iglx', '-nowgl')
}
$args += @('-listen', 'tcp') # ensure TCP listening on 6000

# Log to temp for troubleshooting
$logFile = Join-Path $env:TEMP ("vcxsrv-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
$args += @('-logfile', $logFile)

# Start VcXsrv
if ($DebugRun) {
    Write-Host "Starting VcXsrv in debug (attached) mode..."
    & $vcxsrvExe @args 2>&1 | Tee-Object -FilePath $logFile
} else {
    if ($NoMinimize) {
        Start-Process -FilePath $vcxsrvExe -ArgumentList $args
    } else {
        Start-Process -FilePath $vcxsrvExe -ArgumentList $args -WindowStyle Minimized
    }
}

# Print helpful info
$hostIPs = (Get-NetIPAddress -AddressFamily IPv4 -PrefixOrigin Dhcp, Manual | Where-Object { $_.IPAddress -ne '127.0.0.1' }).IPAddress
Write-Host "VcXsrv started on display :$Display" -ForegroundColor Green
Write-Host "Host IPv4s: $($hostIPs -join ', ')"
Write-Host "In WSL, set DISPLAY to 'localhost:$Display.0' (preferred) or '<HostIP>:$Display.0'"
Write-Host "Log file: $logFile"

# Quick sanity: show whether something is listening on 6000
try {
    $listening = netstat -ano | Select-String ":6000" | ForEach-Object { $_.Line }
    if ($listening) { Write-Host "TCP 6000 state:"; $listening | Write-Host }
} catch { }