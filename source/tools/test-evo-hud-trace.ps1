[CmdletBinding()]
param([string]$Head='169.254.199.119',[switch]$Parked,[Security.SecureString]$Password)
throw 'HUD trace disabled: target SIGSEGV followed by owner-reported Evo restart. Do not retry on the vehicle; offline investigation required.'
. "$PSScriptRoot/EvoDeploy.ps1"
. "$PSScriptRoot/EvoBatch.ps1"
if(-not $Parked -and (Read-Host 'Auto stoi, nasz overlay HUD jest WYLACZONY? Wpisz TAK') -cne 'TAK'){throw 'Trace cancelled.'}
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output=Join-Path $root 'source/native/out/dashboard'
$helper=Join-Path $output 'gaugedeck-fileops'
$probe=Join-Path $output 'hud-trace-qnx'
$run="$script:Runs/hud-trace-$([Guid]::NewGuid().ToString('N'))"
$commands=@('set -e',(Get-EvoFrameworkChecks "$run/fileops"),"chmod 755 '$run/hud-trace-qnx'",
    "LD_LIBRARY_PATH=/lib:/usr/lib:/boot/lib:/opt/sys/lib '$run/hud-trace-qnx' 2>&1 || true",'echo EVO_TRACE_COMPLETE')
$entries=@(@{name='hud-trace-qnx';bytes=[IO.File]::ReadAllBytes($probe)},
    @{name='deploy.sh';bytes=[Text.Encoding]::UTF8.GetBytes((($commands -join "`n")+"`n").Replace("`r`n","`n"))})
$payload=New-EvoBundle -Helper $helper -Entries $entries
Initialize-EvoConnection -Head $Head -Password $Password
Write-Host 'Po HUD_TRACE START: wlacz trase navi, odczekaj ok. 10 s, przerwij prowadzenie. Overlay pozostaw wylaczony.'
$result=Invoke-Evo -Command (Get-EvoBatchBootstrap -Run $run -Helper $helper) -InputBytes $payload -StreamText -Activity 'Pasywny slad HUD: 45 s, bez rysowania, DME i restartu'
$folder=Join-Path $root '.local/diagnostics'
New-Item -ItemType Directory -Force $folder | Out-Null
$file=Join-Path $folder ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-hud-trace.txt')
[IO.File]::WriteAllText($file,$result,[Text.UTF8Encoding]::new($false))
if($result -notmatch 'HUD_TRACE COMPLETE records=[1-9][0-9]*'){throw "No complete terminal capture. Inspect $file"}
Write-Host "HUD trace saved: $file. Temporary files: $run"
