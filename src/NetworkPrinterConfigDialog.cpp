// NetworkPrinterConfigDialog.cpp - 网络打印机配置对话框实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "NetworkPrinterConfigDialog.h"
#include "afxdialogex.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>

// NetworkPrinterConfigDialog 对话框

IMPLEMENT_DYNAMIC(NetworkPrinterConfigDialog, CDialogEx)

NetworkPrinterConfigDialog::NetworkPrinterConfigDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(1051, pParent)
{
	m_currentConfig = NetworkPrintConfig();
}

NetworkPrinterConfigDialog::~NetworkPrinterConfigDialog()
{
}

void NetworkPrinterConfigDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Control(pDX, 1052, m_editIpAddress);
	DDX_Control(pDX, 1053, m_editPort);
	DDX_Control(pDX, 1054, m_comboProtocol);
	DDX_Control(pDX, 1055, m_buttonTestConnection);
}

BOOL NetworkPrinterConfigDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置协议选项
	m_comboProtocol.AddString(_T("RAW (9100)"));
	m_comboProtocol.AddString(_T("LPR/LPD"));
	m_comboProtocol.AddString(_T("IPP"));

	// 设置默认值
	m_editIpAddress.SetWindowText(CString(m_currentConfig.hostname.c_str()));
	CString portStr;
	portStr.Format(_T("%d"), m_currentConfig.port);
	m_editPort.SetWindowText(portStr);
	m_comboProtocol.SetCurSel(static_cast<int>(m_currentConfig.protocol));

	// 初始化Winsock（仅在需要时）
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
		// 成功初始化Winsock
	}

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

BEGIN_MESSAGE_MAP(NetworkPrinterConfigDialog, CDialogEx)
	ON_BN_CLICKED(1055, &NetworkPrinterConfigDialog::OnBnClickedButtonTestConnection)
	ON_EN_CHANGE(1052, &NetworkPrinterConfigDialog::OnEnChangeEditIpAddress)
	ON_EN_CHANGE(1053, &NetworkPrinterConfigDialog::OnEnChangeEditPort)
END_MESSAGE_MAP()

// 设置网络配置
void NetworkPrinterConfigDialog::SetNetworkConfig(const NetworkPrintConfig& config)
{
	m_currentConfig = config;
}

// 获取网络配置
NetworkPrintConfig NetworkPrinterConfigDialog::GetNetworkConfig() const
{
	// 从控件获取值并更新配置
	return m_currentConfig;
}

// 测试连接按钮
void NetworkPrinterConfigDialog::OnBnClickedButtonTestConnection()
{
	// 禁用测试按钮防止重复点击
	m_buttonTestConnection.EnableWindow(FALSE);

	// 验证输入
	std::string errorMessage;
	if (!ValidateInputs(errorMessage)) {
		MessageBox(CString(errorMessage.c_str()), _T("输入错误"), MB_OK | MB_ICONERROR);
		m_buttonTestConnection.EnableWindow(TRUE);
		return;
	}

	// 执行连接测试
	if (TestConnection(m_currentConfig, errorMessage)) {
		MessageBox(_T("连接测试成功！"), _T("测试结果"), MB_OK | MB_ICONINFORMATION);
	} else {
		CString errorMsg = _T("连接测试失败：\n");
		errorMsg += CString(errorMessage.c_str());
		MessageBox(errorMsg, _T("测试结果"), MB_OK | MB_ICONWARNING);
	}

	// 恢复测试按钮
	m_buttonTestConnection.EnableWindow(TRUE);
}

// IP地址变化处理
void NetworkPrinterConfigDialog::OnEnChangeEditIpAddress()
{
	CString ipText;
	m_editIpAddress.GetWindowText(ipText);
	CT2A utf8Ip(ipText, CP_UTF8);
	m_currentConfig.hostname = std::string(utf8Ip);
}

// 端口变化处理
void NetworkPrinterConfigDialog::OnEnChangeEditPort()
{
	CString portText;
	m_editPort.GetWindowText(portText);
	int port = _ttoi(portText);
	m_currentConfig.port = static_cast<WORD>(port);
}

// 验证输入
bool NetworkPrinterConfigDialog::ValidateInputs(std::string& errorMessage)
{
	// 验证IP地址
	CString ipText;
	m_editIpAddress.GetWindowText(ipText);
	if (ipText.IsEmpty()) {
		errorMessage = "IP地址不能为空";
		return false;
	}

	CT2A utf8Ip(ipText, CP_UTF8);
	if (!IsValidIpAddress(std::string(utf8Ip))) {
		errorMessage = "IP地址格式无效";
		return false;
	}

	// 验证端口号
	CString portText;
	m_editPort.GetWindowText(portText);
	if (portText.IsEmpty()) {
		errorMessage = "端口号不能为空";
		return false;
	}

	int port = _ttoi(portText);
	if (!IsValidPort(port)) {
		errorMessage = "端口号必须在1-65535范围内";
		return false;
	}

	m_currentConfig.hostname = std::string(utf8Ip);
	m_currentConfig.port = static_cast<WORD>(port);

	// 设置协议类型
	int protocolIndex = m_comboProtocol.GetCurSel();
	m_currentConfig.protocol = static_cast<NetworkPrintProtocol>(protocolIndex);

	return true;
}

// 验证IP地址格式
bool NetworkPrinterConfigDialog::IsValidIpAddress(const std::string& ip) const
{
	struct in_addr addr;
	return inet_pton(AF_INET, ip.c_str(), &addr) == 1;
}

// 验证端口号
bool NetworkPrinterConfigDialog::IsValidPort(int port) const
{
	return port > 0 && port <= 65535;
}

// 测试连接
bool NetworkPrinterConfigDialog::TestConnection(const NetworkPrintConfig& config, std::string& errorMessage)
{
	// 创建socket
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		errorMessage = "无法创建socket";
		return false;
	}

	// 设置非阻塞模式
	u_long mode = 1;
	ioctlsocket(sock, FIONBIO, &mode);

	// 设置地址结构
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(config.port);

	// 转换IP地址
	if (inet_pton(AF_INET, config.hostname.c_str(), &serverAddr.sin_addr) <= 0) {
		closesocket(sock);
		errorMessage = "IP地址无效";
		return false;
	}

	// 尝试连接（500ms超时）
	fd_set writeSet;
	FD_ZERO(&writeSet);
	FD_SET(sock, &writeSet);

	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;  // 500ms

	int result = connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));

	if (result == SOCKET_ERROR) {
		int error = WSAGetLastError();

		if (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS) {
			// 等待连接完成或超时
			result = select(0, nullptr, &writeSet, nullptr, &timeout);
		}
	}

	bool connected = false;
	if (result > 0 && FD_ISSET(sock, &writeSet)) {
		// 检查连接是否真的成功了
		int error = 0;
		socklen_t len = sizeof(error);
		getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len);

		if (error == 0) {
			connected = true;
		} else {
			errorMessage = "连接被拒绝，错误代码: " + std::to_string(error);
		}
	} else {
		errorMessage = "连接超时";
	}

	closesocket(sock);
	return connected;
}
