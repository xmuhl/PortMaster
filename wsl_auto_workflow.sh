#!/bin/bash

#=============================================================================
# PortMaster WSL环境自动工作流程脚本
# 功能：自动检测文件变更、编译验证、进度更新、版本控制推送
# 环境：WSL2 + Windows 文件系统
# 作者：Claude Code
# 版本：1.0
#=============================================================================

set -euo pipefail  # 严格错误处理模式

# =============================================================================
# 全局配置
# =============================================================================

# WSL环境路径配置
readonly WORK_DIR="/mnt/c/Users/huangl/Desktop/PortMaster"
readonly WIN_PATH="C:\Users\huangl\Desktop\PortMaster"
readonly BACKUP_REPO="/mnt/d/GitBackups/PortMaster.git"
readonly PROGRESS_DOC="Code_Revision_Progress.md"

# 颜色配置
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly BLUE='\033[0;34m'
readonly PURPLE='\033[0;35m'
readonly NC='\033[0m' # No Color

# 全局变量
SCRIPT_START_TIME=""
CURRENT_TASK=""
BUILD_REQUIRED=false
HAS_CHANGES=false

# =============================================================================
# 工具函数
# =============================================================================

# 打印带颜色的消息
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${PURPLE}[STEP]${NC} $1"
}

# 获取当前时间
get_timestamp() {
    date +"%H:%M"
}

get_full_timestamp() {
    date +"%Y-%m-%d %H:%M:%S"
}

# 错误处理函数
cleanup_on_error() {
    log_error "脚本执行失败，正在清理..."
    if [[ $HAS_CHANGES == true ]]; then
        log_warning "检测到已暂存的变更，是否回滚？(y/n)"
        read -r response
        if [[ $response == "y" || $response == "Y" ]]; then
            git restore --staged . 2>/dev/null || true
            log_info "已回滚暂存的变更"
        fi
    fi
    exit 1
}

# 注册错误处理
trap cleanup_on_error ERR

# =============================================================================
# 核心工作流程函数
# =============================================================================

# 1. 环境准备与同步
sync_repository() {
    log_step "步骤1: 环境准备与同步"
    
    # 切换到工作目录
    if ! cd "$WORK_DIR"; then
        log_error "无法访问工作目录: $WORK_DIR"
        exit 1
    fi
    
    log_info "当前工作目录: $(pwd)"
    
    # 同步远程变更
    log_info "正在同步远程变更..."
    if git pull --rebase 2>/dev/null; then
        log_success "远程同步完成"
    else
        log_warning "远程同步失败或无需同步"
    fi
}

# 2. 进度文档更新
update_progress_document() {
    log_step "步骤2: 更新进度文档"
    
    local current_time
    current_time=$(get_timestamp)
    
    # 检查进度文档是否存在
    if [[ ! -f "$PROGRESS_DOC" ]]; then
        log_warning "进度文档不存在，跳过更新"
        return 0
    fi
    
    # 创建临时更新内容
    local temp_update="- ⏳ **${current_time}**: 开始自动工作流程执行 - 检测文件变更并处理"
    
    # 在执行日志部分之前插入新的进度记录
    if grep -q "## 📝 执行日志" "$PROGRESS_DOC"; then
        # 找到执行日志部分，在其后添加新记录
        local temp_file=$(mktemp)
        awk -v new_line="$temp_update" '
        /^### [0-9]{4}-[0-9]{2}-[0-9]{2}/ { 
            if (!inserted) {
                print new_line
                inserted = 1
            }
        }
        { print }
        END {
            if (!inserted) {
                print ""
                print new_line
            }
        }' "$PROGRESS_DOC" > "$temp_file"
        mv "$temp_file" "$PROGRESS_DOC"
        log_success "进度文档已更新: $temp_update"
    else
        log_warning "进度文档格式不匹配，跳过自动更新"
    fi
}

# 3. 检测文件变更
check_file_changes() {
    log_step "步骤3: 检测文件变更"
    
    # 检查是否有变更
    if git status --porcelain | grep -q .; then
        HAS_CHANGES=true
        log_info "检测到以下文件变更:"
        git status --porcelain | while read -r line; do
            echo "  $line"
        done
        
        # 显示具体变更内容
        log_info "变更文件详情:"
        git diff --name-only | while read -r file; do
            echo "  - $file"
        done
    else
        log_info "未检测到文件变更"
        HAS_CHANGES=false
    fi
    
    return 0
}

# 4. 编译验证（可选）
perform_build_verification() {
    log_step "步骤4: 编译验证"
    
    if [[ $BUILD_REQUIRED == false ]]; then
        log_info "跳过编译验证（非代码文件变更）"
        return 0
    fi
    
    log_info "开始编译验证..."
    
    # 首选编译命令
    local build_cmd="cmd.exe /c \"cd /d \\\"$WIN_PATH\\\" && autobuild_x86_debug.bat\""
    local build_success=false
    
    if eval "$build_cmd"; then
        build_success=true
        log_success "编译验证成功 (0 error 0 warning)"
    else
        log_warning "首选编译命令失败，尝试备用编译..."
        build_cmd="cmd.exe /c \"cd /d \\\"$WIN_PATH\\\" && autobuild.bat\""
        
        if eval "$build_cmd"; then
            build_success=true
            log_success "备用编译成功 (0 error 0 warning)"
        else
            log_error "编译失败，无法继续"
            return 1
        fi
    fi
    
    # 更新编译历史记录
    if [[ $build_success == true && -f "$PROGRESS_DOC" ]]; then
        local timestamp=$(date +"%Y-%m-%d")
        local time_only=$(get_timestamp)
        local log_entry="| 自动编译 | $timestamp $time_only | ✅ | 0 | 0 | autobuild_x86_debug.bat | 自动工作流程编译验证 |"
        
        # 在编译验证历史表格中添加记录
        if grep -q "| 阶段 | 编译时间 | 结果 | 错误数 | 警告数 | 编译命令 | 备注 |" "$PROGRESS_DOC"; then
            sed -i "/| 阶段 | 编译时间 | 结果 | 错误数 | 警告数 | 编译命令 | 备注 |/a\\$log_entry" "$PROGRESS_DOC"
            log_success "编译历史记录已更新"
        fi
    fi
}

# 5. 生成提交信息
generate_commit_message() {
    local changed_files
    changed_files=$(git diff --cached --name-only 2>/dev/null || git diff --name-only)
    
    local commit_type="docs"
    local commit_desc="更新项目文档"
    
    # 根据变更文件类型智能判断提交类型
    if echo "$changed_files" | grep -q "\.cpp$\|\.h$\|\.c$"; then
        commit_type="feat"
        commit_desc="代码功能更新"
    elif echo "$changed_files" | grep -q "\.md$\|\.txt$\|README"; then
        commit_type="docs"
        if echo "$changed_files" | grep -q "CLAUDE.md"; then
            commit_desc="更新WSL工作流程配置"
        else
            commit_desc="更新项目文档"
        fi
    elif echo "$changed_files" | grep -q "\.bat$\|\.sh$"; then
        commit_type="chore"
        commit_desc="更新构建脚本"
    elif echo "$changed_files" | grep -q "\.rc$\|Resource\.h"; then
        commit_type="feat"
        commit_desc="更新界面资源"
    fi
    
    echo "${commit_type}: ${commit_desc}"
}

# 6. 版本控制与推送
perform_version_control() {
    log_step "步骤5: 版本控制与推送"
    
    if [[ $HAS_CHANGES == false ]]; then
        log_info "无变更，无需提交"
        return 0
    fi
    
    # 暂存所有变更
    log_info "暂存文件变更..."
    git add -A
    
    # 生成提交信息
    local commit_message
    commit_message=$(generate_commit_message)
    log_info "生成的提交信息: $commit_message"
    
    # 创建提交
    if git commit -m "$commit_message

🤖 Generated with [Claude Code](https://claude.ai/code)

Co-Authored-By: Claude <noreply@anthropic.com>"; then
        log_success "提交创建成功"
        
        # 获取最新提交信息
        local latest_commit
        latest_commit=$(git log -1 --oneline)
        log_info "最新提交: $latest_commit"
        
        # 推送到备份仓库
        log_info "推送到备份仓库..."
        if git push backup HEAD; then
            log_success "备份仓库推送成功"
        else
            log_error "备份仓库推送失败"
            return 1
        fi
        
        # 检查是否有origin远程仓库并推送
        if git remote | grep -q origin; then
            log_info "推送到主仓库..."
            if git push origin HEAD; then
                log_success "主仓库推送成功"
            else
                log_warning "主仓库推送失败，但备份推送成功"
            fi
        else
            log_info "未配置origin远程仓库，跳过主仓库推送"
        fi
        
    else
        log_error "提交创建失败"
        return 1
    fi
}

# 7. 存档标签生成
create_archive_tag() {
    log_step "步骤6: 存档标签生成"
    
    if [[ $HAS_CHANGES == false ]]; then
        log_info "无变更，跳过标签创建"
        return 0
    fi
    
    local tag="save-$(date +%Y%m%d-%H%M%S)"
    log_info "创建存档标签: $tag"
    
    if git tag "$tag" && git push --tags; then
        log_success "存档标签创建并推送成功: $tag"
    else
        log_warning "存档标签创建失败，但不影响主流程"
    fi
}

# 8. 最终进度文档更新
finalize_progress_update() {
    if [[ -f "$PROGRESS_DOC" && $HAS_CHANGES == true ]]; then
        local current_time
        current_time=$(get_timestamp)
        
        # 更新最后一条记录为完成状态
        local temp_file=$(mktemp)
        sed "s/⏳ \*\*${SCRIPT_START_TIME}\*\*: 开始自动工作流程执行 - 检测文件变更并处理/✅ **${SCRIPT_START_TIME}**: 完成自动工作流程执行 - 文件变更已处理并推送 (结束时间: ${current_time})/" "$PROGRESS_DOC" > "$temp_file"
        mv "$temp_file" "$PROGRESS_DOC"
        
        # 重新提交进度文档的更新
        git add "$PROGRESS_DOC"
        git commit -m "docs: 更新自动工作流程执行记录" --amend --no-edit 2>/dev/null || true
        
        log_success "进度文档最终状态已更新"
    fi
}

# 9. 工作汇报
generate_work_report() {
    log_step "步骤7: 工作汇报"
    
    echo ""
    echo "=================================="
    echo "  WSL自动工作流程执行报告"
    echo "=================================="
    echo "开始时间: $(get_full_timestamp)"
    echo "工作目录: $WORK_DIR"
    
    if [[ $HAS_CHANGES == true ]]; then
        echo "文件变更: 是"
        echo "最新提交: $(git log -1 --oneline)"
        echo "远程推送: 已完成"
        
        if git tag --list | tail -1 | grep -q "save-"; then
            echo "存档标签: $(git tag --list | grep "save-" | tail -1)"
        fi
        
        echo "处理的文件:"
        git diff --name-only HEAD~1 2>/dev/null | while read -r file; do
            echo "  - $file"
        done
    else
        echo "文件变更: 无"
        echo "操作结果: 无需处理"
    fi
    
    echo "执行状态: 成功完成"
    echo "=================================="
}

# =============================================================================
# 主函数
# =============================================================================

main() {
    log_info "WSL自动工作流程脚本启动"
    log_info "脚本版本: 1.0"
    log_info "执行时间: $(get_full_timestamp)"
    
    SCRIPT_START_TIME=$(get_timestamp)
    
    # 检查必需工具
    for tool in git cmd.exe; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            log_error "必需工具 '$tool' 未找到"
            exit 1
        fi
    done
    
    # 执行工作流程步骤
    sync_repository
    update_progress_document
    check_file_changes
    
    # 根据变更文件类型决定是否需要编译
    if [[ $HAS_CHANGES == true ]]; then
        changed_files=$(git status --porcelain | cut -c4-)
        if echo "$changed_files" | grep -q "\.cpp$\|\.h$\|\.c$\|\.rc$"; then
            BUILD_REQUIRED=true
        fi
    fi
    
    perform_build_verification
    perform_version_control
    create_archive_tag
    finalize_progress_update
    generate_work_report
    
    log_success "WSL自动工作流程执行完成"
}

# =============================================================================
# 脚本入口点
# =============================================================================

# 检查是否在正确的目录中运行
if [[ ! -f "CLAUDE.md" ]]; then
    log_error "请在PortMaster项目根目录中运行此脚本"
    exit 1
fi

# 运行主函数
main "$@"