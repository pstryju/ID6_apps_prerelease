# Batch deployment helpers. Dot-source after EvoDeploy.ps1.
function Get-EvoInventory([string[]]$Apps) {
    $commands=@('set -e', "echo EVO_FIRMWARE=`$(cat /opt/sys/etc/nbt_version.txt)",
        "test -d '$script:Plugins'", "test ! -L '$script:Plugins'", "test ! -L '$script:Runs'",
        'LD_LIBRARY_PATH=/opt/sys/lib:/opt/hmi/ID5/lib:/lib:/usr/lib:/boot/lib /opt/sys/bin/LayerManagerControl get screen 0 || true')
    foreach ($directory in @($script:Plugins,'/opt/hmi/ID5/data/ro/Plugins') | Select-Object -Unique) {
        $commands += "for manifest in '$directory'/*/*.manifest; do if test -f `"`$manifest`"; then echo `"EVO_MANIFEST_BEGIN `$manifest`"; cat `"`$manifest`"; echo; echo EVO_MANIFEST_END; fi; done"
    }
    foreach ($app in $Apps) {
        $commands += "test ! -L '$script:Plugins/$app'"
        $commands += "if test -e '$script:Plugins/$app'; then test -d '$script:Plugins/$app'; echo EVO_ORIGINAL=$app; else echo EVO_NEW=$app; fi"
    }
    $text=Invoke-EvoScript ($commands -join "`n") -Activity 'Jeden odczyt: firmware, ekran i zainstalowane aplikacje'
    if ($text -notmatch '(?m)^EVO_FIRMWARE=NBTevo_Y24073J\s*$') { throw 'Unexpected target firmware.' }
    $manifests=@(foreach ($match in [regex]::Matches($text,'(?ms)^EVO_MANIFEST_BEGIN ([^\r\n]+)\r?\n(.*?)^EVO_MANIFEST_END\r?$')) {
        @{path=$match.Groups[1].Value; xml=[xml]$match.Groups[2].Value.Trim()}
    })
    $originals=@{}
    foreach ($app in $Apps) {
        if ($text -match "(?m)^EVO_ORIGINAL=$app\s*$") { $originals[$app]=$true }
        elseif ($text -match "(?m)^EVO_NEW=$app\s*$") { $originals[$app]=$false }
        else { throw "Missing target inventory for $app" }
    }
    return @{text=$text; manifests=$manifests; originals=$originals}
}

function New-EvoBundle([string]$Helper, [object[]]$Entries) {
    if ($Entries.Count -gt 128) { throw 'Bundle exceeds 128 files.' }
    $stream=New-Object IO.MemoryStream
    try {
        $bytes=[IO.File]::ReadAllBytes($Helper)
        $stream.Write($bytes,0,$bytes.Length)
        foreach ($entry in $Entries) {
            if ($entry.name -notmatch '^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$' -or
                @($entry.name.Split('/') | Where-Object { $_ -in @('.','..') }).Count) { throw 'Invalid bundle path.' }
            [byte[]]$data=$entry.bytes
            if ($data.Length -gt 32MB -or $stream.Length+$data.Length -gt 128MB) { throw 'Bundle size limit exceeded.' }
            $hash=Get-BytesHash $data
            $header=[Text.Encoding]::ASCII.GetBytes("FILE $($data.Length) $hash $($entry.name)`n")
            $stream.Write($header,0,$header.Length)
            $stream.Write($data,0,$data.Length)
        }
        $end=[Text.Encoding]::ASCII.GetBytes("END`n")
        $stream.Write($end,0,$end.Length)
        return ,$stream.ToArray()
    } finally { $stream.Dispose() }
}

function Get-EvoBatchBootstrap([string]$Run,[string]$Helper) {
    $length=(Get-Item -LiteralPath $Helper).Length
    $hash=Get-BytesHash ([IO.File]::ReadAllBytes($Helper))
    $bootstrap=@"
set -e
test ! -L '$script:Runs'
test ! -e '$Run'
mkdir -p '$Run'
echo 'Receiving helper and complete deployment bundle'
dd bs=1 count=$length of='$Run/fileops'
chmod 700 '$Run/fileops'
'$Run/fileops' verify '$Run/fileops' '$hash'
'$Run/fileops' receive '$Run'
sh '$Run/deploy.sh'
"@
    return $bootstrap.Replace("`r`n","`n").Replace("`r","`n")
}

function Get-EvoFrameworkChecks([string]$Helper) {
    $checks=@"
echo 'Checking target firmware and framework fingerprints'
'$Helper' text /opt/sys/etc/nbt_version.txt NBTevo_Y24073J
'$Helper' verify /opt/hmi/ID5/lib/libAppFramework.A.so ef2488b007d592301d8f21d9aa60771ad552d06b5e97c29e7813e3946644d820
'$Helper' verify /opt/hmi/ID5/lib/libCoreFramework.A.so cb0e488d1ebe2e30de87bb56f0e93d6a2d76b44309b0b47fef5b0b7cec048973
"@
    # Embedded in uploaded scripts, not just the SSH command argument. A CR
    # after the unquoted firmware token becomes part of argv on QNX.
    return $checks.Replace("`r`n","`n").Replace("`r","`n")
}

function Get-EvoInstallScript([string]$Run,[object[]]$Items) {
    $helper="$Run/fileops"
    $commands=@('set -e', (Get-EvoFrameworkChecks $helper),
        "mkdir -p '$Run/backup' '$Run/removed' '$Run/failed'", "mkdir '$script:Lock'",
        "trap `"'$helper' unlock '$script:Lock'`" 0", "trap 'exit 1' 1 2 15")
    foreach ($item in $Items) {
        $app=$item.name
        $commands += "echo 'Verify staged application: $app'"
        $commands += "test ! -L '$script:Plugins/$app'"
        if ($item.hadOriginal) { $commands += "test -d '$script:Plugins/$app'" }
        else { $commands += "test ! -e '$script:Plugins/$app'" }
        foreach ($file in $item.files) {
            $commands += "'$helper' verify '$Run/stage/$app/$($file.name)' '$($file.sha256)'"
            $mode=if ($file.name -like '*-qnx') { '755' } else { '644' }
            $commands += "chmod $mode '$Run/stage/$app/$($file.name)'"
        }
        $commands += "chmod 755 '$Run/stage/$app'"
    }
    # Final readback checks belong inside the transaction, before marking success
    # and unlocking, so any failure rolls the whole selected set back.
    $transaction=Get-EvoTransaction -Run $Run -Items $Items
    $verification=@("echo 'Verifying installed files in Plugins'")
    foreach ($item in $Items) { foreach ($file in $item.files) {
        $verification += "'$helper' verify '$script:Plugins/$($item.name)/$($file.name)' '$($file.sha256)'"
    } }
    $transaction=$transaction.Replace("echo 'installed' > '$Run/STATUS'",(($verification -join "`n")+"`necho 'installed' > '$Run/STATUS'"))
    $commands += "echo 'Backing up previous folders and activating applications'"
    $commands += $transaction
    $commands += "echo EVO_BATCH_INSTALLED"
    return ($commands -join "`n")
}

function Get-EvoUninstallScript([string]$Run,[object[]]$Items,[string]$Helper) {
    $commands=@('set -e',(Get-EvoFrameworkChecks $Helper),
        "test `"`$(cat '$Run/STATUS')`" = installed", "mkdir '$script:Lock'",
        "trap `"'$Helper' unlock '$script:Lock'`" 0", "trap 'exit 1' 1 2 15")
    foreach ($item in $Items) {
        $app=$item.name
        $commands += "echo 'Verify unchanged installed application: $app'"
        $commands += "test -d '$script:Plugins/$app' && test ! -L '$script:Plugins/$app' && test ! -e '$Run/removed/$app'"
        if ($item.hadOriginal) { $commands += "test -d '$Run/backup/$app' && test ! -L '$Run/backup/$app'" }
        # Count all files to retain the old refusal to replace a modified folder.
        $commands += "count=0; for file in '$script:Plugins/$app'/* '$script:Plugins/$app'/.[!.]* '$script:Plugins/$app'/..?*; do if test -e `"`$file`" || test -L `"`$file`"; then test -f `"`$file`"; test ! -L `"`$file`"; count=`$((count+1)); fi; done; test `"`$count`" = $(@($item.files).Count)"
        foreach ($file in $item.files) { $commands += "'$Helper' verify '$script:Plugins/$app/$($file.name)' '$($file.sha256)'" }
    }
    $commands += "echo 'Restoring previous application folders'"
    $commands += (Get-EvoTransaction -Run $Run -Items $Items -Uninstall)
    $commands += 'echo EVO_BATCH_UNINSTALLED'
    return ($commands -join "`n")
}
