#pragma once
#pragma execution_character_set("utf-8")

#include <string>
#include <vector>

/**
 * @brief 字符串转换工具类（静态工具类）
 *
 * 职责：提供安全的字符串编码转换功能
 * 位置：Common/ 目录
 *
 * 功能说明：
 * - 提供安全的宽字符（UTF-16）到UTF-8转换
 * - 封装Windows API WideCharToMultiByte，避免缓冲区溢出
 * - 使用std::vector作为缓冲区，自动管理内存
 *
 * 线程安全性：
 * - 所有方法均为静态纯函数，无状态，天然线程安全
 *
 * 使用示例：
 * @code
 * // 宽字符转UTF-8
 * std::wstring wideStr = L"测试字符串";
 * std::string utf8Str = StringUtils::Utf8EncodeWide(wideStr);
 * @endcode
 */
class StringUtils
{
public:
	// ========== UTF-8编码转换 ==========

	/**
	 * @brief 将宽字符字符串（UTF-16）安全转换为UTF-8字符串
	 * @param wideStr 宽字符字符串
	 * @return UTF-8字符串，转换失败返回空字符串
	 *
	 * 说明：
	 * - 使用WideCharToMultiByte进行转换
	 * - 使用std::vector<char>作为缓冲区，避免手动resize导致的越界风险
	 * - 严格验证API返回值，确保转换安全性
	 * - 自动处理空字符串情况
	 */
	static std::string Utf8EncodeWide(const std::wstring& wideStr);

	/**
	 * @brief 将UTF-8字符串转换为宽字符字符串（UTF-16）
	 * @param utf8Str UTF-8字符串
	 * @return 宽字符字符串，转换失败返回空字符串
	 *
	 * 说明：
	 * - 使用MultiByteToWideChar进行转换
	 * - 使用std::vector<wchar_t>作为缓冲区，确保内存安全
	 * - 严格验证API返回值，确保转换安全性
	 * - 自动处理空字符串情况
	 */
	static std::wstring WideEncodeUtf8(const std::string& utf8Str);

	// ========== 辅助工具 ==========

	/**
	 * @brief 检查字符串是否为有效的UTF-8编码
	 * @param str 待检查的字符串
	 * @return true表示有效的UTF-8编码，false表示无效
	 *
	 * 说明：
	 * - 检查UTF-8字节序列的合法性
	 * - 验证多字节字符的连续性
	 * - 检查是否包含非法的编码字节
	 */
	static bool IsValidUtf8(const std::string& str);

	/**
	 * @brief 安全的字符串截断（避免切断多字节UTF-8字符）
	 * @param str 原始字符串
	 * @param maxLength 最大长度（字符数，非字节数）
	 * @return 截断后的字符串
	 *
	 * 说明：
	 * - 确保不会在多字节UTF-8字符中间截断
	 * - 如果截断点位于多字节字符中间，回退到字符边界
	 * - 保持UTF-8编码的完整性
	 */
	static std::string SafeTruncateUtf8(const std::string& str, size_t maxLength);

	// ========== 通用安全转换（支持指定代码页） ==========

	/**
	 * @brief 将多字节字符串安全转换为宽字符字符串（支持指定代码页）
	 * @param input 输入多字节字符串
	 * @param codePage 代码页（默认CP_UTF8，可指定CP_ACP等）
	 * @return 宽字符字符串，转换失败返回空字符串
	 *
	 * 说明：
	 * - 使用MultiByteToWideChar进行转换
	 * - 支持指定任意代码页（建议使用CP_UTF8）
	 * - 完整的返回值检查和异常保护
	 * - 使用std::vector作为缓冲区，确保内存安全
	 * - 大文件支持：自动处理大数据量的内存分配
	 *
	 * 注意：
	 * - 优先使用WideEncodeUtf8()方法（默认UTF-8编码）
	 * - 仅在需要处理非UTF-8编码的遗留数据时使用此方法
	 */
	static std::wstring SafeMultiByteToWideChar(const std::string& input, UINT codePage = CP_UTF8);

	/**
	 * @brief 将宽字符字符串安全转换为多字节字符串（支持指定代码页）
	 * @param input 输入宽字符字符串
	 * @param codePage 目标代码页（默认CP_UTF8，可指定CP_ACP等）
	 * @return 多字节字符串，转换失败返回空字符串
	 *
	 * 说明：
	 * - 使用WideCharToMultiByte进行转换
	 * - 支持指定任意代码页（建议使用CP_UTF8）
	 * - 完整的返回值检查和异常保护
	 * - 使用std::vector作为缓冲区，确保内存安全
	 * - 大文件支持：自动处理大数据量的内存分配
	 *
	 * 注意：
	 * - 优先使用Utf8EncodeWide()方法（默认UTF-8编码）
	 * - 仅在需要输出非UTF-8编码时使用此方法
	 */
	static std::string SafeWideCharToMultiByte(const std::wstring& input, UINT codePage = CP_UTF8);

	// ========== 安全性检查 ==========

	/**
	 * @brief 检查字符串长度是否在安全范围内
	 * @param str 待检查的字符串
	 * @param maxLength 最大允许长度（字节数，默认1MB）
	 * @return true表示长度安全，false表示超过限制
	 *
	 * 说明：
	 * - 用于在转换前检查字符串长度，避免内存分配失败
	 * - 默认最大长度为1MB，可根据需要调整
	 * - 建议在处理大文件或用户输入时使用
	 */
	static bool IsStringLengthSafe(const std::string& str, size_t maxLength = 1024 * 1024);

	/**
	 * @brief 检查宽字符串长度是否在安全范围内
	 * @param str 待检查的宽字符串
	 * @param maxLength 最大允许长度（字符数，默认512K字符）
	 * @return true表示长度安全，false表示超过限制
	 *
	 * 说明：
	 * - 用于在转换前检查宽字符串长度，避免内存分配失败
	 * - 默认最大长度为512K字符（约1MB UTF-16数据）
	 * - 建议在处理大文件或用户输入时使用
	 */
	static bool IsStringLengthSafe(const std::wstring& str, size_t maxLength = 512 * 1024);
};