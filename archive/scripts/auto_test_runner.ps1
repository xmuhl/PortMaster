# auto_test_runner.ps1 - 自动化测试运行器，包含程序启动和截图功能
param(
    [string]$ExecutablePath = "build\Debug\PortMaster.exe",
    [string]$ScreenshotDir = "test_screenshots",
    [int]$WaitTime = 10,
    [int]$MaxRunTime = 60
)

# 创建截图目录
if (!(Test-Path $ScreenshotDir)) {
    New-Item -ItemType Directory -Path $ScreenshotDir | Out-Null
}

Write-Host "=== PortMaster 自动化测试运行器 ===" -ForegroundColor Green
Write-Host "可执行文件路径: $ExecutablePath" -ForegroundColor Yellow
Write-Host "截图保存目录: $ScreenshotDir" -ForegroundColor Yellow
Write-Host "等待时间: $WaitTime 秒" -ForegroundColor Yellow
Write-Host "最大运行时间: $MaxRunTime 秒" -ForegroundColor Yellow

# 检查可执行文件是否存在
if (!(Test-Path $ExecutablePath)) {
    Write-Host "错误: 可执行文件不存在 - $ExecutablePath" -ForegroundColor Red
    exit 1
}

# 获取当前时间戳
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logFile = "$ScreenshotDir\test_log_$timestamp.txt"

Write-Host "开始执行测试..." -ForegroundColor Green

# 截图函数
function Take-Screenshot {
    param([string]$suffix = "")

    try {
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing

        $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
        $bitmap = New-Object System.Drawing.Bitmap($bounds.width, $bounds.height)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CopyFromScreen($bounds.X, $bounds.Y, 0, 0, $bounds.size, [System.Drawing.CopyPixelOperation]::SourceCopy)

        $screenshotFile = "$ScreenshotDir\screenshot_$timestamp$($suffix).png"
        $bitmap.Save($screenshotFile, [System.Drawing.Imaging.ImageFormat]::Png)

        $graphics.Dispose()
        $bitmap.Dispose()

        Write-Host "截图已保存: $screenshotFile" -ForegroundColor Cyan
        return $screenshotFile
    } catch {
        Write-Host "截图失败: $($_.Exception.Message)" -ForegroundColor Red
        return $null
    }
}

# 检查进程是否运行的函数
function Test-ProcessRunning {
    param([string]$processName)

    $processes = Get-Process | Where-Object { $_.ProcessName -like "*$processName*" }
    return ($processes.Count -gt 0)
}

# 查找主窗口的函数
function Find-MainWindow {
    try {
        # 查找PortMaster主窗口
        $process = Get-Process | Where-Object { $_.MainWindowTitle -like "*PortMaster*" -or $_.ProcessName -like "*PortMaster*" } | Select-Object -First 1
        if ($process -and $process.MainWindowHandle) {
            return $process
        }

        # 如果没找到，尝试按标题查找
        $windows = Get-Process | Where-Object { $_.MainWindowTitle -ne "" } | Where-Object { $_.MainWindowTitle -like "*大师*" -or $_.MainWindowTitle -like "*Port*" }
        if ($windows) {
            return $windows | Select-Object -First 1
        }

        return $null
    } catch {
        return $null
    }
}

# 截图特定窗口的函数
function Take-WindowScreenshot {
    param([System.Diagnostics.Process]$process, [string]$suffix = "")

    if (!$process -or !$process.MainWindowHandle) {
        return Take-Screenshot -suffix $suffix
    }

    try {
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing

        # 获取窗口位置和大小
        $handle = $process.MainWindowHandle
        $rect = New-Object System.Drawing.Rectangle
        $success = [System.Windows.Forms.NativeMethods]::GetWindowRect($handle, [ref]$rect)

        if (!$success) {
            return Take-Screenshot -suffix $suffix
        }

        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top

        if ($width -le 0 -or $height -le 0) {
            return Take-Screenshot -suffix $suffix
        }

        $bitmap = New-Object System.Drawing.Bitmap($width, $height)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size, [System.Drawing.CopyPixelOperation]::SourceCopy)

        $screenshotFile = "$ScreenshotDir\window_$timestamp$($suffix).png"
        $bitmap.Save($screenshotFile, [System.Drawing.Imaging.ImageFormat]::Png)

        $graphics.Dispose()
        $bitmap.Dispose()

        Write-Host "窗口截图已保存: $screenshotFile" -ForegroundColor Cyan
        return $screenshotFile
    } catch {
        Write-Host "窗口截图失败，使用全屏截图: $($_.Exception.Message)" -ForegroundColor Yellow
        return Take-Screenshot -suffix $suffix
    }
}

# 启动前截图
Write-Host "启动前截图..." -ForegroundColor Yellow
Take-Screenshot -suffix "_before_start"

# 关闭可能已存在的进程
Write-Host "清理现有进程..." -ForegroundColor Yellow
Get-Process | Where-Object { $_.ProcessName -like "*PortMaster*" } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

# 启动程序
Write-Host "启动程序: $ExecutablePath" -ForegroundColor Yellow
$startTime = Get-Date

try {
    $process = Start-Process -FilePath $ExecutablePath -PassThru
    Write-Host "进程已启动，PID: $($process.Id)" -ForegroundColor Green

    # 等待程序启动
    Write-Host "等待程序启动..." -ForegroundColor Yellow
    Start-Sleep -Seconds $WaitTime

    # 检查进程是否仍在运行
    if (!$process.HasExited) {
        Write-Host "程序正在运行中..." -ForegroundColor Green

        # 查找主窗口
        $mainWindow = Find-MainWindow
        if ($mainWindow) {
            Write-Host "找到主窗口: $($mainWindow.MainWindowTitle)" -ForegroundColor Green
        } else {
            Write-Host "未找到主窗口，使用全屏截图" -ForegroundColor Yellow
        }

        # 启动后截图
        Take-WindowScreenshot -process $mainWindow -suffix "_after_start"

        # 等待用户交互时间
        Write-Host "等待 $MaxRunTime 秒进行观察..." -ForegroundColor Yellow

        $elapsedSeconds = 0
        while ($elapsedSeconds -lt $MaxRunTime -and !$process.HasExited) {
            Start-Sleep -Seconds 5
            $elapsedSeconds += 5

            # 每15秒截图一次
            if ($elapsedSeconds % 15 -eq 0) {
                Write-Host "运行时间: $elapsedSeconds 秒，截图记录..." -ForegroundColor Cyan
                Take-WindowScreenshot -process $mainWindow -suffix "_running_$elapsedSeconds"
            }

            # 检查是否有断言错误对话框
            $errorWindows = Get-Process | Where-Object { $_.MainWindowTitle -like "*Assertion*" -or $_.MainWindowTitle -like "*Debug*" -or $_.MainWindowTitle -like "*Error*" }
            if ($errorWindows) {
                Write-Host "检测到错误对话框！" -ForegroundColor Red
                Take-WindowScreenshot -process $errorWindows -suffix "_error_dialog"
                break
            }
        }

        # 最终截图
        Take-WindowScreenshot -process $mainWindow -suffix "_final"

    } else {
        Write-Host "程序已退出" -ForegroundColor Red
        Take-Screenshot -suffix "_program_exited"
    }

} catch {
    Write-Host "启动程序失败: $($_.Exception.Message)" -ForegroundColor Red
    Take-Screenshot -suffix "_start_failed"
}

# 生成测试报告
$endTime = Get-Date
$duration = $endTime - $startTime

# 检查截图数量
$screenshotCount = 0
if (Test-Path $ScreenshotDir) {
    $screenshotCount = (Get-ChildItem $ScreenshotDir -Filter "screenshot_*.png" | Measure-Object).Count
}

# 检查错误截图
$errorDetected = "未发现错误对话框"
if (Test-Path $ScreenshotDir) {
    $errorScreenshots = Get-ChildItem $ScreenshotDir -Filter "*error*.png" -ErrorAction SilentlyContinue
    if ($errorScreenshots) {
        $errorDetected = "发现错误对话框"
    }
}

$report = @"
=== PortMaster 自动化测试报告 ===
测试时间: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
可执行文件: $ExecutablePath
测试持续时间: $($duration.TotalSeconds) 秒

测试结果:
- 程序启动: 成功
- 截图数量: $screenshotCount
- 错误检测: $errorDetected

截图文件:
"@

if (Test-Path $ScreenshotDir) {
    Get-ChildItem $ScreenshotDir -Filter "*.png" | ForEach-Object {
        $report += "`n- $($_.Name)"
    }
}

$report | Out-File -FilePath $logFile -Encoding UTF8

Write-Host "`n=== 测试完成 ===" -ForegroundColor Green
Write-Host "测试报告: $logFile" -ForegroundColor Yellow
Write-Host "截图目录: $ScreenshotDir" -ForegroundColor Yellow

# 清理进程
Write-Host "清理测试进程..." -ForegroundColor Yellow
Get-Process | Where-Object { $_.ProcessName -like "*PortMaster*" } | Stop-Process -Force -ErrorAction SilentlyContinue

Write-Host "自动化测试完成！" -ForegroundColor Green