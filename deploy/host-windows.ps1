param([ValidateSet('install', 'uninstall')][string]$Action)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Adb = Join-Path $Root 'platform-tools\adb.exe'
if (-not (Test-Path -LiteralPath $Adb)) { $Adb = 'adb.exe' }

function Invoke-Adb {
    & $Adb @args
    if ($LASTEXITCODE -ne 0) { throw 'ADB 命令失败，请检查设备连接和授权。' }
}

try {
    $state = Invoke-Adb get-state
    if ($state -ne 'device') { throw '请通过 ADB 连接一台设备。' }
    if ($Action -eq 'install') {
        $program = Join-Path $Root 'zxct-ims.gz'
        if (-not (Test-Path -LiteralPath $program)) { $program = Join-Path $Root 'build\zxct-ims.arm.gz' }
        if (-not (Test-Path -LiteralPath $program)) { throw '找不到程序文件，请使用完整的部署包或先构建程序。' }
        Invoke-Adb push $program /mnt/userdata/zxct-ims.gz
        Invoke-Adb shell 'mkdir -p /tmp/zxct-deploy'
        foreach ($file in @('global.sh', 'hook.sh', 'install-autostart.sh')) {
            Invoke-Adb push (Join-Path $PSScriptRoot $file) "/tmp/zxct-deploy/$file"
        }
        Invoke-Adb shell 'sh /tmp/zxct-deploy/install-autostart.sh'
        Write-Host '安装完成。重启设备后程序将自动启动，重启会短暂断网。'
        $answer = Read-Host '现在重启设备？[y/N]'
        if ($answer -match '^(y|yes)$') { Invoke-Adb reboot }
    } else {
        Invoke-Adb shell 'mkdir -p /tmp/zxct-deploy'
        Invoke-Adb push (Join-Path $PSScriptRoot 'uninstall-autostart.sh') /tmp/zxct-deploy/uninstall-autostart.sh
        Invoke-Adb shell 'sh /tmp/zxct-deploy/uninstall-autostart.sh'
        Write-Host '卸载完成。短信数据库和短信功能开关已保留。'
        Write-Host '若程序仍在运行，重启设备后会停止。'
    }
} catch {
    Write-Host "操作失败：$_"
    exit 1
}
