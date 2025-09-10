#!/bin/bash

#=============================================================================
# PortMaster WSL环境自动工作流程脚本 V2.0
# 功能：自动检测文件变更、编译验证、进度更新、版本控制推送
# 环境：WSL2 + Windows 文件系统
# 作者：Claude Code
# 版本：2.0 - 修复分叉问题、改进推送逻辑、支持自动化触发
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
readonly LOCK_FILE="$WORK_DIR/.workflow_lock"

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
DAEMON_MODE=false

# =============================================================================
# 工具函数
# =============================================================================

# 打印带颜色的消息
log_info() {
    echo -e "${BLUE}[INFO]${NC} $(date '+%H:%M:%S') $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $(date '+%H:%M:%S') $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $(date '+%H:%M:%S') $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $(date '+%H:%M:%S') $1"
}

log_step() {
    echo -e "${PURPLE}[STEP]${NC} $(date '+%H:%M:%S') $1"
}

# 获取当前时间
get_timestamp() {
    date +"%H:%M"
}

get_full_timestamp() {
    date +"%Y-%m-%d %H:%M:%S"
}

# 检查并创建锁文件
acquire_lock() {
    if [[ -f "$LOCK_FILE" ]]; then
        local lock_pid
        lock_pid=$(cat "$LOCK_FILE" 2>/dev/null || echo "")
        if [[ -n "$lock_pid" ]] && kill -0 "$lock_pid" 2>/dev/null; then
            log_warning "工作流程正在运行 (PID: $lock_pid)，跳过执行"
            exit 0
        else
            log_info "清理过期的锁文件"
            rm -f "$LOCK_FILE"
        fi
    fi
    echo $$ > "$LOCK_FILE"
    log_info "获取工作流程锁 (PID: $$)"
}

# 释放锁文件
release_lock() {
    if [[ -f "$LOCK_FILE" ]]; then
        rm -f "$LOCK_FILE"
        log_info "释放工作流程锁"
    fi
}

# 错误处理函数
cleanup_on_error() {
    log_error "脚本执行失败，正在清理..."
    release_lock
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

# 注册错误处理和退出清理
trap cleanup_on_error ERR
trap release_lock EXIT

# =============================================================================
# 核心工作流程函数
# =============================================================================

# 1. 环境准备与同步（改进版 - 防止分叉）
sync_repository() {
    log_step "步骤1: 环境准备与同步"
    
    # 切换到工作目录
    if ! cd "$WORK_DIR"; then
        log_error "无法访问工作目录: $WORK_DIR"
        exit 1
    fi
    
    log_info "当前工作目录: $(pwd)"
    
    # 获取所有远程仓库
    local remotes
    remotes=$(git remote 2>/dev/null || echo "")
    
    if [[ -n "$remotes" ]]; then
        # 同步所有远程仓库
        for remote in $remotes; do
            log_info "从远程仓库 '$remote' 获取最新变更..."
            if git fetch "$remote" 2>/dev/null; then
                log_success "远程仓库 '$remote' 同步成功"
            else
                log_warning "远程仓库 '$remote' 同步失败"
            fi
        done
        
        # 检查本地是否有未提交的变更
        if git diff --quiet && git diff --cached --quiet; then
            # 使用rebase方式合并远程变更，避免产生merge commit
            local main_remote
            main_remote=$(git remote | head -1)  # 使用第一个远程仓库
            
            if git merge-base --is-ancestor HEAD "$main_remote/main" 2>/dev/null; then
                log_info "本地版本是最新的"
            elif git merge-base --is-ancestor "$main_remote/main" HEAD 2>/dev/null; then
                log_info "本地版本领先于远程版本"
            else
                log_info "使用rebase方式同步远程变更..."
                if git rebase "$main_remote/main" 2>/dev/null; then
                    log_success "远程变更同步完成"
                else
                    log_error "同步远程变更失败，存在冲突需要手动解决"
                    exit 1
                fi
            fi
        else
            log_info "本地有未提交变更，跳过远程同步"
        fi
    else
        log_warning "未配置远程仓库"
    fi
}

# 2. 进度文档更新（改进版）
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
    
    # 在执行日志部分添加记录
    if grep -q "## 📝 执行日志" "$PROGRESS_DOC"; then
        # 在执行日志标题后添加新记录
        sed -i "/## 📝 执行日志/a\\$temp_update" "$PROGRESS_DOC"
        log_success "进度文档已更新: $temp_update"
    else
        log_warning "进度文档格式不匹配，跳过自动更新"
    fi
}

# 3. 检测文件变更（增强版）
check_file_changes() {
    log_step "步骤3: 检测文件变更"
    
    # 检查是否有变更
    if git status --porcelain | grep -q .; then
        HAS_CHANGES=true
        log_info "检测到以下文件变更:"
        git status --porcelain | while read -r line; do
            echo "  $line"
        done
        
        # 显示具体变更内容并分析变更类型
        log_info "变更文件详情:"
        local changed_files
        changed_files=$(git status --porcelain | sed 's/^...//' | sort)
        
        local code_changed=false
        local doc_changed=false
        local resource_changed=false
        
        while IFS= read -r file; do
            echo "  - $file"
            case "$file" in
                *.cpp|*.h|*.c|*.hpp)
                    code_changed=true
                    ;;
                *.md|*.txt|README*)
                    doc_changed=true
                    ;;
                *.rc|Resource.h|*.ico|*.png|*.bmp)
                    resource_changed=true
                    ;;
                *.bat|*.sh)
                    resource_changed=true
                    ;;
            esac
        done <<< "$changed_files"
        
        # 根据变更类型决定是否需要编译
        if [[ $code_changed == true || $resource_changed == true ]]; then
            BUILD_REQUIRED=true
            log_info "检测到代码或资源变更，需要编译验证"
        else
            log_info "仅检测到文档变更，跳过编译验证"
        fi
        
    else
        log_info "未检测到文件变更"
        HAS_CHANGES=false
    fi
    
    return 0
}

# 4. 编译验证（增强版）
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
    local build_log=""
    
    log_info "执行编译命令: autobuild_x86_debug.bat"
    if build_log=$(eval "$build_cmd" 2>&1); then
        if echo "$build_log" | grep -q "0 error.*0 warning"; then
            build_success=true
            log_success "编译验证成功 (0 error 0 warning)"
        else
            log_warning "编译完成但可能有警告，检查输出..."
            echo "$build_log" | tail -10
        fi
    else
        log_warning "首选编译命令失败，尝试备用编译..."
        build_cmd="cmd.exe /c \"cd /d \\\"$WIN_PATH\\\" && autobuild.bat\""
        
        if build_log=$(eval "$build_cmd" 2>&1); then
            if echo "$build_log" | grep -q "0 error.*0 warning"; then
                build_success=true
                log_success "备用编译成功 (0 error 0 warning)"
            else
                log_error "编译失败或有错误/警告"
                echo "$build_log" | tail -10
                return 1
            fi
        else
            log_error "编译失败，无法继续"
            echo "$build_log" | tail -10
            return 1
        fi
    fi
    
    # 更新编译历史记录（但不立即提交）
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

# 5. 生成提交信息（增强版）
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
        elif echo "$changed_files" | grep -q "Code_Revision_Progress.md"; then
            commit_desc="更新项目进度记录"
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

# 6. 版本控制与推送（改进版 - 防止分叉）
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
    
    # 创建提交（避免使用amend防止分叉）
    if git commit -m "$commit_message

🤖 Generated with [Claude Code](https://claude.ai/code)

Co-Authored-By: Claude <noreply@anthropic.com>"; then
        log_success "提交创建成功"
        
        # 获取最新提交信息
        local latest_commit
        latest_commit=$(git log -1 --oneline)
        log_info "最新提交: $latest_commit"
        
        # 推送到所有远程仓库
        local remotes
        remotes=$(git remote 2>/dev/null || echo "")
        local push_success=false
        
        for remote in $remotes; do
            log_info "推送到远程仓库 '$remote'..."
            if git push "$remote" HEAD 2>/dev/null; then
                log_success "远程仓库 '$remote' 推送成功"
                push_success=true
            else
                log_warning "远程仓库 '$remote' 推送失败"
            fi
        done
        
        if [[ $push_success == false ]]; then
            log_error "所有远程仓库推送都失败"
            return 1
        fi
        
        # 更新进度文档的完成状态（单独提交）
        update_progress_completion
        
    else
        log_error "提交创建失败"
        return 1
    fi
}

# 更新进度文档完成状态
update_progress_completion() {
    if [[ -f "$PROGRESS_DOC" ]]; then
        local current_time
        current_time=$(get_timestamp)
        
        # 查找最近的待处理记录并更新为完成状态
        local temp_file=$(mktemp)
        sed "0,/⏳ \*\*${SCRIPT_START_TIME}\*\*: 开始自动工作流程执行/{s/⏳ \*\*${SCRIPT_START_TIME}\*\*: 开始自动工作流程执行 - 检测文件变更并处理/✅ **${SCRIPT_START_TIME}**: 完成自动工作流程执行 - 文件变更已处理并推送 (结束时间: ${current_time})/}" "$PROGRESS_DOC" > "$temp_file"
        mv "$temp_file" "$PROGRESS_DOC"
        
        # 单独提交进度更新
        if git add "$PROGRESS_DOC" && git commit -m "docs: 更新自动工作流程执行记录

🤖 Generated with [Claude Code](https://claude.ai/code)

Co-Authored-By: Claude <noreply@anthropic.com>" 2>/dev/null; then
            # 推送更新
            local remotes
            remotes=$(git remote 2>/dev/null || echo "")
            for remote in $remotes; do
                git push "$remote" HEAD 2>/dev/null || true
            done
            log_success "进度文档完成状态已更新并推送"
        fi
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
    
    if git tag "$tag"; then
        # 推送标签到所有远程仓库
        local remotes
        remotes=$(git remote 2>/dev/null || echo "")
        local tag_push_success=false
        
        for remote in $remotes; do
            if git push "$remote" "$tag" 2>/dev/null; then
                log_success "标签推送到远程仓库 '$remote' 成功"
                tag_push_success=true
            fi
        done
        
        if [[ $tag_push_success == true ]]; then
            log_success "存档标签创建并推送成功: $tag"
        else
            log_warning "存档标签创建成功但推送失败: $tag"
        fi
    else
        log_warning "存档标签创建失败，但不影响主流程"
    fi
}

# 8. 工作汇报（增强版）
generate_work_report() {
    log_step "步骤7: 工作汇报"
    
    echo ""
    echo "=================================="
    echo "  WSL自动工作流程执行报告 V2.0"
    echo "=================================="
    echo "开始时间: $(get_full_timestamp)"
    echo "工作目录: $WORK_DIR"
    echo "脚本版本: 2.0"
    
    if [[ $HAS_CHANGES == true ]]; then
        echo "文件变更: 是"
        echo "编译验证: $([ $BUILD_REQUIRED == true ] && echo "已执行" || echo "已跳过")"
        echo "最新提交: $(git log -1 --oneline)"
        
        local remotes
        remotes=$(git remote 2>/dev/null || echo "")
        echo "远程推送: 已完成 (推送到: $remotes)"
        
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
# 自动化触发机制
# =============================================================================

# 守护进程模式
daemon_mode() {
    log_info "启动守护进程模式，监控文件变更..."
    local last_check=$(date +%s)
    
    while true; do
        local current_time=$(date +%s)
        
        # 每30秒检查一次文件变更
        if (( current_time - last_check >= 30 )); then
            if git status --porcelain | grep -q .; then
                log_info "检测到文件变更，启动自动工作流程..."
                execute_workflow
                last_check=$(date +%s)
            fi
            last_check=$current_time
        fi
        
        sleep 5
    done
}

# 执行主工作流程
execute_workflow() {
    acquire_lock
    
    SCRIPT_START_TIME=$(get_timestamp)
    
    # 执行工作流程步骤
    sync_repository
    update_progress_document
    check_file_changes
    perform_build_verification
    perform_version_control
    create_archive_tag
    generate_work_report
    
    release_lock
}

# =============================================================================
# 主函数
# =============================================================================

show_usage() {
    cat << EOF
WSL自动工作流程脚本 V2.0

用法:
    $0 [选项]

选项:
    -d, --daemon    启动守护进程模式，持续监控文件变更
    -o, --once      执行一次工作流程（默认）
    -h, --help      显示此帮助信息

示例:
    $0              # 执行一次工作流程
    $0 -d           # 启动守护进程模式
    $0 --once       # 明确指定执行一次
EOF
}

main() {
    # 解析命令行参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            -d|--daemon)
                DAEMON_MODE=true
                shift
                ;;
            -o|--once)
                DAEMON_MODE=false
                shift
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    log_info "WSL自动工作流程脚本启动"
    log_info "脚本版本: 2.0"
    log_info "执行时间: $(get_full_timestamp)"
    log_info "运行模式: $([ $DAEMON_MODE == true ] && echo "守护进程" || echo "单次执行")"
    
    # 检查必需工具
    for tool in git cmd.exe; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            log_error "必需工具 '$tool' 未找到"
            exit 1
        fi
    done
    
    # 检查工作目录
    if [[ ! -d "$WORK_DIR" ]]; then
        log_error "工作目录不存在: $WORK_DIR"
        exit 1
    fi
    
    if [[ ! -f "$WORK_DIR/CLAUDE.md" ]]; then
        log_error "请确保在PortMaster项目根目录中运行此脚本"
        exit 1
    fi
    
    # 执行主逻辑
    if [[ $DAEMON_MODE == true ]]; then
        daemon_mode
    else
        execute_workflow
    fi
    
    log_success "WSL自动工作流程执行完成"
}

# =============================================================================
# 脚本入口点
# =============================================================================

# 运行主函数
main "$@"