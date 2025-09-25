#!/bin/bash
# 智能编译脚本 - WSL版本，自动处理进程占用问题

echo "================================================================"
echo "= 智能编译脚本 - WSL环境，自动处理进程占用问题"
echo "================================================================"

WORK_DIR="/mnt/c/Users/huangl/Desktop/PortMaster"
cd "$WORK_DIR"

retry_build() {
    echo ""
    echo "[信息] 开始编译 Win32 Debug 配置..."
    
    # 调用编译脚本并捕获输出
    local build_output
    build_output=$(cmd.exe /c "autobuild_x86_debug.bat" 2>&1)
    local build_result=$?
    
    # 检查编译结果
    if echo "$build_output" | grep -q "已成功生成"; then
        echo "[成功] 编译完成 - Debug配置构建成功"
        echo "$build_output" | grep "个警告"
        echo "$build_output" | grep "个错误"
        return 0
    fi
    
    # 检查是否为进程占用错误
    if echo "$build_output" | grep -q "LNK1104" && echo "$build_output" | grep -q "无法打开文件"; then
        echo ""
        echo "[警告] 检测到文件被占用，可能是程序进程在运行"
        echo "[处理] 尝试关闭 PortMaster.exe 进程..."
        
        # 检查进程是否存在
        local process_check
        process_check=$(cmd.exe /c "tasklist /FI \"IMAGENAME eq PortMaster.exe\"" 2>/dev/null)
        
        if echo "$process_check" | grep -i "PortMaster.exe" > /dev/null; then
            echo "[信息] 发现 PortMaster.exe 进程，正在关闭..."
            cmd.exe /c "taskkill /F /IM PortMaster.exe" > /dev/null 2>&1
            
            if [ $? -eq 0 ]; then
                echo "[成功] PortMaster.exe 进程已关闭"
                echo "[信息] 等待2秒后重新编译..."
                sleep 2
                # 递归重试
                retry_build
                return $?
            else
                echo "[错误] 无法关闭 PortMaster.exe 进程"
            fi
        else
            echo "[信息] 未发现 PortMaster.exe 进程，可能是其他文件占用"
        fi
    fi
    
    # 如果不是进程占用问题，显示完整错误信息
    echo ""
    echo "[错误] 编译失败，错误信息："
    echo "$build_output" | grep -i "error\|fatal"
    echo ""
    echo "[详情] 完整编译日志："
    echo "$build_output"
    return 1
}

# 执行编译
if retry_build; then
    echo ""
    echo "================================================================"
    echo "= 编译成功完成"
    echo "================================================================"
    exit 0
else
    echo ""
    echo "================================================================"
    echo "= 编译失败"
    echo "================================================================"
    exit 1
fi
