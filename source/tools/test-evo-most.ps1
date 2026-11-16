[CmdletBinding()]
param([string]$Head='169.254.199.119',[switch]$Parked,[switch]$ExhaustInfo,[switch]$EngineInfo,[Security.SecureString]$Password)
. "$PSScriptRoot/EvoDeploy.ps1"
. "$PSScriptRoot/EvoBatch.ps1"
if($ExhaustInfo -and $EngineInfo){throw 'Select only one identification mode.'}
if(-not $Parked -and (Read-Host 'Auto stoi, zaplon wlaczony, testery diagnostyczne zamkniete? Wpisz TAK') -cne 'TAK'){throw 'Probe cancelled.'}
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output=Join-Path $root 'source/native/out/dashboard'
$helper=Join-Path $output 'gaugedeck-fileops'
$probe=Join-Path $output 'most-probe-qnx'
$run="$script:Runs/most-probe-$([Guid]::NewGuid().ToString('N'))"
$argument=if($ExhaustInfo){' --exhaust-info'}elseif($EngineInfo){' --engine-info'}else{''}
$commands=@('set -e',(Get-EvoFrameworkChecks "$run/fileops"),"chmod 755 '$run/most-probe-qnx'",
    "LD_LIBRARY_PATH=/lib:/usr/lib:/boot/lib:/opt/sys/lib '$run/most-probe-qnx'$argument 2>&1 || true",'echo EVO_PROBE_COMPLETE')
$entries=@(@{name='most-probe-qnx';bytes=[IO.File]::ReadAllBytes($probe)},
    @{name='deploy.sh';bytes=[Text.Encoding]::UTF8.GetBytes(($commands -join "`n")+"`n")})
$payload=New-EvoBundle -Helper $helper -Entries $entries
Initialize-EvoConnection -Head $Head -Password $Password
$result=Invoke-Evo -Command (Get-EvoBatchBootstrap -Run $run -Helper $helper) -InputBytes $payload -StreamText -Activity 'MOST: ograniczone odczyty DME, bez instalacji aplikacji i restartu'
$folder=Join-Path $root '.local/diagnostics'
New-Item -ItemType Directory -Force $folder | Out-Null
$file=Join-Path $folder ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-most.txt')
[IO.File]::WriteAllText($file,$result,[Text.UTF8Encoding]::new($false))
Write-Host "MOST probe saved: $file. Temporary files: $run"
