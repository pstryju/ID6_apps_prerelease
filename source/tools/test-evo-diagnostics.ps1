[CmdletBinding()]
param(
    [string]$Head='169.254.199.119',
    [string[]]$Gateways=@('169.254.20.21','169.254.21.3','160.48.199.99','160.48.199.97','160.48.199.253'),
    [switch]$Parked,
    [switch]$Signals,
    [Security.SecureString]$Password
)
. "$PSScriptRoot/EvoDeploy.ps1"
. "$PSScriptRoot/EvoBatch.ps1"
foreach($gateway in $Gateways){$ip=$null;if(-not [Net.IPAddress]::TryParse($gateway,[ref]$ip)-or $ip.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork){throw 'Invalid gateway IPv4 address.'}}
if(-not $Parked -and (Read-Host 'Auto stoi, zaplon wlaczony, aplikacje zegarow i inne testery zamkniete? Wpisz TAK') -cne 'TAK'){throw 'Probe cancelled.'}
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output=Join-Path $root 'source/native/out/dashboard'
$helper=Join-Path $output 'gaugedeck-fileops'
$probe=Join-Path $output 'diagnostic-probe-qnx'
$run="$script:Runs/probe-$([Guid]::NewGuid().ToString('N'))"
$commands=@('set -e',(Get-EvoFrameworkChecks "$run/fileops"),"chmod 755 '$run/diagnostic-probe-qnx'")
foreach($gateway in $Gateways){
    $commands+="echo '=== GATEWAY $gateway ==='"
    $signalArgument=if($Signals){' --signals'}else{''}
    $commands+="LD_LIBRARY_PATH=/lib:/usr/lib:/boot/lib '$run/diagnostic-probe-qnx' --host '$gateway'$signalArgument || true"
}
$commands+='echo EVO_PROBE_COMPLETE'
$entries=@(@{name='diagnostic-probe-qnx';bytes=[IO.File]::ReadAllBytes($probe)},
    @{name='deploy.sh';bytes=[Text.Encoding]::UTF8.GetBytes((($commands -join "`n")+"`n").Replace("`r`n","`n").Replace("`r","`n"))})
$payload=New-EvoBundle -Helper $helper -Entries $entries
Initialize-EvoConnection -Head $Head -Password $Password
$result=Invoke-Evo -Command (Get-EvoBatchBootstrap -Run $run -Helper $helper) -InputBytes $payload -StreamText -Activity 'Jedna sesja: tylko odczyt RPM, predkosci i temperatury przez wskazane bramki'
$folder=Join-Path $root '.local/diagnostics'
New-Item -ItemType Directory -Force $folder | Out-Null
$file=Join-Path $folder ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'.txt')
[IO.File]::WriteAllText($file,$result,[Text.UTF8Encoding]::new($false))
Write-Host "Probe saved: $file. No ECU writes, fault clearing, plugin installation or restart."
