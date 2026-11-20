param(
    [string]$EvoDirectory = 'C:\Users\pawel\evo',
    [string]$QnxDirectory = 'C:\Users\pawel\Developer\qnx650-docker',
    [string]$Image = 'qnx650-sdk'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$docker = Join-Path $env:LOCALAPPDATA 'Programs\DockerDesktop\resources\bin\docker.exe'
$license = Join-Path $QnxDirectory '.local-qnx\config\license\licenses'
foreach ($path in @($EvoDirectory, $license, $docker)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing: $path" }
}
# ARM GCC 4.4.2 is a 32-bit host program; compile on the container's own
# filesystem to avoid EOVERFLOW on Docker Desktop bind-mount inode numbers.
$command = 'set -eu; cp /tmp/qnx-license /etc/qnx/license/licenses; cp -r /work/source/native /tmp/native; mkdir /tmp/frameworks; cp /evo/opt/hmi/ID5/lib/libAppFramework.A.so /evo/opt/hmi/ID5/lib/libCoreFramework.A.so /tmp/frameworks/; cd /tmp/native; NBTEVO_FRAMEWORKS_DIR=/tmp/frameworks sh build-dashboard.sh; mkdir -p /work/source/native/out; cp -r out/dashboard /work/source/native/out/'
& $docker run --rm --network none `
    --mount "type=bind,source=$root,target=/work" `
    --mount "type=bind,source=$EvoDirectory,target=/evo,readonly" `
    --mount "type=bind,source=$license,target=/tmp/qnx-license,readonly" `
    $Image sh -c $command
if ($LASTEXITCODE -ne 0) { throw "QNX build failed: $LASTEXITCODE" }
$native = Join-Path $root 'source/native'
$output = Join-Path $native 'out/dashboard'
$stage = Join-Path $output 'candidate'
# Retire only known generated test files. No recursion.
foreach($app in @('AbiSmoke','CdsProbe')) {
    foreach($relative in @("candidate/$app/$app.manifest","candidate/$app/$app.xml",
        "candidate/$app/lib$app.so","lib$app.so")) {
        $retired=Join-Path $output $relative
        if(Test-Path -LiteralPath $retired -PathType Leaf){Remove-Item -LiteralPath $retired}
    }
    $retiredFolder=Join-Path $stage $app
    if(Test-Path -LiteralPath $retiredFolder -PathType Container){
        if(@(Get-ChildItem -LiteralPath $retiredFolder -Force).Count){throw "Unexpected files in retired generated folder: $retiredFolder"}
        Remove-Item -LiteralPath $retiredFolder
    }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $root 'packages/dashboard.zip'))
try {
    foreach ($app in @('BoostDeck', 'ShiftDeck', 'EngineCodes', 'HudTest', 'XDriveControl', 'LaunchDeck', 'RoadJet', 'BEAMer')) {
        $folder = Join-Path $stage $app
        New-Item -ItemType Directory -Force $folder | Out-Null
        foreach ($extension in @('manifest', 'xml')) {
            $resource="plugin/$app.$extension"
            Copy-Item -LiteralPath (Join-Path $native $resource) -Destination (Join-Path $folder "$app.$extension")
        }
        $library = Join-Path $output "lib$app.so"
        $bytes = [IO.File]::ReadAllBytes($library)
        if ($bytes[0] -ne 127 -or $bytes[1] -ne 69 -or $bytes[2] -ne 76 -or $bytes[3] -ne 70 -or
            $bytes[4] -ne 1 -or $bytes[5] -ne 1 -or [BitConverter]::ToUInt16($bytes, 16) -ne 3 -or
            [BitConverter]::ToUInt16($bytes, 18) -ne 40) { throw "Invalid ARM ELF library: $library" }
        Copy-Item -LiteralPath $library -Destination $folder
        if ($app -notin @('HudTest','XDriveControl')) {
            $appArchive=$archive
            $package=switch($app){'LaunchDeck'{'launchdeck.zip'} 'RoadJet'{'road-jet.zip'} 'BEAMer'{'beamer.zip'}}
            if($package){$appArchive=[IO.Compression.ZipFile]::OpenRead((Join-Path $root "packages/$package"))}
            try {
                $entry = $appArchive.GetEntry("$app/${app}_ALL_Images.zip")
                if (-not $entry) { throw "Missing image archive: $app" }
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, (Join-Path $folder "${app}_ALL_Images.zip"), $true)
            } finally { if($package){$appArchive.Dispose()} }
        }
    }
} finally { $archive.Dispose() }
foreach ($pair in @(@('EngineCodes','engine-codes-qnx'),@('BoostDeck','boostdeck-most-qnx'),@('XDriveControl','xdrive-control-qnx'),@('HudTest','hud-display-qnx'),@('LaunchDeck','shift-deck-qnx'),@('RoadJet','road-jet-qnx'),@('BEAMer','beamer-qnx'))) {
    $worker = Join-Path $output $pair[1]
    $bytes = [IO.File]::ReadAllBytes($worker)
    if ([BitConverter]::ToUInt16($bytes,16) -ne 2 -or [BitConverter]::ToUInt16($bytes,18) -ne 40) {
        throw "Invalid ARM ELF executable: $worker"
    }
    Copy-Item -LiteralPath $worker -Destination (Join-Path $stage $pair[0])
}
$retiredHudBoost=Join-Path $stage 'HudTest/hud-boost-most-qnx'
if(Test-Path -LiteralPath $retiredHudBoost -PathType Leaf){Remove-Item -LiteralPath $retiredHudBoost}
# Remove only the two obsolete generated workers from the staged CDS apps.
foreach ($relative in @('BoostDeck/boostdeck-collector-qnx','ShiftDeck/shift-deck-qnx')) {
    $oldWorker=Join-Path $stage $relative
    if(Test-Path -LiteralPath $oldWorker){Remove-Item -LiteralPath $oldWorker}
}
$hudLicense=[IO.File]::ReadAllText((Join-Path $root 'source/LICENSE'))+"`n`n"+[IO.File]::ReadAllText((Join-Path $root 'source/third-party/hud-LICENSE.md'))
[IO.File]::WriteAllText((Join-Path $stage 'HudTest/LICENSE.txt'),$hudLicense)
Copy-Item -LiteralPath (Join-Path $root 'source/third-party/hud-NOTICE.txt') -Destination (Join-Path $stage 'HudTest/NOTICE.txt')
$xdriveLicense=[IO.File]::ReadAllText((Join-Path $root 'source/LICENSE'))+"`n`n"+[IO.File]::ReadAllText((Join-Path $root 'source/third-party/idrive7-apps-LICENSE'))
[IO.File]::WriteAllText((Join-Path $stage 'XDriveControl/LICENSE.txt'),$xdriveLicense)
$background=Join-Path $output 'stock-background.png'
& "$PSScriptRoot/make-stock-background.ps1" -OutputPath $background -NoCaptions
$imagesPath=Join-Path $stage 'BoostDeck/BoostDeck_ALL_Images.zip'
$images=[IO.Compression.ZipFile]::Open($imagesPath,[IO.Compression.ZipArchiveMode]::Update)
try {
    $images.GetEntry('56300.png').Delete()
    $null=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($images,$background,'56300.png',[IO.Compression.CompressionLevel]::Optimal)
} finally{$images.Dispose()}
# The reference XML retains manual units; this CDS candidate follows Evo.
foreach($app in @('BoostDeck','ShiftDeck')) {
    $path=Join-Path $stage "$app/$app.xml"
    [xml]$description=[IO.File]::ReadAllText($path)
    foreach($node in @($description.SelectNodes('//button[@id="410" or @id="411"]'))) {
        $null=$node.ParentNode.RemoveChild($node)
    }
    $ids=if($app -eq 'BoostDeck'){@('49','59')}else{@('46','47')}
    foreach($id in $ids){
        foreach($node in @($description.SelectNodes("//raAction[@id='$id']"))){$null=$node.ParentNode.RemoveChild($node)}
    }
    $description.Save($path)
}
$sums = foreach ($file in Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName) {
    $relative = $file.FullName.Substring($stage.Length + 1).Replace('\','/')
    '{0}  {1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLower(), $relative
}
[IO.File]::WriteAllLines((Join-Path $output 'SHA256SUMS'), [string[]]$sums)
Write-Host 'Candidate folders: source/native/out/dashboard/candidate. No installation performed.'
