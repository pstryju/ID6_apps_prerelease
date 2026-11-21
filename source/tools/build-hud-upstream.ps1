param(
    [string]$EvoDirectory='C:\Users\pawel\evo',
    [string]$QnxDirectory='C:\Users\pawel\Developer\qnx650-docker',
    [string]$NbtAadDirectory='C:\Users\pawel\Developer\Supra-iDrive6-Android-Auto',
    [string]$NbtAadRevision='7aec566e63182043371a484f04487f396197788d',
    [string]$Image='qnx650-sdk'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$docker=Join-Path $env:LOCALAPPDATA 'Programs/DockerDesktop/resources/bin/docker.exe'
$license=Join-Path $QnxDirectory '.local-qnx/config/license/licenses'
foreach($path in @($EvoDirectory,$license,$docker,(Join-Path $NbtAadDirectory '.git'))){
    if(-not (Test-Path -LiteralPath $path)){throw "Missing: $path"}
}
$git=Join-Path $env:LOCALAPPDATA 'Fork/gitInstance/2.50.1/cmd/git.exe'
if(-not (Test-Path -LiteralPath $git)){$git=(Get-Command git -ErrorAction Stop).Source}
$snapshot=Join-Path $root '.local/hud-upstream-aad.tar'
New-Item -ItemType Directory -Force (Split-Path -Parent $snapshot) | Out-Null
# Pin the last dependency revision with checkNav/navTexts/navBuffers used by main.
# Export into this workspace; do not change the neighboring repository.
& $git -c "safe.directory=$($NbtAadDirectory.Replace('\','/'))" -C $NbtAadDirectory archive --format=tar --output $snapshot "${NbtAadRevision}:milestone-7/src"
if($LASTEXITCODE -ne 0){throw 'Could not export the required licensed dependency revision'}
$command='set -eu; cp /tmp/qnx-license /etc/qnx/license/licenses; cp -r /work/source/native /tmp/native; mkdir /tmp/aad; tar -xf /tmp/aad.tar -C /tmp/aad; mkdir /tmp/frameworks; cp /evo/opt/hmi/ID5/lib/libAppFramework.A.so /evo/opt/hmi/ID5/lib/libCoreFramework.A.so /tmp/frameworks/; cd /tmp/native; NBTEVO_FRAMEWORKS_DIR=/tmp/frameworks NBTAAD_SOURCE=/tmp/aad sh build-hud-upstream-cds.sh; mkdir -p /work/source/native/out/hud-upstream-cds; cp out/hud-upstream-cds/libHudTest.so out/hud-upstream-cds/hud-shape-qnx /work/source/native/out/hud-upstream-cds/'
& $docker run --rm --network none --mount "type=bind,source=$root,target=/work" `
    --mount "type=bind,source=$EvoDirectory,target=/evo,readonly" `
    --mount "type=bind,source=$snapshot,target=/tmp/aad.tar,readonly" `
    --mount "type=bind,source=$license,target=/tmp/qnx-license,readonly" $Image sh -c $command
if($LASTEXITCODE -ne 0){throw "HUD build failed: $LASTEXITCODE"}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$output=Join-Path $root 'source/native/out/dashboard/hud-variants'
$archive=[IO.Compression.ZipFile]::OpenRead((Join-Path $root 'packages/hud-gauges.zip'))
try {
    foreach($variant in @('Upstream','UpstreamCds')){
        $stage=Join-Path $output "$variant/HudTest"
        New-Item -ItemType Directory -Force $stage | Out-Null
        foreach($name in @('HudTest.manifest','HudTest.xml','libHudTest.so','hud-shape-qnx')){
            $entry=$archive.GetEntry("HudTest/$name")
            if(-not $entry){throw "Missing upstream file: $name"}
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,(Join-Path $stage $name),$true)
        }
        foreach($pair in @(@('LICENSE.md','LICENSE.txt'),@('NOTICE.txt','NOTICE.txt'))){
            $entry=$archive.GetEntry($pair[0]);if(-not $entry){throw "Missing $($pair[0])"}
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,(Join-Path $stage $pair[1]),$true)
        }
        if($variant -eq 'UpstreamCds'){
            foreach($name in @('libHudTest.so','hud-shape-qnx')){
                Copy-Item -LiteralPath (Join-Path $root "source/native/out/hud-upstream-cds/$name") -Destination $stage
            }
        }
        $sums=foreach($file in Get-ChildItem -LiteralPath $stage -File | Sort-Object Name){
            '{0}  HudTest/{1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLower(),$file.Name
        }
        [IO.File]::WriteAllLines((Join-Path $output "$variant/SHA256SUMS"),[string[]]$sums)
    }
} finally {$archive.Dispose()}
$evidence=[ordered]@{renderer_sha256=(Get-FileHash (Join-Path $root 'source/native/hud/hud_shape.cpp')).Hash;package_sha256=(Get-FileHash (Join-Path $root 'packages/hud-gauges.zip')).Hash;nbtaad_revision=$NbtAadRevision;dependency_snapshot_sha256=(Get-FileHash $snapshot).Hash}
$evidence | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $output 'build-evidence.json')
Write-Host 'Prepared Upstream (unchanged) and UpstreamCds (CDS RPM; boost unavailable). No installation performed.'
