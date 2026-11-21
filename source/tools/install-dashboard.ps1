[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [string]$Head = '169.254.199.119',
    [ValidateSet('BoostDeck','ShiftDeck','EngineCodes','HudTest','XDriveControl','LaunchDeck','RoadJet','BEAMer')]
    [string[]]$Apps = @('BoostDeck','ShiftDeck'),
    [ValidateSet('Core','All','Gauges','Extras','Activities')][string]$AppSet = 'Gauges',
    [ValidateSet('Legacy','Upstream','UpstreamCds')][string]$HudVariant = 'Legacy',
    [switch]$Parked,
    [switch]$Legacy,
    [string]$IdentityFile,
    [Security.SecureString]$Password,
    [string]$Gateway = '169.254.20.21',
    [ValidateSet('n20','auto','b48','n13')][string]$EngineProfile = 'n20',
    [int]$DisplayWidth = 0,
    [int]$DisplayHeight = 0
)
. "$PSScriptRoot/EvoDeploy.ps1"
if (-not $PSBoundParameters.ContainsKey('Apps')) {
    $Apps = switch ($AppSet) {
        'Gauges' { @('BoostDeck','ShiftDeck') }
        'Extras' { @('HudTest','XDriveControl','LaunchDeck','RoadJet','BEAMer') }
        'Activities' { @('LaunchDeck','RoadJet','BEAMer') }
        'Core' { @('ShiftDeck','EngineCodes') }
        'All' { @('BoostDeck','ShiftDeck','EngineCodes') }
    }
}
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output = Join-Path $root 'source/native/out/dashboard'
$candidate = Join-Path $output 'candidate'
$Apps = @($Apps | Select-Object -Unique)
if (-not $Apps.Count) { throw 'Select at least one app.' }
$helper = Join-Path $output 'gaugedeck-fileops'
if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) { throw 'Build first: source/tools/build-dashboard.ps1' }
$sums = @{}
foreach ($line in Get-Content -LiteralPath (Join-Path $output 'SHA256SUMS')) {
    if ($line -notmatch '^([a-f0-9]{64})  (.+)$') { throw 'Invalid SHA256SUMS.' }
    $sums[$matches[2]] = $matches[1]
}
$items = @()
$sourceFolders=@{}
Write-EvoStatus '[1/5] Sprawdzanie kompletnosci i sum lokalnych plikow'
foreach ($app in $Apps) {
    $folder = Join-Path $candidate $app
    $appSums=$sums
    if($app -eq 'HudTest' -and $HudVariant -ne 'Legacy'){
        $variantOutput=Join-Path $output "hud-variants/$HudVariant"
        $folder=Join-Path $variantOutput 'HudTest'
        if(-not (Test-Path -LiteralPath (Join-Path $variantOutput 'SHA256SUMS'))){throw 'Build HUD variants first: source/tools/build-hud-upstream.ps1'}
        $appSums=@{}
        foreach($line in Get-Content -LiteralPath (Join-Path $variantOutput 'SHA256SUMS')){
            if($line -notmatch '^([a-f0-9]{64})  (.+)$'){throw 'Invalid HUD variant SHA256SUMS'}
            $appSums[$matches[2]]=$matches[1]
        }
    }
    $sourceFolders[$app]=$folder
    [xml]$manifest = [IO.File]::ReadAllText((Join-Path $folder "$app.manifest"))
    $connector = [int]$manifest.plugin.config.dependencies.pluginAppConnector.id
    $files = @(foreach ($file in Get-ChildItem -LiteralPath $folder -File) {
        $relative = "$app/$($file.Name)"
        if ($file.Name -notmatch '^[A-Za-z0-9_.-]+$' -or -not $appSums.ContainsKey($relative)) { throw "Unlisted file: $relative" }
        $hash = Get-BytesHash ([IO.File]::ReadAllBytes($file.FullName))
        if ($hash -ne $appSums[$relative]) { throw "Local checksum mismatch: $relative" }
        @{ name=$file.Name; sha256=$hash }
    })
    $expectedNames=@("$app.manifest", "$app.xml", "lib$app.so")
    if ($app -notin @('HudTest','XDriveControl')) {
        $worker = switch ($app) {
            'BoostDeck' { 'boostdeck-most-qnx' }
            'ShiftDeck' { 'shift-deck-qnx' }
            'EngineCodes' { 'engine-codes-qnx' }
            'LaunchDeck' { 'shift-deck-qnx' }
            'RoadJet' { 'road-jet-qnx' }
            'BEAMer' { 'beamer-qnx' }
        }
        $expectedNames += @("${app}_ALL_Images.zip")
        if($app -in @('BoostDeck','EngineCodes','LaunchDeck','RoadJet','BEAMer')){$expectedNames += $worker}
    }
    if($app -eq 'HudTest'){
        $hudWorker=if($HudVariant -eq 'Legacy'){'hud-display-qnx'}else{'hud-shape-qnx'}
        $expectedNames+=@($hudWorker,'LICENSE.txt','NOTICE.txt')
    }
    if($app -eq 'XDriveControl'){$expectedNames+=@('xdrive-control-qnx','LICENSE.txt')}
    if (@(Compare-Object @($files | ForEach-Object { $_.name }) $expectedNames).Count) { throw "Incomplete or unexpected candidate files: $app" }
    $items += @{ name=$app; connector=$connector; files=$files; hadOriginal=$false }
    Write-EvoStatus "Lokalne pliki OK: $app ($($files.Count) plikow)"
}
. "$PSScriptRoot/EvoBatch.ps1"
$gatewayAddress=$null
if (-not [Net.IPAddress]::TryParse($Gateway,[ref]$gatewayAddress) -or $gatewayAddress.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork) { throw 'Gateway must be an IPv4 address.' }
$Gateway=$gatewayAddress.ToString()
Write-Host "Apps: $($Apps -join ', '). Two SSH sessions; no automatic restart."
if('HudTest' -in $Apps){Write-Host "HUD variant: $HudVariant (replaces the existing HudTest app)"}
if(@($Apps | Where-Object {$_ -in @('EngineCodes','XDriveControl','LaunchDeck','RoadJet','BEAMer')}).Count){Write-Host "Diagnostic gateway: $Gateway"}
if('BoostDeck' -in $Apps){Write-Host "Engine profile: $EngineProfile (B48/N13 mappings await validation)"}
if (-not $PSCmdlet.ShouldProcess($Head, "Install $($Apps -join ', ') using one verified bundle")) { return }
if (-not $Parked -and (Read-Host 'Auto stoi, iDrive jest wlaczone? Wpisz TAK, aby kontynuowac') -cne 'TAK') { throw 'Installation cancelled.' }
Write-EvoStatus '[2/5] Jeden odczyt konfiguracji i zainstalowanych aplikacji'
Initialize-EvoConnection -Head $Head -Legacy:$Legacy -IdentityFile $IdentityFile -Password $Password
$inventory=Get-EvoInventory -Apps $Apps
if ($Apps.Count) {
    if ($inventory.text -match 'resolution:\s*x\s*=\s*(\d+),\s*y\s*=\s*(\d+)') {
        $DisplayWidth=[int]$matches[1]; $DisplayHeight=[int]$matches[2]
    }
    if ($DisplayWidth -ne 1280 -or $DisplayHeight -ne 480) { throw 'Expected 1280x480; supply verified -DisplayWidth and -DisplayHeight if the query is unavailable.' }
}
foreach ($item in $items) {
    $item.hadOriginal=$inventory.originals[$item.name]
    foreach ($installed in $inventory.manifests) {
        $sameFolder=$installed.path.StartsWith("$script:Plugins/$($item.name)/",[StringComparison]::Ordinal)
        $idNode=$installed.xml.SelectSingleNode('//pluginAppConnector/id')
        $nameNode=$installed.xml.SelectSingleNode('/plugin/name')
        if (-not $sameFolder -and (($idNode -and [int]$idNode.InnerText -eq $item.connector) -or ($nameNode -and $nameNode.InnerText -eq $item.name))) { throw "Conflicting name or connector: $($installed.path)" }
    }
}
Write-EvoStatus '[3/5] Przygotowanie jednej paczki aplikacji'
$runId=[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,12)
$run="$script:Runs/$runId"
Write-EvoStatus "Deployment ID: $runId"
$entries=@()
foreach ($item in $items) {
    foreach ($file in $item.files) {
        $entries+=@{name="stage/$($item.name)/$($file.name)"; bytes=[IO.File]::ReadAllBytes((Join-Path $sourceFolders[$item.name] $file.name))}
    }
    if ($item.name -in @('EngineCodes','XDriveControl','LaunchDeck','RoadJet','BEAMer') -or
        ($item.name -eq 'HudTest' -and $HudVariant -eq 'Upstream')) {
        $bytes=[Text.Encoding]::ASCII.GetBytes($Gateway+"`n")
        $item.files+=@{name='gateway-ip.txt';sha256=(Get-BytesHash $bytes)}
        $entries+=@{name="stage/$($item.name)/gateway-ip.txt";bytes=$bytes}
    }
    if ($item.name -eq 'BoostDeck') {
        $bytes=[Text.Encoding]::ASCII.GetBytes($EngineProfile+"`n")
        $item.files+=@{name='engine-profile.txt';sha256=(Get-BytesHash $bytes)}
        $entries+=@{name="stage/$($item.name)/engine-profile.txt";bytes=$bytes}
    }
}
$receipt=[ordered]@{schema=1;runId=$runId;head=$Head;state='Prepared';remoteRun=$run;gateway=$Gateway;engineProfile=$EngineProfile;hudVariant=$HudVariant;apps=$items}
$receiptDir=Get-EvoReceiptDirectory
New-Item -ItemType Directory -Force $receiptDir | Out-Null
$receiptPath=Join-Path $receiptDir "$runId.json"
$receiptBytes=[Text.Encoding]::UTF8.GetBytes(($receipt | ConvertTo-Json -Depth 8))
[IO.File]::WriteAllBytes($receiptPath,$receiptBytes)
$entries+=@{name='receipt.json';bytes=$receiptBytes}
$deploy=Get-EvoInstallScript -Run $run -Items $items
$entries+=@{name='deploy.sh';bytes=[Text.Encoding]::UTF8.GetBytes($deploy.Replace("`r`n","`n")+"`n")}
$payload=New-EvoBundle -Helper $helper -Entries $entries
Write-EvoStatus "Paczka: $($entries.Count) plikow, $($payload.Length) B; SHA256 i kopie sprawdzane na Evo"
Write-EvoStatus '[4/5] Jedno SSH: wysylanie, weryfikacja i instalacja na Evo'
try {
    $result=Invoke-Evo -Command (Get-EvoBatchBootstrap -Run $run -Helper $helper) -InputBytes $payload -StreamText -Activity 'Cala paczka i transakcja na Evo'
    if ($result -notmatch '(?m)^EVO_BATCH_INSTALLED\s*$') { throw 'Missing final installation confirmation.' }
    $receipt.state='Installed'
    Write-EvoStatus '[5/5] Zapis potwierdzenia instalacji'
    [IO.File]::WriteAllText($receiptPath,($receipt | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    Write-Host "Installed and verified. Receipt: $receiptPath"
    Write-Host "Backup on Evo: $run/backup"
    Write-Host 'Restart iDrive manually while parked (hold the volume knob).'
} catch {
    Write-Warning "Inspect STATUS for deployment $runId if the connection was interrupted. Recovery files: $run"
    throw
}
