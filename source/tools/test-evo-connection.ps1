[CmdletBinding()]
param(
    [string]$Head = '169.254.199.119',
    [switch]$Legacy,
    [string]$IdentityFile,
    [Security.SecureString]$Password
)
. "$PSScriptRoot/EvoDeploy.ps1"
Initialize-EvoConnection -Head $Head -Legacy:$Legacy -IdentityFile $IdentityFile -Password $Password
$result = Invoke-Evo -Command 'cat /opt/sys/etc/nbt_version.txt'
Write-Host "SSH OK: $result"
Write-Host 'Read-only check completed. No installation or restart performed.'
