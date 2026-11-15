param([string]$EvoDirectory='C:\Users\pawel\evo')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$path=Join-Path $EvoDirectory 'opt/hmi/ID5/data/ro/hmiplugins/libhmiremoting.so'
$content=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($path))
$strings=@([regex]::Matches($content,'[ -~]{5,}') | ForEach-Object {$_.Value})
$names=@($strings | Where-Object {$_ -cmatch '^[a-z]+\.[A-Za-z][A-Za-z0-9]*$'} | Sort-Object -Unique)
$classes=@([regex]::Matches($content,'CCdsProperty[A-Za-z0-9]+') | ForEach-Object {$_.Value.TrimEnd('E')} | Sort-Object -Unique)
$report=[ordered]@{
    source='opt/hmi/ID5/data/ro/hmiplugins/libhmiremoting.so'
    sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower()
    property_names=$names
    engine_names=@($names | Where-Object {$_ -clike 'engine.*'})
    candidate_related_classes=@($classes | Where-Object {$_ -match 'Engine|Pressure|Temperature|Torque|Fuel|Transmission|Boost|Lambda|Timing'})
    limitations=@('String inventory is not a complete reconstruction of the factory registry.',
        'Missing names are not proof that no internal vehicle service exposes a measurement.',
        'Dynamic api.* properties and non-CDS services require separate analysis.',
        'No network or vehicle runtime queries are performed.')
}
$out=Join-Path $root '.local/cds-audit'
New-Item -ItemType Directory -Force $out | Out-Null
$report | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $out 'inventory.json')
Write-Host "Found $($names.Count) property-like names. Report: .local/cds-audit/inventory.json"
$report.engine_names
