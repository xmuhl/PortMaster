# PortMaster 自动化工作流脚本 (PowerShell版本)
# 用途：实现完整的开发工作流，包括环境检测、编译验证、自动推送
# 基于CLAUDE.md中的工作流规范
# 作者：PortMaster开发团队
# 版本：v1.0

param(
    [switch]$SkipBuild = $false,
    [switch]$SkipPush = $false,
    [string]$CustomMessage = ""
)

$ErrorActionPreference = "Stop"
$global:WorkflowResult = @{
    Success = $true
    Messages = @()
    CommitId = ""
    TagName = ""
}

function Write-WorkflowLog {
    param(
        [string]$Message,
        [string]$Level = "INFO"
    )
    
    $timestamp = Get-Date -Format "HH:mm:ss"
    $logMessage = "[$timestamp] [$Level] $Message"
    
    switch ($Level) {
        "ERROR" { Write-Host $logMessage -ForegroundColor Red }
        "WARNING" { Write-Host $logMessage -ForegroundColor Yellow }
        "SUCCESS" { Write-Host $logMessage -ForegroundColor Green }
        default { Write-Host $logMessage }
    }
    
    $global:WorkflowResult.Messages += $logMessage
}

function Test-GitEnvironment {
    Write-WorkflowLog "检测Git环境..."
    
    try {
        # 检查Git是否可用
        $gitVersion = git --version 2>$null
        if ($LASTEXITCODE -ne 0) {
            throw "Git未安装或不在PATH中"
        }
        Write-WorkflowLog "Git版本: $gitVersion"
        
        # 检查是否在Git仓库中
        git rev-parse --git-dir 2>$null | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "当前目录不是Git仓库"
        }
        
        # 检查远程仓库
        $remotes = git remote
        if ($remotes -contains "backup") {
            Write-WorkflowLog "backup远程仓库配置正常"
            # 检查远程仓库URL是否正确
            $remoteUrl = git remote get-url backup
            if ($remoteUrl -ne "D:\GitBackups\PortMaster.git") {
                Write-WorkflowLog "backup远程仓库URL不正确，正在修正..." "WARNING"
                git remote set-url backup "D:\GitBackups\PortMaster.git"
            }
        } else {
            Write-WorkflowLog "未找到backup远程仓库，正在创建..." "WARNING"
            git remote add backup "D:\GitBackups\PortMaster.git"
        }
        
        if ($remotes -contains "origin") {
            Write-WorkflowLog "origin远程仓库配置正常"
        } else {
            Write-WorkflowLog "未找到origin远程仓库" "WARNING"
        }
        
        return $true
    }
    catch {
        Write-WorkflowLog $_.Exception.Message "ERROR"
        return $false
    }
}

function Sync-RemoteRepository {
    Write-WorkflowLog "同步远程仓库..."
    
    try {
        # 获取当前分支
        $currentBranch = git rev-parse --abbrev-ref HEAD
        Write-WorkflowLog "当前分支: $currentBranch"
        
        # 尝试拉取远程变更
        $remotes = git remote
        foreach ($remote in $remotes) {
            try {
                Write-WorkflowLog "尝试从 $remote 拉取变更..."
                git pull $remote $currentBranch --rebase
                if ($LASTEXITCODE -eq 0) {
                    Write-WorkflowLog "成功从 $remote 同步变更"
                }
            }
            catch {
                Write-WorkflowLog "从 $remote 同步失败: $($_.Exception.Message)" "WARNING"
            }
        }
        
        return $true
    }
    catch {
        Write-WorkflowLog $_.Exception.Message "WARNING"
        return $true  # 同步失败不中断工作流
    }
}

function Test-CodeChanges {
    Write-WorkflowLog "检查代码变更..."
    
    try {
        # 获取变更状态
        $status = git status --porcelain
        if ([string]::IsNullOrEmpty($status)) {
            Write-WorkflowLog "无代码变更"
            return $false
        }
        
        Write-WorkflowLog "检测到以下变更:"
        $status | ForEach-Object { Write-WorkflowLog "  $_" }
        
        # 分析变更类型
        $changedFiles = git diff --name-only HEAD
        $hasCodeChanges = $false
        $hasDocChanges = $false
        
        foreach ($file in $changedFiles) {
            if ($file -match '\.(cpp|h|hpp|c|rc|vcxproj|sln)$') {
                $hasCodeChanges = $true
                Write-WorkflowLog "检测到源码文件变更: $file"
            }
            elseif ($file -match '\.(md|txt|log)$') {
                $hasDocChanges = $true
                Write-WorkflowLog "检测到文档文件变更: $file"
            }
        }
        
        # 设置全局变量供后续使用
        $global:HasCodeChanges = $hasCodeChanges
        $global:HasDocChanges = $hasDocChanges
        
        return $true
    }
    catch {
        Write-WorkflowLog $_.Exception.Message "ERROR"
        return $false
    }
}

function Invoke-BuildVerification {
    if ($SkipBuild) {
        Write-WorkflowLog "跳过编译验证"
        return $true
    }
    
    if (-not $global:HasCodeChanges) {
        Write-WorkflowLog "无源码变更，跳过编译验证"
        return $true
    }
    
    Write-WorkflowLog "执行编译验证..."
    
    try {
        # 检查编译脚本
        $buildScript = ""
        if (Test-Path "autobuild_x86_debug.bat") {
            $buildScript = "autobuild_x86_debug.bat"
            Write-WorkflowLog "使用 autobuild_x86_debug.bat 进行编译"
        }
        elseif (Test-Path "autobuild.bat") {
            $buildScript = "autobuild.bat"
            Write-WorkflowLog "使用 autobuild.bat 进行编译"
        }
        else {
            throw "未找到编译脚本"
        }
        
        # 执行编译
        Write-WorkflowLog "开始编译..."
        & ".\$buildScript"
        if ($LASTEXITCODE -ne 0) {
            throw "编译失败，退出代码: $LASTEXITCODE"
        }
        
        # 验证编译结果
        if (Test-Path "build_check.bat") {
            Write-WorkflowLog "使用 build_check.bat 验证编译结果"
            & ".\build_check.bat"
            if ($LASTEXITCODE -ne 0) {
                throw "编译验证未通过"
            }
        }
        else {
            Write-WorkflowLog "未找到 build_check.bat，手动检查编译日志" "WARNING"
        }
        
        Write-WorkflowLog "编译验证通过" "SUCCESS"
        return $true
    }
    catch {
        Write-WorkflowLog $_.Exception.Message "ERROR"
        return $false
    }
}

function New-CommitMessage {
    param(
        [string]$CustomMessage = ""
    )
    
    if (-not [string]::IsNullOrEmpty($CustomMessage)) {
        return $CustomMessage
    }
    
    try {
        # 分析变更内容
        $changedFiles = git diff --cached --name-only
        
        $hasFeature = $false
        $hasFix = $false
        $hasDocs = $false
        $hasRefactor = $false
        
        foreach ($file in $changedFiles) {
            if ($file -match '(feature|new)') {
                $hasFeature = $true
            }
            if ($file -match '(fix|bug|error)') {
                $hasFix = $true
            }
            if ($file -match '\.md$') {
                $hasDocs = $true
            }
            if ($file -match '(refactor|optimize)') {
                $hasRefactor = $true
            }
        }
        
        # 根据变更类型生成提交信息
        if ($global:HasCodeChanges -and $global:HasDocChanges) {
            return "fix: 修复代码问题并更新相关文档"
        }
        elseif ($hasFeature) {
            return "feat: 添加新功能"
        }
        elseif ($hasFix) {
            return "fix: 修复功能问题"
        }
        elseif ($hasDocs) {
            return "docs: 更新项目文档"
        }
        elseif ($hasRefactor) {
            return "refactor: 代码重构优化"
        }
        else {
            return "chore: 更新项目文件"
        }
    }
    catch {
        return "chore: 更新项目文件"
    }
}

function Invoke-AutoPush {
    if ($SkipPush) {
        Write-WorkflowLog "跳过自动推送"
        return $true
    }
    
    Write-WorkflowLog "执行自动推送..."
    
    try {
        # 生成提交信息
        $commitMessage = New-CommitMessage -CustomMessage $CustomMessage
        Write-WorkflowLog "提交信息: $commitMessage"
        
        # 添加所有变更
        Write-WorkflowLog "添加变更文件..."
        git add -A
        
        # 检查是否有变更需要提交
        $status = git status --porcelain
        if ([string]::IsNullOrEmpty($status)) {
            Write-WorkflowLog "无变更需要提交"
            return $true
        }
        
        # 提交变更
        Write-WorkflowLog "提交变更..."
        git commit -m $commitMessage
        if ($LASTEXITCODE -ne 0) {
            throw "提交失败"
        }
        
        # 获取提交ID
        $commitId = git rev-parse --short HEAD
        $global:WorkflowResult.CommitId = $commitId
        Write-WorkflowLog "提交完成，Commit ID: $commitId" "SUCCESS"
        
        # 推送到远程仓库
        $remotes = git remote
        foreach ($remote in $remotes) {
            try {
                Write-WorkflowLog "推送到 $remote 远程仓库..."
                git push $remote HEAD
                if ($LASTEXITCODE -eq 0) {
                    Write-WorkflowLog "成功推送到 $remote" "SUCCESS"
                }
            }
            catch {
                Write-WorkflowLog "推送到 $remote 失败: $($_.Exception.Message)" "WARNING"
            }
        }
        
        # 生成时间戳标签
        $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
        $tagName = "save-$timestamp"
        $global:WorkflowResult.TagName = $tagName
        
        Write-WorkflowLog "创建标签: $tagName"
        git tag $tagName
        
        # 推送标签
        Write-WorkflowLog "推送标签..."
        git push --tags
        
        Write-WorkflowLog "自动推送完成" "SUCCESS"
        return $true
    }
    catch {
        Write-WorkflowLog $_.Exception.Message "ERROR"
        return $false
    }
}

function Show-WorkflowReport {
    Write-WorkflowLog "工作流执行报告:" "SUCCESS"
    Write-WorkflowLog "==================="
    
    if ($global:WorkflowResult.Success) {
        Write-WorkflowLog "✅ 工作流执行成功" "SUCCESS"
    } else {
        Write-WorkflowLog "❌ 工作流执行失败" "ERROR"
    }
    
    if (-not [string]::IsNullOrEmpty($global:WorkflowResult.CommitId)) {
        Write-WorkflowLog "提交ID: $($global:WorkflowResult.CommitId)"
    }
    
    if (-not [string]::IsNullOrEmpty($global:WorkflowResult.TagName)) {
        Write-WorkflowLog "标签: $($global:WorkflowResult.TagName)"
    }
    
    Write-WorkflowLog "详细日志已记录，可用于问题排查"
}

# ================= 主执行流程 =================

try {
    Write-WorkflowLog "开始执行PortMaster自动化工作流" "SUCCESS"
    Write-WorkflowLog "==============================================="
    
    # STEP 1: 环境检测
    Write-WorkflowLog "[STEP 1] 环境检测与初始化..."
    if (-not (Test-GitEnvironment)) {
        throw "Git环境检测失败"
    }
    
    # STEP 2: 同步远程仓库
    Write-WorkflowLog "[STEP 2] 同步远程仓库..."
    Sync-RemoteRepository
    
    # STEP 3: 检查代码变更
    Write-WorkflowLog "[STEP 3] 检查代码变更..."
    $hasChanges = Test-CodeChanges
    if (-not $hasChanges) {
        Write-WorkflowLog "无代码变更，工作流结束"
        Show-WorkflowReport
        exit 0
    }
    
    # STEP 4: 编译验证
    Write-WorkflowLog "[STEP 4] 编译验证..."
    if (-not (Invoke-BuildVerification)) {
        throw "编译验证失败"
    }
    
    # STEP 5: 自动推送
    Write-WorkflowLog "[STEP 5] 自动推送..."
    if (-not (Invoke-AutoPush)) {
        throw "自动推送失败"
    }
    
    Write-WorkflowLog "==============================================="
    Write-WorkflowLog "✅ 自动化工作流执行完成！" "SUCCESS"
    Show-WorkflowReport
}
catch {
    Write-WorkflowLog $_.Exception.Message "ERROR"
    $global:WorkflowResult.Success = $false
    Show-WorkflowReport
    exit 1
}