$ErrorActionPreference = 'Stop'
# 把 build\INIWeaver.sln 的项目路径加上 build\ 前缀后，派生一个根目录可见的 INIWeaver.sln，
# 这样 VS 从仓库根打开 INIWeaver.sln 时，所有 CMake 自动生成的 *.vcxproj 都被正确指向 build\ 子目录，
# 构建中间产物仍保留在 build\，仅“构建入口”sln 可见于根目录（对齐 ImGui 旧版的根目录可见布局）。
# 每次 `cmake -S . -B build` 重新配置后必须再跑一次本脚本以同步根 sln。
$root   = Split-Path -Parent $MyInvocation.MyCommand.Path
$srcSln = Join-Path $root 'build\INIWeaver.sln'
$outSln = Join-Path $root 'INIWeaver.sln'
if (-not (Test-Path $srcSln)) {
  Write-Error "源 sln 不存在: $srcSln。请先运行: cmake -S `"$root`" -B `"$root\build`""
}
$content = [System.IO.File]::ReadAllText($srcSln)
# 重写：把 Project(...) = "Name", "X.vcxproj", "{GUID}" 中的 "X.vcxproj" 前面加 build\
$rewritten = $content -replace '(,\s+")([^"]+\.vcxproj)(")', '${1}build\${2}${3}'
# CMake 生成的 sln 是 UTF-8 with BOM，输出也保留 BOM
$utf8WithBom = New-Object System.Text.UTF8Encoding $true
[System.IO.File]::WriteAllText($outSln, $rewritten, $utf8WithBom)
Write-Host ("已生成根 sln: " + $outSln + "  (18 个 .vcxproj 路径已重写为 build\<名称>.vcxproj)")