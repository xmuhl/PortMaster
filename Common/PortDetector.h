#pragma once
#pragma execution_character_set("utf-8")

#include "CommonTypes.h"
#include <Windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <vector>
#include <string>
#include <memory>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

/**
 * @brief 设备检测器
 *
 * 职责：封装Windows设备检测功能，提供统一的设备枚举、状态检测、信息获取接口
 * 位置：Common/ 目录
 *
 * 功能说明：
 * - 使用SetupDi API枚举串口、并口、USB设备
 * - 查询设备详细信息（友好名称、硬件ID、制造商等）
 * - 检测设备连接状态和配置状态
 * - 提供设备预检测功能，避免无效连接尝试
 *
 * 兼容性：
 * - 支持Windows 7及以上系统
 * - 通过条件编译处理不同版本的API差异
 * - 降级策略：API不可用时自动回退到基础检测
 *
 * 使用示例：
 * @code
 * // 枚举所有可用设备
 * auto devices = PortDetector::EnumerateAllDevices();
 * for (const auto& device : devices) {
 *     std::cout << device.friendlyName << " - " << device.portName << std::endl;
 * }
 *
 * // 检测指定端口状态
 * PortStatus status = PortDetector::CheckDeviceStatus("COM3");
 * if (status == PortStatus::Connected) {
 *     std::cout << "设备已连接" << std::endl;
 * }
 * @endcode
 */
class PortDetector
{
public:
	// 禁止实例化
	PortDetector() = delete;
	~PortDetector() = delete;

	// ========== 设备枚举接口 ==========

	/**
	 * @brief 枚举所有类型的所有可用设备
	 * @return 设备信息列表
	 *
	 * 说明：
	 * - 包括串口、并口、USB设备
	 * - 仅返回状态为Available或Connected的设备
	 * - 过滤禁用或错误的设备
	 */
	static std::vector<DeviceInfo> EnumerateAllDevices();

	/**
	 * @brief 枚举指定类型的设备
	 * @param portType 端口类型
	 * @return 设备信息列表
	 */
	static std::vector<DeviceInfo> EnumerateDevicesByType(PortType portType);

	/**
	 * @brief 枚举串口设备（COM端口）
	 * @return 串口设备列表
	 */
	static std::vector<DeviceInfo> EnumerateSerialPorts();

	/**
	 * @brief 枚举并口设备（LPT端口）
	 * @return 并口设备列表
	 */
	static std::vector<DeviceInfo> EnumerateParallelPorts();

	/**
	 * @brief 枚举USB打印设备
	 * @return USB设备列表
	 */
	static std::vector<DeviceInfo> EnumerateUsbPrintDevices();

	// ========== 设备状态检测接口 ==========

	/**
	 * @brief 检测指定端口的设备状态
	 * @param portName 端口名称（如"COM3"、"LPT1"）
	 * @return 设备状态枚举值
	 *
	 * 状态说明：
	 * - Unknown: 设备不存在或无法检测
	 * - Available: 端口存在且可用
	 * - Connected: 设备已连接
	 * - Busy: 设备忙碌
	 * - Offline: 设备离线
	 * - Error: 设备错误（未配置驱动、被禁用等）
	 */
	static PortStatus CheckDeviceStatus(const std::string& portName);

	/**
	 * @brief 检测指定设备是否实际连接
	 * @param device 设备信息
	 * @return 是否已连接
	 *
	 * 说明：
	 * - 不仅检查端口是否存在，更关注设备实际连接状态
	 * - 对于串口：通过检查握手信号判断
	 * - 对于并口/USB：通过设备实例状态判断
	 */
	static bool IsDeviceConnected(const DeviceInfo& device);

	/**
	 * @brief 快速预检测设备是否可用
	 * @param portName 端口名称
	 * @param portType 端口类型
	 * @return 预检测结果（仅用于快速筛选，不完全准确）
	 *
	 * 说明：
	 * - 使用轻量级检测，速度快
	 * - 用于连接前快速筛选
	 * - 检测失败不代表真正无法连接，需要进一步验证
	 */
	static bool QuickCheckDevice(const std::string& portName, PortType portType);

	// ========== 设备详细信息接口 ==========

	/**
	 * @brief 获取设备详细信息
	 * @param device 设备信息（将填充详细信息）
	 * @return 是否成功获取
	 *
	 * 说明：
	 * - 补充friendlyName、hardwareId、manufacturer等信息
	 * - 查询设备注册表获取详细信息
	 * - 检查设备驱动配置状态
	 */
	static bool GetDeviceDetails(DeviceInfo& device);

	/**
	 * @brief 查找指定端口对应的设备信息
	 * @param portName 端口名称
	 * @return 设备信息，如果未找到返回空对象
	 *
	 * 说明：
	 * - 在已枚举的设备中查找匹配项
	 * - 未找到返回默认构造的DeviceInfo对象
	 */
	static DeviceInfo FindDeviceByPortName(const std::string& portName);

	// ========== 辅助接口 ==========

	/**
	 * @brief 将PortStatus转换为可读字符串
	 * @param status 端口状态
	 * @return 状态描述字符串
	 */
	static std::string StatusToString(PortStatus status);

	/**
	 * @brief 获取端口类型的显示名称
	 * @param portType 端口类型
	 * @return 显示名称字符串
	 */
	static std::string PortTypeToString(PortType portType);

private:
	// ========== 内部辅助方法 ==========

	/**
	 * @brief 初始化Windows设备检测环境
	 * @return 是否成功
	 *
	 * 说明：
	 * - 预分配缓冲区
	 * - 设置错误处理
	 * - 仅在第一次调用时执行
	 */
	static bool InitializeEnvironment();

	/**
	 * @brief 从设备路径获取USB端口号（通过注册表查询）
	 * @param devicePath 实际设备路径
	 * @return 端口号，失败返回 -1
	 *
	 * 说明：
	 * - 查询注册表HKLM\SYSTEM\CurrentControlSet\Control\DeviceClasses获取Port Number
	 * - 转换设备路径格式以匹配注册表键名
	 * - 支持Windows 7及以上系统
	 */
	static int GetUsbPortNumberFromRegistry(const std::string& devicePath);

	/**
	 * @brief 清理Windows设备检测环境
	 *
	 * 说明：
	 * - 释放缓冲区
	 * - 清理资源
	 * - 程序退出时自动调用
	 */
	static void CleanupEnvironment();

	/**
	 * @brief 枚举设备通用方法
	 * @param deviceInfoSet 设备信息集句柄
	 * @param deviceInterfaceGuid 设备接口GUID（可选）
	 * @param deviceClassGuid 设备类GUID（可选）
	 * @param devices 输出参数，设备列表
	 * @param portType 端口类型
	 * @return 是否成功
	 */
	static bool EnumerateDevicesInternal(
		HDEVINFO deviceInfoSet,
		const GUID* deviceInterfaceGuid,
		const GUID* deviceClassGuid,
		std::vector<DeviceInfo>& devices,
		PortType portType
	);

	/**
	 * @brief 获取设备注册表属性
	 * @param deviceInfoSet 设备信息集
	 * @param deviceInfoData 设备信息数据
	 * @param property 属性标识（SPDRP_*）
	 * @param value 输出参数，属性值
	 * @return 是否成功获取
	 */
	static bool GetDeviceRegistryProperty(
		HDEVINFO deviceInfoSet,
		PSP_DEVINFO_DATA deviceInfoData,
		DWORD property,
		std::string& value
	);

	/**
	 * @brief 从设备路径提取端口名称
	 * @param devicePath 设备路径
	 * @param portType 端口类型
	 * @return 端口名称（如"COM3"），如果无法提取返回空字符串
	 */
	static std::string ExtractPortNameFromPath(const std::string& devicePath, PortType portType);

	/**
	 * @brief 检测设备连接状态（Windows 7兼容版本）
	 * @param deviceInstanceId 设备实例ID
	 * @return 连接状态
	 *
	 * 说明：
	 * - 使用CM_* API检测设备状态
	 * - 检查DN_DEVICE_CONNECTED标志
	 * - 支持Windows 7的API行为
	 */
	static PortStatus DetectDeviceConnectionStatus(const std::string& deviceInstanceId);

	/**
	 * @brief 尝试打开设备句柄（快速检测）
	 * @param portName 端口名称
	 * @param portType 端口类型
	 * @param accessMode 访问模式
	 * @param shareMode 共享模式
	 * @return 设备句柄，失败返回INVALID_HANDLE_VALUE
	 */
	static HANDLE TryOpenDeviceHandle(
		const std::string& portName,
		PortType portType,
		DWORD accessMode = GENERIC_READ | GENERIC_WRITE,
		DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE
	);

	// 静态成员变量
	static bool s_initialized;                    // 是否已初始化
	static std::vector<DeviceInfo> s_cachedDevices; // 缓存的设备列表
};
