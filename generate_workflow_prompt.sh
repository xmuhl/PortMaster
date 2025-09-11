#!/bin/bash

#=============================================================================
# PortMaster 工作流提示词生成器
# 功能：分析当前项目状态，生成适合的工作流程提示词
# 用途：在每次新对话开始前运行，获取工作流指令
# 环境：WSL2 + Windows 文件系统
# 版本：1.0
#=============================================================================

set -euo pipefail

# =============================================================================
# 全局配置
# =============================================================================

readonly WORK_DIR="/mnt/c/Users/huangl/Desktop/PortMaster"
readonly WIN_PATH="C:\Users\huangl\Desktop\PortMaster"
readonly PROGRESS_DOC="Code_Revision_Progress.md"
readonly CLAUDE_CONFIG="CLAUDE.md"

# 颜色配置（用于终端显示）
readonly BLUE='\033[0;34m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly RED='\033[0;31m'
readonly BOLD='\033[1m'
readonly NC='\033[0m'

# =============================================================================
# 分析函数
# =============================================================================

# 分析当前项目状态
analyze_project_status() {
    local status_info=""
    
    # 1. Git状态分析
    local git_status
    git_status=$(git status --porcelain 2>/dev/null || echo "")
    
    if [[ -n "$git_status" ]]; then
        local changed_files=$(echo "$git_status" | wc -l)
        status_info+="📝 **文件变更**: 检测到 $changed_files 个文件变更\n"
        
        # 分析变更类型
        local code_files=$(echo "$git_status" | grep -E '\.(cpp|h|c|hpp|rc)$' | wc -l 2>/dev/null | tr -d ' ' || echo "0")
        local doc_files=$(echo "$git_status" | grep -E '\.(md|txt)$' | wc -l 2>/dev/null | tr -d ' ' || echo "0")
        local script_files=$(echo "$git_status" | grep -E '\.(sh|bat)$' | wc -l 2>/dev/null | tr -d ' ' || echo "0")
        
        status_info+="   - 代码文件: $code_files 个\n"
        status_info+="   - 文档文件: $doc_files 个\n"
        status_info+="   - 脚本文件: $script_files 个\n"
        
        # 需要编译验证判断
        if [[ $code_files -gt 0 || $script_files -gt 0 ]]; then
            status_info+="⚡ **编译需求**: 需要编译验证\n"
        else
            status_info+="📚 **编译需求**: 仅文档更新，可跳过编译\n"
        fi
    else
        status_info+="✅ **文件变更**: 无变更，工作目录整洁\n"
    fi
    
    # 2. 项目进度分析
    if [[ -f "$PROGRESS_DOC" ]]; then
        local overall_progress
        overall_progress=$(grep "整体进度" "$PROGRESS_DOC" | head -1 | sed 's/.*整体进度[：:]//' | sed 's/[^0-9]*\([0-9]*\)%.*/\1/' || echo "未知")
        
        local current_stage
        current_stage=$(grep "当前阶段" "$PROGRESS_DOC" | head -1 | sed 's/.*当前阶段[：:]//' | sed 's/^\s*\*\*//' | sed 's/\*\*.*$//')
        
        status_info+="📊 **项目进度**: $overall_progress%\n"
        status_info+="🎯 **当前阶段**: $current_stage\n"
    fi
    
    # 3. 最近提交分析
    local recent_commit
    recent_commit=$(git log -1 --oneline 2>/dev/null | cut -c1-50 || echo "无提交记录")
    status_info+="🔄 **最新提交**: $recent_commit\n"
    
    # 4. 远程仓库状态
    local remote_status
    if git status | grep -q "Your branch is up to date"; then
        remote_status="同步"
    elif git status | grep -q "Your branch is ahead"; then
        remote_status="本地领先"
    elif git status | grep -q "Your branch is behind"; then
        remote_status="需要拉取"
    elif git status | grep -q "have diverged"; then
        remote_status="存在分叉"
    else
        remote_status="未知"
    fi
    status_info+="🌐 **远程状态**: $remote_status\n"
    
    echo -e "$status_info"
}

# 生成工作流提示词
generate_workflow_prompt() {
    local git_status
    git_status=$(git status --porcelain 2>/dev/null || echo "")
    
    local workflow_prompt=""
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    # 提示词开头
    workflow_prompt+="THINK HARD THROUGH THIS STEP BY STEP:\n"
    workflow_prompt+="根据以下项目状态分析，请严格按照 CLAUDE.md 中设定的 WSL环境自动执行工作流程 来处理当前任务。\n\n"
    
    # 项目状态摘要
    workflow_prompt+="## 📋 当前项目状态摘要 ($timestamp)\n\n"
    workflow_prompt+="$(analyze_project_status)\n"
    
    # 根据状态生成具体工作流程
    if [[ -n "$git_status" ]]; then
        # 有文件变更的情况
        workflow_prompt+="## 🎯 执行任务\n\n"
        workflow_prompt+="请按照以下步骤执行完整的工作流程：\n\n"
        
        workflow_prompt+="### 1. 环境准备与同步\n"
        workflow_prompt+="\`\`\`bash\n"
        workflow_prompt+="cd \"/mnt/c/Users/huangl/Desktop/PortMaster\"\n"
        workflow_prompt+="git pull --rebase 2>/dev/null || echo \"同步完成或无需同步\"\n"
        workflow_prompt+="\`\`\`\n\n"
        
        workflow_prompt+="### 2. 进度文档更新\n"
        workflow_prompt+="- 在 Code_Revision_Progress.md 的执行日志中添加当前任务开始记录\n"
        workflow_prompt+="- 格式：- ⏳ **$(date +%H:%M)**: 开始处理文件变更并验证推送\n\n"
        
        # 检查是否需要编译
        local code_files=$(echo "$git_status" | grep -E '\.(cpp|h|c|hpp|rc)$' | wc -l 2>/dev/null | tr -d ' ' || echo "0")
        local script_files=$(echo "$git_status" | grep -E '\.(sh|bat)$' | wc -l 2>/dev/null | tr -d ' ' || echo "0")
        
        if [[ $code_files -gt 0 || $script_files -gt 0 ]]; then
            workflow_prompt+="### 3. 编译验证（必需）\n"
            workflow_prompt+="\`\`\`bash\n"
            workflow_prompt+="# 首选编译命令\n"
            workflow_prompt+="cd \"/mnt/c/Users/huangl/Desktop/PortMaster\" && cmd.exe /c \"autobuild_x86_debug.bat\" 2>&1 | tail -20\n\n"
            workflow_prompt+="# 如果失败则使用备用命令\n"
            workflow_prompt+="cd \"/mnt/c/Users/huangl/Desktop/PortMaster\" && cmd.exe /c \"autobuild.bat\" 2>&1 | tail -20\n"
            workflow_prompt+="\`\`\`\n"
            workflow_prompt+="**要求**: 必须达到 0 error 0 warning 标准，展示关键日志片段\n\n"
        else
            workflow_prompt+="### 3. 编译验证（跳过）\n"
            workflow_prompt+="检测到仅有文档变更，跳过编译验证步骤\n\n"
        fi
        
        workflow_prompt+="### 4. 版本控制与推送\n"
        workflow_prompt+="\`\`\`bash\n"
        workflow_prompt+="# 检查变更状态\n"
        workflow_prompt+="git status --porcelain\n\n"
        workflow_prompt+="# 如有变更则提交推送\n"
        workflow_prompt+="git add -A\n"
        workflow_prompt+="git commit -m \"智能生成的提交信息\"\n"
        workflow_prompt+="git push backup HEAD\n"
        workflow_prompt+="git remote | grep -q origin && git push origin HEAD\n"
        workflow_prompt+="\`\`\`\n\n"
        
        workflow_prompt+="### 5. 存档标签生成\n"
        workflow_prompt+="\`\`\`bash\n"
        workflow_prompt+="tag=\"save-\\\$(date +%Y%m%d-%H%M%S)\"\n"
        workflow_prompt+="git tag \"\\\$tag\"\n"
        workflow_prompt+="git push --tags\n"
        workflow_prompt+="\`\`\`\n\n"
        
        workflow_prompt+="### 6. 工作汇报\n"
        workflow_prompt+="请提供：\n"
        workflow_prompt+="- 最新 commit ID 和说明\n"
        workflow_prompt+="- 远程推送结果\n"
        workflow_prompt+="- 编译成功的日志片段（如适用）\n"
        workflow_prompt+="- 进度文档更新确认\n\n"
        
    else
        # 无文件变更的情况
        workflow_prompt+="## ✅ 项目状态良好\n\n"
        workflow_prompt+="当前工作目录整洁，无需执行工作流程。\n"
        workflow_prompt+="项目处于稳定状态，可以进行以下操作：\n\n"
        workflow_prompt+="- 📖 查看项目文档和进度\n"
        workflow_prompt+="- 🔍 代码审查和分析\n"
        workflow_prompt+="- 🚀 开始新的开发任务\n"
        workflow_prompt+="- 📋 项目规划和需求分析\n\n"
    fi
    
    # 重要提醒
    workflow_prompt+="## ⚠️ 重要提醒\n\n"
    workflow_prompt+="1. **零容忍政策**: 绝不允许带有编译错误或警告的代码进入版本库\n"
    workflow_prompt+="2. **防止分叉**: 使用 rebase 方式同步，避免产生 merge commit\n"
    workflow_prompt+="3. **进度同步**: 及时更新进度文档，保持项目状态可追踪\n"
    workflow_prompt+="4. **WSL环境**: 使用 cmd.exe 调用Windows编译脚本\n\n"
    
    workflow_prompt+="---\n"
    workflow_prompt+="**📌 提示**: 此工作流程基于 CLAUDE.md 中的 WSL环境自动执行工作流程 设定"
    
    echo -e "$workflow_prompt"
}

# =============================================================================
# 主函数
# =============================================================================

main() {
    # 检查工作目录
    if ! cd "$WORK_DIR" 2>/dev/null; then
        echo -e "${RED}错误: 无法访问工作目录 $WORK_DIR${NC}" >&2
        exit 1
    fi
    
    if [[ ! -f "$CLAUDE_CONFIG" ]]; then
        echo -e "${RED}错误: 未找到 CLAUDE.md 配置文件，请确保在项目根目录执行${NC}" >&2
        exit 1
    fi
    
    # 显示生成信息
    echo -e "${BLUE}==========================================${NC}"
    echo -e "${BOLD}  PortMaster 工作流提示词生成器${NC}"
    echo -e "${BLUE}==========================================${NC}"
    echo -e "${GREEN}分析时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo -e "${GREEN}工作目录: $WORK_DIR${NC}"
    echo ""
    
    # 生成并输出工作流提示词
    echo -e "${YELLOW}以下是生成的工作流提示词（可直接复制使用）：${NC}"
    echo ""
    echo "========================================================"
    echo ""
    
    generate_workflow_prompt
    
    echo ""
    echo "========================================================"
    echo ""
    echo -e "${GREEN}✅ 工作流提示词生成完成！${NC}"
    echo -e "${BLUE}💡 使用方法: 复制上方提示词内容到新的Claude对话中${NC}"
}

# =============================================================================
# 脚本入口点
# =============================================================================

# 显示使用说明
if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    cat << EOF
PortMaster 工作流提示词生成器

用途:
  在每次新对话开始前运行，分析当前项目状态并生成相应的工作流程提示词

用法:
  $0              # 生成工作流提示词
  $0 -h|--help    # 显示此帮助信息

输出:
  生成的提示词可直接复制粘贴到Claude对话中使用

示例:
  cd /mnt/c/Users/huangl/Desktop/PortMaster
  ./generate_workflow_prompt.sh

EOF
    exit 0
fi

# 运行主函数
main "$@"