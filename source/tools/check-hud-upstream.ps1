param(
    [string]$EvoDirectory='C:\Users\pawel\evo',
    [string]$ReadElf='C:\Users\pawel\Developer\qnx650-docker\.local-qnx\research\ntoarmv7-readelf.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output=Join-Path $root 'source/native/out/dashboard/hud-variants'
$exports=@{}
foreach($name in @('libAppFramework.A.so','libCoreFramework.A.so')){
    foreach($line in (& $ReadElf -s -D -W (Join-Path $EvoDirectory "opt/hmi/ID5/lib/$name"))){
        if($line -match '\s(?:GLOBAL|WEAK)\s+DEFAULT\s+(?!UND)\S+\s+(\S+)'){$exports[$matches[1]]=$true}
    }
    if($LASTEXITCODE -ne 0){throw 'Cannot inspect target framework exports'}
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead((Join-Path $root 'packages/hud-gauges.zip'))
try {
    foreach($variant in @('Upstream','UpstreamCds')){
        $stage=Join-Path $output "$variant/HudTest"
        $files=@(Get-ChildItem -LiteralPath $stage -File)
        if($files.Count -ne 6){throw "Unexpected files in $variant"}
        $sums=@{}
        foreach($line in Get-Content (Join-Path $output "$variant/SHA256SUMS")){
            if($line -notmatch '^([a-f0-9]{64})  (.+)$'){throw 'Invalid sums'}
            $sums[$matches[2]]=$matches[1]
        }
        foreach($file in $files){
            if((Get-FileHash -LiteralPath $file.FullName).Hash.ToLower() -ne $sums["HudTest/$($file.Name)"]){throw "Checksum mismatch: $file"}
        }
        if($variant -eq 'Upstream'){
            foreach($name in @('HudTest.manifest','HudTest.xml','libHudTest.so','hud-shape-qnx')){
                $stream=$archive.GetEntry("HudTest/$name").Open()
                try {$hash=[Security.Cryptography.SHA256]::Create();try{$original=[BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','')}finally{$hash.Dispose()}}finally{$stream.Dispose()}
                if($original -ne (Get-FileHash (Join-Path $stage $name)).Hash){throw "Original package changed: $name"}
            }
        }
        foreach($name in @('libHudTest.so','hud-shape-qnx')){
            $path=Join-Path $stage $name;$bytes=[IO.File]::ReadAllBytes($path)
            $kind=if($name -like '*.so'){3}else{2}
            if($bytes[0] -ne 127 -or $bytes[1] -ne 69 -or $bytes[2] -ne 76 -or $bytes[3] -ne 70 -or
               $bytes[4] -ne 1 -or $bytes[5] -ne 1 -or [BitConverter]::ToUInt16($bytes,16) -ne $kind -or [BitConverter]::ToUInt16($bytes,18) -ne 40){throw "Invalid QNX ARM ELF: $name"}
            $symbols=@(& $ReadElf -s -D -W $path)
            if($LASTEXITCODE -ne 0){throw "Cannot inspect $path"}
            foreach($line in $symbols){
                if($line -match '\sUND\s+(\S*bmw\S*)' -and -not $exports.ContainsKey($matches[1])){throw "Missing BMW import: $($matches[1])"}
            }
            if($variant -eq 'UpstreamCds' -and @($symbols | Where-Object {$_ -match '\sUND\s+connect$'}).Count){throw 'Unexpected TCP connect in CDS HUD'}
        }
        [xml]$layout=[IO.File]::ReadAllText((Join-Path $stage 'HudTest.xml'))
        if(-not $layout.SelectSingleNode('//checkbox[@model="51" and @action="21"]')){throw 'Missing upstream checkbox'}
        Write-Host "$variant`: checksums, ARM ELF, BMW imports and checkbox OK."
    }
} finally {$archive.Dispose()}
Write-Host 'Static checks only; navigation handover and target runtime remain unverified.'
