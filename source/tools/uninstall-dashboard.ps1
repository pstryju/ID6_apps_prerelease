[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [string]$Head = '169.254.199.119',
    [string]$RunId,
    [switch]$Parked,
    [switch]$Legacy,
    [string]$IdentityFile,
    [Security.SecureString]$Password
)
. "$PSScriptRoot/EvoDeploy.ps1"
. "$PSScriptRoot/EvoBatch.ps1"
$receiptDir = Get-EvoReceiptDirectory
if (-not $RunId) {
    $matchesHead = @(Get-ChildItem -LiteralPath $receiptDir -Filter '*.json' -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | ForEach-Object {
            $entry = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
            if ($entry.head -eq $Head -and $entry.state -eq 'Installed') { $entry }
        })
    if (-not $matchesHead.Count) { throw 'No local receipt. Supply -RunId from the installation output.' }
    $RunId=$matchesHead[0].runId
}
Assert-RunId $RunId
Write-Host "Restore deployment $RunId on $Head. Removed candidates and backups are retained. No automatic restart."
if (-not $PSCmdlet.ShouldProcess($Head, "Uninstall deployment $RunId and restore its previous app folders")) { return }
if (-not $Parked -and (Read-Host 'Auto stoi, iDrive jest wlaczone? Wpisz TAK, aby kontynuowac') -cne 'TAK') { throw 'Uninstallation cancelled.' }
Write-EvoStatus '[1/5] Logowanie SSH oraz sprawdzanie jednostki'
Initialize-EvoConnection -Head $Head -Legacy:$Legacy -IdentityFile $IdentityFile -Password $Password

Write-EvoStatus '[2/5] Odczyt potwierdzenia i statusu instalacji'
$run="$script:Runs/$RunId"
$record=Invoke-Evo -Command "cat '$run/receipt.json'; echo; echo EVO_STATUS_BEGIN; if test -f '$run/STATUS'; then cat '$run/STATUS'; else echo incomplete; fi" -Activity 'Odczyt potwierdzenia i statusu w jednym SSH'
$parts=$record -split '(?m)^EVO_STATUS_BEGIN\r?$',2
if ($parts.Count -ne 2) { throw 'Invalid deployment response.' }
$receipt=$parts[0].Trim() | ConvertFrom-Json
$status=$parts[1].Trim()
if ($receipt.schema -ne 1 -or $receipt.runId -ne $RunId) { throw 'Receipt does not match this deployment.' }
$items=@($receipt.apps)
if (-not $items.Count -or $items.Count -gt 7) { throw 'Invalid receipt app count.' }
$names=@{}
foreach ($item in $items) {
    # AbiSmoke and CdsProbe remain accepted only to restore/remove historical deployments.
    if ($item.name -notin @('AbiSmoke','BoostDeck','ShiftDeck','EngineCodes','CdsProbe','HudTest','XDriveControl') -or $names.ContainsKey($item.name)) { throw 'Invalid app in receipt.' }
    $names[$item.name]=$true
    if ($item.hadOriginal -isnot [bool] -or -not @($item.files).Count) { throw 'Invalid original-app record.' }
    foreach ($file in $item.files) {
        if ($file.name -notmatch '^[A-Za-z0-9_.-]+$' -or $file.name -in @('.','..') -or $file.sha256 -notmatch '^[a-f0-9]{64}$') { throw 'Invalid file in receipt.' }
    }
}

if ($status -eq 'uninstalled' -or $status -eq 'rolledback') { Write-Host 'Deployment is already removed or rolled back.'; return }
if ($status -ne 'installed') { throw "Deployment is incomplete. Inspect $run before changing Plugins." }
Write-EvoStatus '[3/5] Przygotowanie jednej paczki deinstalacyjnej'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$helper=Join-Path $root 'source/native/out/dashboard/gaugedeck-fileops'
if (-not (Test-Path -LiteralPath $helper)) { throw 'Build the current fileops helper first.' }
$operation="$run/uninstall-$([Guid]::NewGuid().ToString('N'))"
$script=Get-EvoUninstallScript -Run $run -Items $items -Helper "$operation/fileops"
$entries=@(@{name='deploy.sh';bytes=[Text.Encoding]::UTF8.GetBytes($script.Replace("`r`n","`n")+"`n")})
$payload=New-EvoBundle -Helper $helper -Entries $entries
Write-EvoStatus '[4/5] Jedno SSH: weryfikacja plikow i przywracanie kopii na Evo'
try {
    $result=Invoke-Evo -Command (Get-EvoBatchBootstrap -Run $operation -Helper $helper) -InputBytes $payload -StreamText -Activity 'Weryfikacja i deinstalacja na Evo'
    if ($result -notmatch '(?m)^EVO_BATCH_UNINSTALLED\s*$') { throw 'Missing final uninstall confirmation.' }
    Write-EvoStatus '[5/5] Zapis lokalnego potwierdzenia deinstalacji'
    $receipt.state='Uninstalled'
    New-Item -ItemType Directory -Force $receiptDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $receiptDir "$RunId.json"),($receipt | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    Write-Host "Removed candidates and restored previous folders. Retained files: $run"
    Write-Host 'Restart iDrive manually while parked (hold the volume knob).'
} catch {
    Write-Warning "Inspect STATUS for $run if SSH was interrupted. Recovery files: $operation"
    throw
}
