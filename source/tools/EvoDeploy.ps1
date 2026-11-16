# Shared by install-dashboard.ps1 and uninstall-dashboard.ps1. No work on import.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:Plugins = '/var/opt/hmi/data/ID5/Plugins'
$script:Runs = '/var/opt/hmi/data/ID5/CodexAppTests'
$script:Lock = '/var/opt/hmi/data/ID5/CodexAppTests.lock'
$script:EvoSshNumber = 0

function Write-EvoStatus([string]$Message) {
    Write-Host ('[{0}] {1}' -f [DateTime]::Now.ToString('HH:mm:ss'), $Message)
}

function Show-EvoWait($State, [string]$Activity) {
    if ($State.clock.Elapsed.TotalSeconds -ge $State.nextNotice) {
        Write-EvoStatus ('  SSH #{0}: nadal czekam - {1} ({2}s)' -f $State.number, $Activity,
            [int]$State.clock.Elapsed.TotalSeconds)
        $State.nextNotice = $State.clock.Elapsed.TotalSeconds + 5
    }
}

function Wait-EvoTask($Task, $Process, $State, [string]$Activity) {
    while (-not $Task.IsCompleted) {
        if ($State.clock.Elapsed.TotalSeconds -ge 90) { $Process.Kill(); throw "SSH timeout: $Activity" }
        $null = $Task.Wait(1000)
        Show-EvoWait $State $Activity
    }
    $null = $Task.GetAwaiter().GetResult()
}

function Receive-EvoStream($Stream) {
    while ($null -ne $Stream.pending -and $Stream.pending.IsCompleted) {
        $line=$Stream.pending.GetAwaiter().GetResult()
        if ($null -eq $line) { $Stream.pending=$null; break }
        Write-EvoStatus "Evo: $line"
        $null=$Stream.text.AppendLine($line)
        $Stream.pending=$Stream.reader.ReadLineAsync()
    }
}

function ConvertTo-ShellLiteral([string]$Value) {
    return "'" + $Value.Replace("'", "'\''") + "'"
}

function ConvertTo-WindowsArgument([string]$Value) {
    # CommandLineToArgvW quoting; shell metacharacters are never evaluated locally.
    return '"' + [regex]::Replace([regex]::Replace($Value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
}

function Initialize-EvoConnection([string]$Head, [switch]$Legacy, [string]$IdentityFile,
    [Security.SecureString]$Password) {
    $address = $null
    if (-not [Net.IPAddress]::TryParse($Head, [ref]$address) -or
        $address.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork -or
        $address.GetAddressBytes()[0] -ne 169 -or $address.GetAddressBytes()[1] -ne 254) {
        throw 'Use the wired ENET head-unit IPv4 address (169.254.x.x).'
    }
    $script:Ssh = (Get-Command ssh.exe -ErrorAction Stop).Source
    Write-EvoStatus "Polaczenie: root@$Head; klient: $script:Ssh"
    $script:SshArgs = @('-T', '-o', 'StrictHostKeyChecking=yes', '-o', 'ConnectTimeout=15',
        '-o', 'ServerAliveInterval=10', '-o', 'ServerAliveCountMax=3',
        '-o', 'HostKeyAlgorithms=+ssh-rsa', '-o', 'PubkeyAcceptedAlgorithms=+ssh-rsa',
        '-o', 'MACs=+hmac-sha1')
    # -Legacy remains accepted by the launchers; the verified Evo options above
    # are now always enabled. Do not add unrelated KEX/cipher/MAC algorithms.
    if ($IdentityFile) { $script:SshArgs += @('-i', (Resolve-Path -LiteralPath $IdentityFile).Path) }
    $script:EvoPassword = $null
    if (-not $IdentityFile) {
        if ($Password) { $script:EvoPassword=$Password }
        else {
            # Keep the owner's automatic login in the ignored local configuration.
            $root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $auth=Join-Path $root '.local/evo-auth.ps1'
            if(Test-Path -LiteralPath $auth -PathType Leaf){$script:EvoPassword=& $auth}
            else{$script:EvoPassword=Read-Host 'Haslo SSH root dla Evo' -AsSecureString}
        }
        if ($script:EvoPassword -isnot [Security.SecureString] -or -not $script:EvoPassword.Length) { throw 'SSH password is empty.' }
        Initialize-EvoAskPass
        $script:SshArgs += @('-o','PreferredAuthentications=password,keyboard-interactive', '-o','PubkeyAuthentication=no',
            '-o','NumberOfPasswordPrompts=1')
    }
    $script:SshArgs += "root@$Head"
}

function Initialize-EvoAskPass {
    $existingType = 'EvoAuth.Request' -as [type]
    if ($existingType) {
        $script:EvoAskPass=$existingType.Assembly.Location
    } else {
        $root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
        $folder=Join-Path $root '.local/ssh'
        New-Item -ItemType Directory -Force $folder | Out-Null
        $script:EvoAskPass=Join-Path $folder ('askpass-'+[Guid]::NewGuid().ToString('N')+'.exe')
        Add-Type -Path "$PSScriptRoot/EvoAskPass.cs" -OutputAssembly $script:EvoAskPass -OutputType ConsoleApplication
        $null=[Reflection.Assembly]::LoadFrom($script:EvoAskPass)
    }
}

function Invoke-Evo([string]$Command, [byte[]]$InputBytes, [switch]$Bytes, [string]$Activity, [switch]$StreamText) {
    if ($StreamText -and $Bytes) { throw 'StreamText cannot be used for binary reads.' }
    # Here-strings inherit CRLF from Windows checkouts. QNX ksh interprets
    # the trailing CR in `set -e` as part of the option. Normalize only the
    # remote shell command; binary stdin (helper/bundle/files) must stay exact.
    $Command=$Command.Replace("`r`n","`n").Replace("`r","`n")
    if (-not $Activity) {
        $Activity=($Command -replace '\s+', ' ').Trim()
        if ($Activity.Length -gt 120) { $Activity=$Activity.Substring(0,117)+'...' }
    }
    $script:EvoSshNumber++
    $state=@{number=$script:EvoSshNumber; clock=[Diagnostics.Stopwatch]::StartNew(); nextNotice=5}
    Write-EvoStatus "SSH #$($state.number) START: $Activity"
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $script:Ssh
    $info.UseShellExecute = $false
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $request=$null
    if ((Get-Variable EvoPassword -Scope Script -ErrorAction SilentlyContinue) -and $script:EvoPassword) {
        $request=New-Object EvoAuth.Request($script:EvoPassword)
        $info.EnvironmentVariables['SSH_ASKPASS']=$script:EvoAskPass
        $info.EnvironmentVariables['SSH_ASKPASS_REQUIRE']='force'
        $info.EnvironmentVariables['DISPLAY']='nbt-evo'
        $info.EnvironmentVariables['EVO_ASKPASS_PIPE']=$request.PipeName
        $info.EnvironmentVariables['EVO_ASKPASS_TOKEN']=$request.Token
    }
    # Keep SSH errors and authentication prompts on the user's terminal.
    $arguments = @($script:SshArgs) + @($Command)
    $info.Arguments = ($arguments | ForEach-Object { ConvertTo-WindowsArgument $_ }) -join ' '
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    $buffer = New-Object IO.MemoryStream
    $started=$false
    $errorRead=$null
    try {
        if (-not $process.Start()) { throw 'Could not start SSH.' }
        $started=$true
        $copy=$null
        $lines=New-Object Text.StringBuilder
        $stream=@{reader=$process.StandardOutput;pending=$null;text=$lines}
        if ($StreamText) { $stream.pending=$process.StandardOutput.ReadLineAsync() }
        else { $copy = $process.StandardOutput.BaseStream.CopyToAsync($buffer) }
        $errorRead = $process.StandardError.ReadToEndAsync()
        if ($null -ne $InputBytes) {
            $write = $process.StandardInput.BaseStream.WriteAsync($InputBytes, 0, $InputBytes.Length)
            if ($StreamText) {
                while (-not $write.IsCompleted) {
                    Receive-EvoStream $stream
                    $null=$write.Wait(250)
                    Show-EvoWait $state $Activity
                    if ($state.clock.Elapsed.TotalSeconds -ge 90) { $process.Kill(); throw "SSH timeout: $Activity" }
                }
                $null=$write.GetAwaiter().GetResult()
            } else { Wait-EvoTask $write $process $state $Activity }
        }
        $process.StandardInput.Close()
        if ($StreamText) {
            while ($null -ne $stream.pending) {
                $null=$stream.pending.Wait(1000)
                Receive-EvoStream $stream
                Show-EvoWait $state $Activity
                if ($state.clock.Elapsed.TotalSeconds -ge 90) { $process.Kill(); throw "SSH timeout: $Activity" }
            }
        }
        while (-not $process.WaitForExit(1000)) {
            if ($state.clock.Elapsed.TotalSeconds -ge 90) { $process.Kill(); throw "SSH timeout: $Activity" }
            Show-EvoWait $state $Activity
        }
        if ($copy) { $null = $copy.GetAwaiter().GetResult() }
        $errorText = $errorRead.GetAwaiter().GetResult().Trim()
        if ($process.ExitCode -ne 0) { throw "SSH failed ($($process.ExitCode)): $errorText`nRemote command: $Command" }
        if ($errorText) { Write-Host $errorText }
        $data = if ($StreamText) { [Text.Encoding]::UTF8.GetBytes($lines.ToString()) } else { $buffer.ToArray() }
        Write-EvoStatus ('SSH #{0} OK: {1} ({2:N1}s, odczyt {3} B)' -f $state.number, $Activity,
            $state.clock.Elapsed.TotalSeconds, $data.Length)
        if ($Bytes) { return ,$data }
        return [Text.Encoding]::UTF8.GetString($data).TrimEnd("`r", "`n")
    } catch {
        Write-EvoStatus ('SSH #{0} BLAD: {1} (po {2:N1}s)' -f $state.number, $Activity,
            $state.clock.Elapsed.TotalSeconds)
        if ($started -and $errorRead -and $process.WaitForExit(1000) -and $process.ExitCode -ne 0) {
            $detail=$errorRead.GetAwaiter().GetResult().Trim()
            throw "SSH failed ($($process.ExitCode)): $detail`nRemote command: $Command"
        }
        throw
    } finally {
        if ($started -and -not $process.HasExited) { $process.Kill() }
        $buffer.Dispose(); $process.Dispose()
        if ($request) { $request.Dispose() }
    }
}

function Get-BytesHash([byte[]]$Data) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Data))).Replace('-','').ToLower() }
    finally { $sha.Dispose() }
}

function Send-EvoFile([string]$LocalPath, [string]$RemotePath) {
    $data = [IO.File]::ReadAllBytes($LocalPath)
    $quoted = ConvertTo-ShellLiteral $RemotePath
    Write-EvoStatus "Plik: $([IO.Path]::GetFileName($LocalPath)) ($($data.Length) B) -> $RemotePath"
    $null = Invoke-Evo -Command "cat > $quoted" -InputBytes $data -Activity "Wysylanie $RemotePath"
    $received = Invoke-Evo -Command "cat $quoted" -Bytes -Activity "Odczyt kontrolny $RemotePath"
    if ((Get-BytesHash $data) -ne (Get-BytesHash $received)) { throw "Readback differs: $RemotePath" }
    Write-EvoStatus "SHA256 OK: $RemotePath"
}

function Invoke-EvoScript([string]$Text, [string]$Activity='Wykonywanie transakcji na Evo') {
    return Invoke-Evo -Command 'sh -s' -Activity $Activity -InputBytes ([Text.Encoding]::UTF8.GetBytes($Text.Replace("`r`n","`n") + "`n"))
}

function Assert-EvoTarget {
    $version = Invoke-Evo -Command 'cat /opt/sys/etc/nbt_version.txt' -Activity 'Sprawdzanie wersji firmware'
    if ($version.Trim() -ne 'NBTevo_Y24073J') { throw "Unexpected firmware: $version" }
    $expected = @{
        'libAppFramework.A.so'='ef2488b007d592301d8f21d9aa60771ad552d06b5e97c29e7813e3946644d820'
        'libCoreFramework.A.so'='cb0e488d1ebe2e30de87bb56f0e93d6a2d76b44309b0b47fef5b0b7cec048973'
    }
    foreach ($name in $expected.Keys) {
        $data = Invoke-Evo -Command "cat /opt/hmi/ID5/lib/$name" -Bytes -Activity "Sprawdzanie frameworka $name"
        if ((Get-BytesHash $data) -ne $expected[$name]) { throw "Unexpected framework: $name" }
        Write-EvoStatus "SHA256 frameworka OK: $name"
    }
}

function Get-EvoReceiptDirectory {
    $root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    return Join-Path $root '.local/deployments'
}

function Assert-RunId([string]$RunId) {
    if ($RunId -notmatch '^\d{8}T\d{6}Z-[a-f0-9]{12}$') { throw 'Invalid deployment run ID.' }
}

function Get-EvoFileNames([string]$Directory) {
    $quoted = ConvertTo-ShellLiteral $Directory
    $text = Invoke-Evo -Command "find $quoted -type f"
    return @($text -split "`n" | Where-Object { $_ } | ForEach-Object { $_.TrimEnd("`r") })
}

function Get-EvoTransaction([string]$Run, [object[]]$Items, [switch]$Uninstall) {
    $rollback=@('set +e', 'rollback_failed=0')
    foreach ($item in @($Items)[($Items.Count-1)..0]) {
        $app=$item.name
        if ($Uninstall) {
            if ($item.hadOriginal) {
                $rollback += "if test -d '$Run/removed/$app' && test ! -d '$Run/backup/$app' && test -d '$script:Plugins/$app'; then '$Run/fileops' move '$script:Plugins/$app' '$Run/backup/$app' || rollback_failed=1; fi"
            }
            $rollback += "if test -d '$Run/removed/$app'; then '$Run/fileops' move '$Run/removed/$app' '$script:Plugins/$app' || rollback_failed=1; fi"
        } else {
            $rollback += "if test ! -d '$Run/stage/$app' && test -d '$script:Plugins/$app'; then '$Run/fileops' move '$script:Plugins/$app' '$Run/failed/$app' || rollback_failed=1; fi"
            $rollback += "if test -d '$Run/backup/$app'; then '$Run/fileops' move '$Run/backup/$app' '$script:Plugins/$app' || rollback_failed=1; fi"
        }
    }
    $rollbackState = if ($Uninstall) { 'installed' } else { 'rolledback' }
    $rollback += "if test `"`$rollback_failed`" = 0; then if echo '$rollbackState' > '$Run/STATUS'; then '$Run/fileops' unlock '$script:Lock'; else echo 'Rollback status write failed; lock retained for recovery.' >&2; fi; else echo 'Rollback incomplete; lock retained for recovery.' >&2; fi"
    $commands=@('set -e', 'rollback() {', ($rollback -join "`n"), '}', "trap 'rollback' 0", "trap 'exit 1' 1 2 15")
    foreach ($item in $Items) {
        $app=$item.name
        if ($Uninstall) {
            $commands += "'$Run/fileops' move '$script:Plugins/$app' '$Run/removed/$app'"
            if ($item.hadOriginal) { $commands += "'$Run/fileops' move '$Run/backup/$app' '$script:Plugins/$app'" }
        } else {
            if ($item.hadOriginal) { $commands += "'$Run/fileops' move '$script:Plugins/$app' '$Run/backup/$app'" }
            $commands += "'$Run/fileops' move '$Run/stage/$app' '$script:Plugins/$app'"
        }
    }
    $state = if ($Uninstall) { 'uninstalled' } else { 'installed' }
    $commands += "echo '$state' > '$Run/STATUS'"
    $commands += "'$Run/fileops' unlock '$script:Lock'"
    $commands += 'trap - 0 1 2 15'
    return ($commands -join "`n")
}
