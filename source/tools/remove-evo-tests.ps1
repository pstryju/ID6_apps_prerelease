[CmdletBinding(SupportsShouldProcess=$true)]
param([string]$Head='169.254.199.119',[switch]$Parked,[Security.SecureString]$Password)
. "$PSScriptRoot/EvoDeploy.ps1"
$directory=Get-EvoReceiptDirectory
$receipts=@(Get-ChildItem -LiteralPath $directory -Filter '*.json' -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | ForEach-Object {Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json})
foreach($name in @('AbiSmoke','CdsProbe')) {
    $receipt=@($receipts | Where-Object {
        $_.head -eq $Head -and $_.state -eq 'Installed' -and
        @($_.apps).Count -eq 1 -and $_.apps[0].name -eq $name
    } | Select-Object -First 1)
    if(-not $receipt.Count){Write-Host "No standalone installed receipt for $name; skipping.";continue}
    if($receipt[0].apps[0].hadOriginal){throw "$name has an older app backup. Refusing to restore it as a removal."}
    Write-Host "Remove retired test $name using deployment $($receipt[0].runId)"
    & "$PSScriptRoot/uninstall-dashboard.ps1" -Head $Head -RunId $receipt[0].runId `
        -Parked:$Parked -Password $Password -WhatIf:$WhatIfPreference
}
