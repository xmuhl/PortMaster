#pragma once
#pragma execution_character_set("utf-8")

#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief 数据展示服务（静态工具类）
 *
 * 职责：处理十六进制/文本转换、二进制检测、节流策略等纯粹数据展示逻辑
 * 位置：Common/ 目录
 *
 * 功能说明：
 * - 提供字节数据与十六进制字符串的双向转换
 * - 提供字节数据与文本字符串的双向转换
 * - 检测数据是否为二进制（包含不可打印字符）
 * - 格式化混合显示（Hex + ASCII）
 * - 准备显示更新数据（节流控制）
 *
 * 线程安全性：
 * - 所有方法均为静态纯函数，无状态，天然线程安全
 *
 * 使用示例：
 * @code
 * // 十六进制转换
 * std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
 * std::string hex = DataPresentationService::BytesToHex(data.data(), data.size());
 * // 输出: "48 65 6C 6C 6F"
 *
 * // 二进制检测
 * bool isBinary = DataPresentationService::IsBinaryData(data.data(), data.size());
 *
 * // 混合显示
 * std::string formatted = DataPresentationService::FormatHexAscii(data.data(), data.size());
 * // 输出: "48 65 6C 6C 6F | Hello"
 * @endcode
 */
class DataPresentationService
{
public:
	// ========== 十六进制转换 ==========

	/**
	 * @brief 将字节数据转换为十六进制字符串
	 * @param data 字节数据指针
	 * @param length 数据长度
	 * @return 十六进制字符串（空格分隔），如 "48 65 6C 6C 6F"
	 */
	static std::string BytesToHex(const uint8_t* data, size_t length);

	/**
	 * @brief 将字节向量转换为十六进制字符串
	 * @param data 字节向量
	 * @return 十六进制字符串（空格分隔）
	 */
	static std::string BytesToHex(const std::vector<uint8_t>& data);

	/**
	 * @brief 将十六进制字符串转换为字节数据
	 * @param hex 十六进制字符串（可以带空格、连字符等分隔符）
	 * @return 字节数据向量，解析失败返回空向量
	 *
	 * 说明：
	 * - 支持多种格式：48656C6C6F、48 65 6C 6C 6F、48-65-6C-6C-6F
	 * - 自动跳过非十六进制字符
	 */
	static std::vector<uint8_t> HexToBytes(const std::string& hex);

	// ========== 文本转换 ==========

	/**
	 * @brief 将字节数据转换为文本字符串
	 * @param data 字节向量
	 * @return 文本字符串（UTF-8编码）
	 */
	static std::string BytesToText(const std::vector<uint8_t>& data);

	/**
	 * @brief 将文本字符串转换为字节数据
	 * @param text 文本字符串（UTF-8编码）
	 * @return 字节数据向量
	 */
	static std::vector<uint8_t> TextToBytes(const std::string& text);

	// ========== 二进制检测 ==========

	/**
	 * @brief 检测数据是否为二进制（包含不可打印字符）
	 * @param data 字节数据指针
	 * @param length 数据长度
	 * @param threshold 二进制判定阈值（不可打印字符比例，默认0.3即30%）
	 * @return true表示二进制数据，false表示文本数据
	 *
	 * 说明：
	 * - 统计不可打印字符（< 0x20 且非 \r \n \t，或 >= 0x7F）的比例
	 * - 如果比例超过阈值，则判定为二进制
	 */
	static bool IsBinaryData(const uint8_t* data, size_t length, double threshold = 0.3);

	// ========== 编码检测 ==========

	/**
	 * @brief 检测字节数据是否为有效的UTF-8编码
	 * @param data 字节数据指针
	 * @param length 数据长度
	 * @return 如果是有效的UTF-8编码返回true，否则返回false
	 *
	 * 说明：
	 * - 检查UTF-8编码的字节序列是否符合规范
	 * - 支持1-4字节的UTF-8字符
	 * - 用于智能编码检测，决定是否需要进行编码转换
	 */
	static bool IsValidUtf8(const uint8_t* data, size_t length);

	// ========== 混合显示（Hex + ASCII）==========

	/**
	 * @brief 格式化为十六进制+ASCII混合显示
	 * @param data 字节数据指针
	 * @param length 数据长度
	 * @param bytesPerLine 每行显示字节数（默认16）
	 * @return 格式化字符串，类似：
	 *         "48 65 6C 6C 6F 20 57 6F 72 6C 64 | Hello World\n"
	 *
	 * 说明：
	 * - 左侧为十六进制，右侧为ASCII
	 * - 不可打印字符显示为 '.'
	 */
	static std::string FormatHexAscii(const uint8_t* data, size_t length, size_t bytesPerLine = 16);

	// ========== 显示更新准备 ==========

	/**
	 * @brief 显示更新信息结构
	 */
	struct DisplayUpdate
	{
		std::string content;      // 显示内容
		size_t dataSize;          // 数据大小（字节）
		bool isBinary;            // 是否为二进制数据

		DisplayUpdate()
			: dataSize(0), isBinary(false) {
		}
	};

	/**
	 * @brief 准备显示更新数据
	 * @param cache 数据缓存
	 * @param hexMode 是否为十六进制显示模式
	 * @param maxDisplaySize 最大显示数据量（字节），超过则截断（默认64KB）
	 * @return 显示更新信息
	 *
	 * 说明：
	 * - 如果hexMode为true，转换为十六进制显示
	 * - 如果hexMode为false，先检测是否二进制，如果是二进制则使用混合显示
	 * - 如果数据过大，仅显示前maxDisplaySize字节
	 */
	static DisplayUpdate PrepareDisplay(const std::vector<uint8_t>& cache, bool hexMode, size_t maxDisplaySize = 65536);

	// ========== 辅助工具 ==========

	/**
	 * @brief 将单字节转换为两位十六进制字符串
	 * @param byte 字节值
	 * @return 两位十六进制字符串（大写），如 "4F"
	 */
	static std::string ByteToHexString(uint8_t byte);

	/**
	 * @brief 将十六进制字符转换为数值
	 * @param c 十六进制字符（0-9, A-F, a-f）
	 * @param value 输出数值（0-15）
	 * @return 转换是否成功
	 */
	static bool HexCharToValue(char c, uint8_t& value);

	/**
	 * @brief 判断字符是否可打印
	 * @param byte 字节值
	 * @return true表示可打印字符
	 *
	 * 说明：
	 * - 可打印字符：0x20-0x7E
	 * - 特殊可打印：\t \r \n
	 */
	static bool IsPrintable(uint8_t byte);
};
