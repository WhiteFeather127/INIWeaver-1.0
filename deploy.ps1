# deploy.ps1 — 用 windeployqt 自动生成 Qt6 运行库并打 Release 包
#
# 不把 Qt 运行库放入 git 仓库：这些 DLL/QML 插件是产物，由本脚本从本机 Qt
# 安装目录用 windeployqt 一次性生成，可随时重建。
#
# 用法：
#   # 最小包（仅 EXE + Qt 运行库）
#   .\deploy.ps1
#
#   # 连同应用数据一起发布并打 zip（-AppData 指向含 Global/、Resources/ 的安装目录）
#   .\deploy.ps1 -AppData ".\install" -OutZip ".\INIWeaver-Release.zip"
#
# 参数：
#   -QtBin   windeployqt 所在目录（缺省自动探测：QTDIR 环境变量 → PATH）
#   -Target  发布目录（默认 <根>/publish）
#   -AppData 已有安装目录，取其 Global/、Resources/ 作为应用数据（可选）
#   -OutZip  打成的 zip 路径（可选）
param(
    [string]$QtBin   = "",
    [string]$Target  = "",
    [string]$AppData = "",
    [string]$OutZip  = ""
)

$ErrorActionPreference = "Stop"
$root   = $PSScriptRoot

# ---- 1) 定位构建产物 ----
$exeSrc = Join-Path $root "Release\INIWeaver.exe"
if (-not (Test-Path $exeSrc)) {
    Write-Error "缺少构建产物: $exeSrc（请先编译 Release）"
    exit 1
}

if ($Target -eq "") { $Target = Join-Path $root "publish" }
if (Test-Path $Target) { Remove-Item $Target -Recurse -Force }
New-Item -ItemType Directory -Path $Target | Out-Null

# ---- 2) 拷贝 EXE ----
Copy-Item $exeSrc -Destination $Target

# ---- 3) windeployqt 生成 Qt 运行库（DLL + 平台/QML 插件）----
# 未显式指定 -QtBin 时自动探测：QTDIR 环境变量（指向含 bin\windeployqt.exe 的目录）→ PATH
if ($QtBin -eq "") {
    if ($env:QTDIR -and (Test-Path (Join-Path $env:QTDIR "bin\windeployqt.exe"))) {
        $QtBin = Join-Path $env:QTDIR "bin"
        Write-Host "==> 使用 QTDIR 环境变量: $QtBin"
    } else {
        $wdqCmd = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
        if ($wdqCmd -and $wdqCmd.Source) {
            $QtBin = Split-Path $wdqCmd.Source
            Write-Host "==> 使用 PATH 中的 windeployqt: $QtBin"
        }
    }
}
$wdq = Join-Path $QtBin "windeployqt.exe"
if (-not (Test-Path $wdq)) {
    Write-Error "找不到 windeployqt: $wdq（请用 -QtBin 指定 windeployqt 所在目录）"
    exit 1
}
Write-Host "==> windeployqt 生成 Qt 运行库 ..."
# --qmldir 指定源码里的 QML，使 windeployqt 能解析 QtQuick/Controls 等依赖并部署对应插件
& $wdq --release --no-translations --qmldir (Join-Path $root "INIBrowser\ui") (Join-Path $Target "INIWeaver.exe")
if ($LASTEXITCODE -ne 0) {
    Write-Error "windeployqt 失败（exit $LASTEXITCODE）"
    exit $LASTEXITCODE
}

# ---- 4) 应用数据（Global/ 模块库、Resources/ 配置），从已有安装目录拷贝 ----
if ($AppData -ne "") {
    if (Test-Path $AppData) {
        foreach ($d in @("Global", "Resources")) {
            $s = Join-Path $AppData $d
            if (Test-Path $s) { Copy-Item $s -Destination $Target -Recurse }
        }
    } else {
        Write-Host "警告：-AppData 路径不存在，跳过 Global/、Resources/：$AppData"
    }
} else {
    Write-Host "提示：未指定 -AppData，未部署 Global/、Resources/（仅生成最小 Qt 运行库包）"
}

# ---- 5) 打包 zip ----
if ($OutZip -ne "") {
    if (Test-Path $OutZip) { Remove-Item $OutZip -Force }
    Compress-Archive -Path (Join-Path $Target "*") -DestinationPath $OutZip -CompressionLevel Optimal
    Write-Host "==> 已生成: $OutZip"
}

Write-Host "==> 发布目录: $Target"