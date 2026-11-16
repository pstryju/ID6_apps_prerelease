param(
    [string]$EvoDirectory = 'C:\Users\pawel\evo',
    [string]$ReadElf = 'C:\Users\pawel\Developer\qnx650-docker\.local-qnx\research\ntoarmv7-readelf.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output = Join-Path $root 'source/native/out/dashboard'
$stage = Join-Path $output 'candidate'
function Read-Table([string]$path) {
    $lines = @(& $ReadElf -s -D -W $path)
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $path" }
    return $lines
}
$exports = @{}
$frameworks = foreach ($name in @('libAppFramework.A.so','libCoreFramework.A.so')) {
    $path = Join-Path $EvoDirectory "opt/hmi/ID5/lib/$name"
    foreach ($line in (Read-Table $path)) {
        if ($line -match '\s(?:GLOBAL|WEAK)\s+DEFAULT\s+(?!UND)\S+\s+(\S+)') { $exports[$matches[1]] = $true }
    }
    @{ name=$name; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower() }
}
$failures = [Collections.Generic.List[string]]::new()
$dsiPath=Join-Path $EvoDirectory 'opt/sys/lib/libsys_dsi_base.cpp-ne.so.4'
$dsiTable=Read-Table $dsiPath
foreach($symbol in @('DSIInit','DSIClientInit','DSIClientRun','DSIClientStop','DSIClientFree',
    'DSIClientSendRegister','DSIClientSetNotification','DSIClientSendRequest','DSICreateId',
    'DSIOpen','DSIClose','DSIWrite','DSIRead','DSIWriteString2')) {
    if(-not @($dsiTable | Where-Object {$_ -match ('\s(?:GLOBAL|WEAK)\s+DEFAULT\s+(?!UND)\S+\s'+$symbol+'$')}).Count){$failures.Add("DSI: missing $symbol")}
}
$mostWorker=Join-Path $stage 'BoostDeck/boostdeck-most-qnx'
if(Test-Path -LiteralPath $mostWorker){
    foreach($line in (Read-Table $mostWorker)){
        if($line -match '\sUND\s+connect$'){$failures.Add('BoostDeck MOST worker contains an unexpected TCP connect import')}
    }
}
$libraries = foreach ($file in Get-ChildItem -LiteralPath $stage -Filter '*.so' -File -Recurse) {
    $imports = @(foreach ($line in (Read-Table $file.FullName)) {
        if ($line -match '\sUND\s+(\S*bmw\S*)') { $matches[1] }
    }) | Sort-Object -Unique
    $missing = @($imports | Where-Object { -not $exports.ContainsKey($_) })
    if (-not @($imports).Count) { $failures.Add("$($file.Name): no BMW imports found; inspection is incomplete") }
    foreach ($name in $missing) { $failures.Add("$($file.Name): missing BMW symbol $name") }
    if($file.Name -eq 'libShiftDeck.so') {
        $networkImports=@((Read-Table $file.FullName) | Where-Object {$_ -match '\sUND\s+(socket|connect|recv|recvfrom|posix_spawn|fork|spawn.*)$'})
        if($networkImports.Count){$failures.Add("$($file.Name): diagnostic/network imports in CDS gauge")}
    }
    $needed = @(& $ReadElf -d $file.FullName | ForEach-Object {
        if ($_ -match 'NEEDED.*\[(.+)\]') { $matches[1] }
    })
    @{ name=$file.Name; bmw_import_count=@($imports).Count; missing_bmw_imports=$missing; needed=$needed }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$resources = foreach ($folder in Get-ChildItem -LiteralPath $stage -Directory) {
    [xml]$xml = [IO.File]::ReadAllText((Join-Path $folder.FullName ($folder.Name+'.xml')))
    $models = @($xml.SelectNodes('//imageIdModel'))
    $zipPath = Join-Path $folder.FullName ($folder.Name+'_ALL_Images.zip')
    if ($models.Count) {
        $zip = [IO.Compression.ZipFile]::OpenRead($zipPath)
        try {
            $names = @($zip.Entries | ForEach-Object { $_.Name })
            foreach ($model in $models) {
                if (($model.imageId+'.png') -notin $names) { $failures.Add("$($folder.Name): missing initial image $($model.imageId)") }
            }
        } finally { $zip.Dispose() }
    }
    @{ app=$folder.Name; initial_image_models=$models.Count }
}
[xml]$codes = [IO.File]::ReadAllText((Join-Path $stage 'EngineCodes/EngineCodes.xml'))
foreach($app in @('BoostDeck','ShiftDeck')) {
    [xml]$description=[IO.File]::ReadAllText((Join-Path $stage "$app/$app.xml"))
    if($description.SelectNodes('//button[@id="410" or @id="411"]').Count){$failures.Add("${app}: manual unit options remain")}
    $workers=@(Get-ChildItem -LiteralPath (Join-Path $stage $app) -Filter '*-qnx' -File)
    if($app -eq 'ShiftDeck' -and $workers.Count){$failures.Add('ShiftDeck: obsolete worker remains')}
    if($app -eq 'BoostDeck' -and ($workers.Count -ne 1 -or $workers[0].Name -ne 'boostdeck-most-qnx')){$failures.Add('BoostDeck: MOST worker missing or obsolete worker remains')}
}
[xml]$hud=[IO.File]::ReadAllText((Join-Path $stage 'HudTest/HudTest.xml'))
if(-not $hud.SelectSingleNode('//checkbox[@model="51" and @action="21"]')){$failures.Add('HudTest: off/on checkbox missing')}
[xml]$xdrive=[IO.File]::ReadAllText((Join-Path $stage 'XDriveControl/XDriveControl.xml'))
if(-not $xdrive.SelectSingleNode('//hmiState[@id="101"]//button[@action="22"]') -or
   -not $xdrive.SelectSingleNode('//hmiState[@id="102"]//button[@action="23"]')){$failures.Add('XDriveControl: confirmation screens missing')}
if(Test-Path -LiteralPath (Join-Path $stage 'HudTest/hud-boost-most-qnx')){$failures.Add('HudTest: obsolete boost worker remains')}
foreach($pair in @(@('HudTest','hud-display-qnx'),@('XDriveControl','xdrive-control-qnx'))){
    if(-not (Test-Path -LiteralPath (Join-Path $stage "$($pair[0])/$($pair[1])"))){$failures.Add("Missing optional worker: $($pair[1])")}
}
if (-not $codes.SelectSingleNode('//hmiState[@id="102"]//button[@action="27"]') -or
    -not $codes.SelectSingleNode('//button[@id="313"]')) { $failures.Add('EngineCodes: clear confirmation or menu missing') }
$report = [ordered]@{
    firmware=([IO.File]::ReadAllText((Join-Path $EvoDirectory 'opt/sys/etc/nbt_version.txt'))).Trim()
    frameworks=@($frameworks); libraries=@($libraries); resources=@($resources)
    dsi_library_sha256=(Get-FileHash -LiteralPath $dsiPath -Algorithm SHA256).Hash.ToLower()
    failures=@($failures.ToArray()); static_checks_passed=($failures.Count -eq 0)
    runtime_verified=$false; target_tested=$false
    limitations=@('System libraries from /lib, /usr/lib and /boot/lib are missing from the supplied copy.',
        'Symbol presence does not prove object layouts or runtime behavior.',
        'Resource check covers initial image models; runtime-selected image ranges require target checks.',
        'Live read-only LayerManager query confirmed 1280x480; UI behavior still needs target tests.',
        'BoostDeck B58 diagnostic scaling has not been validated on N20B20.')
}
$report | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 (Join-Path $output 'static-checks.json')
if ($failures.Count) { throw ($failures -join "`n") }
Write-Host 'BMW imports, initial image resources and clear confirmation verified. Runtime and target checks remain open.'
