﻿// This code is part of Pcap_DNSProxy
// A local DNS server based on WinPcap and LibPcap
// Copyright (C) 2012-2016 Chengr28
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "Main.h"

//The Main function of program
#if defined(PLATFORM_WIN)
int wmain(
	int argc, 
	wchar_t* argv[])
{
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
int main(
	int argc, 
	char *argv[])
{
#endif

//Get commands.
	if (argc > 0)
	{
		if (!ReadCommand(argc, argv))
			return EXIT_SUCCESS;
	}
	else {
		return EXIT_FAILURE;
	}

//Read configuration file.
	if (!ReadParameter(true))
		return EXIT_FAILURE;

//DNSCurve initialization
#if defined(ENABLE_LIBSODIUM)
	if (Parameter.DNSCurve)
	{
		DNSCurveParameterModificating.SetToMonitorItem();

	//Encryption mode initialization
		if (DNSCurveParameter.IsEncryption)
			DNSCurveInit();
	}
#endif

//Mark Local DNS address to PTR Records, read Parameter(Monitor mode), IPFilter and Hosts.
	ParameterModificating.SetToMonitorItem();
	std::thread NetworkInformationMonitorThread(std::bind(NetworkInformationMonitor));
	NetworkInformationMonitorThread.detach();
	std::thread ReadParameterThread(std::bind(ReadParameter, false));
	ReadParameterThread.detach();
	std::thread ReadHostsThread(std::bind(ReadHosts));
	ReadHostsThread.detach();
	if (Parameter.OperationMode == LISTEN_MODE_CUSTOM || Parameter.DataCheck_Blacklist || Parameter.LocalRouting)
	{
		std::thread ReadIPFilterThread(std::bind(ReadIPFilter));
		ReadIPFilterThread.detach();
	}

#if defined(PLATFORM_WIN)
//Service initialization and start service.
	SERVICE_TABLE_ENTRYW ServiceTable[]{{SYSTEM_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain}, {nullptr, nullptr}};
	if (!StartServiceCtrlDispatcherW(ServiceTable))
	{
		GlobalRunningStatus.Console = true;
		auto ErrorCode = GetLastError();
		
	//Print to screen.
		std::unique_lock<std::mutex> ScreenMutex(ScreenLock);
		fwprintf_s(stderr, L"System Error: Service start error, error code is %lu.\n", ErrorCode);
		fwprintf_s(stderr, L"System Error: Program will continue to run in console mode.\n");
		fwprintf_s(stderr, L"Please ignore these error messages if you want to run in console mode.\n\n");
		ScreenMutex.unlock();

	//Handle the system signal and start all monitors.
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
		MonitorInit();
	}
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
	MonitorInit();
#endif

	return EXIT_SUCCESS;
}

//Read commands from main program
#if defined(PLATFORM_WIN)
bool __fastcall ReadCommand(
	int argc, 
	wchar_t *argv[])
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
bool ReadCommand(
	int argc, 
	char *argv[])
#endif
{
//Path initialization
#if defined(PLATFORM_WIN)
	if (!FileNameInit(argv[0]))
		return false;
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
	char FileName[PATH_MAX + 1U];
	memset(FileName, 0, PATH_MAX + 1U);
	if (getcwd(FileName, PATH_MAX) == nullptr)
	{
		std::lock_guard<std::mutex> ScreenMutex(ScreenLock);
		fwprintf(stderr, L"Path initialization error.\n");

		return false;
	}
	if (!FileNameInit(FileName))
		return false;
#endif

//Screen output buffer setting
	if (setvbuf(stderr, NULL, _IONBF, 0) != 0)
	{
		auto ErrorCode = errno;
		std::unique_lock<std::mutex> ScreenMutex(ScreenLock);
		fwprintf_s(stderr, L"Screen output buffer setting error, error code is %d.\n", ErrorCode);
		ScreenMutex.unlock();
		PrintError(LOG_LEVEL_2, LOG_ERROR_NETWORK, L"Screen output buffer setting error", ErrorCode, nullptr, 0);

		return false;
	}

//Winsock initialization
#if defined(PLATFORM_WIN)
	WSAData WSAInitialization;
	memset(&WSAInitialization, 0, sizeof(WSAData));
	if (WSAStartup(MAKEWORD(WINSOCK_VERSION_HIGH, WINSOCK_VERSION_LOW), &WSAInitialization) != 0 || 
		LOBYTE(WSAInitialization.wVersion) != WINSOCK_VERSION_LOW || HIBYTE(WSAInitialization.wVersion) != WINSOCK_VERSION_HIGH)
	{
		auto ErrorCode = WSAGetLastError();
		std::unique_lock<std::mutex> ScreenMutex(ScreenLock);
		fwprintf_s(stderr, L"Winsock initialization error, error code is %d.\n", ErrorCode);
		ScreenMutex.unlock();
		PrintError(LOG_LEVEL_1, LOG_ERROR_NETWORK, L"Winsock initialization error", ErrorCode, nullptr, 0);

		return false;
	}
	else {
		GlobalRunningStatus.Initialization_WinSock = true;
	}

//Read commands.
	std::wstring Commands;
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
	std::string Commands;
#endif
	for (size_t Index = 1U;(SSIZE_T)Index < argc;++Index)
	{
		Commands = argv[Index];

	//Flush DNS Cache from user.
		if (Commands == COMMAND_FLUSH_DNS)
		{
		#if defined(PLATFORM_WIN)
			FlushDNSMailSlotSender();
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
			FlushDNSFIFOSender();
		#endif

			return false;
		}
	//Windows Firewall Test in first start.
	#if defined(PLATFORM_WIN)
		else if (Commands == COMMAND_FIREWALL_TEST)
		{
			if (!FirewallTest(AF_INET6) && !FirewallTest(AF_INET))
			{
				auto ErrorCode = WSAGetLastError();
				std::unique_lock<std::mutex> ScreenMutex(ScreenLock);
				fwprintf_s(stderr, L"Windows Firewall Test error, error code is %d.\n", ErrorCode);
				ScreenMutex.unlock();
				PrintError(LOG_LEVEL_2, LOG_ERROR_NETWORK, L"Windows Firewall Test error", ErrorCode, nullptr, 0);
			}

			return false;
		}
	#endif
	//Set system daemon.
	#if defined(PLATFORM_LINUX)
		else if (Commands == COMMAND_DISABLE_DAEMON)
		{
			GlobalRunningStatus.Daemon = false;
		}
	#endif
	//Print current version.
		else if (Commands == COMMAND_LONG_PRINT_VERSION || Commands == COMMAND_SHORT_PRINT_VERSION)
		{
			std::lock_guard<std::mutex> ScreenMutex(ScreenLock);
			fwprintf_s(stderr, L"Pcap_DNSProxy ");
			fwprintf_s(stderr, FULL_VERSION);
			fwprintf_s(stderr, L"\n");

			return false;
		}
	//Print library version.
		else if (Commands == COMMAND_LIB_VERSION)
		{
			std::lock_guard<std::mutex> ScreenMutex(ScreenLock);

		#if (defined(ENABLE_LIBSODIUM) || defined(ENABLE_PCAP))
			std::wstring LibVersion;

			//LibSodium version
			#if defined(ENABLE_LIBSODIUM)
				if (MBSToWCSString(SODIUM_VERSION_STRING, strlen(SODIUM_VERSION_STRING), LibVersion))
					fwprintf_s(stderr, L"LibSodium version %ls\n", LibVersion.c_str());
			#endif

			//WinPcap or LibPcap version
			#if defined(ENABLE_PCAP)
				if (MBSToWCSString(pcap_lib_version(), strlen(pcap_lib_version()), LibVersion))
					fwprintf_s(stderr, L"%ls\n", LibVersion.c_str());
			#endif
		#else
			fwprintf(stderr, L"No any available libraries.\n");
		#endif

			return false;
		}
	//Print help messages.
		else if (Commands == COMMAND_LONG_HELP || Commands == COMMAND_SHORT_HELP)
		{
			std::lock_guard<std::mutex> ScreenMutex(ScreenLock);

			fwprintf_s(stderr, L"Pcap_DNSProxy ");
			fwprintf_s(stderr, FULL_VERSION);
		#if defined(PLATFORM_WIN)
			fwprintf_s(stderr, L"(Windows)\n");
		#elif defined(PLATFORM_OPENWRT)
			fwprintf(stderr, L"(OpenWrt)\n");
		#elif defined(PLATFORM_LINUX)
			fwprintf(stderr, L"(Linux)\n");
		#elif defined(PLATFORM_MACX)
			fwprintf(stderr, L"(Mac)\n");
		#endif
			fwprintf_s(stderr, COPYRIGHT_MESSAGE);
			fwprintf_s(stderr, L"\nUsage: Please visit ReadMe... files in Documents folder.\n");
			fwprintf_s(stderr, L"   -v/--version:          Print current version on screen.\n");
			fwprintf_s(stderr, L"   --lib-version:         Print current version of libraries on screen.\n");
			fwprintf_s(stderr, L"   -h/--help:             Print help messages on screen.\n");
			fwprintf_s(stderr, L"   --flush-dns:           Flush all DNS cache in program and system immediately.\n");
		#if defined(PLATFORM_WIN)
			fwprintf_s(stderr, L"   --first-setup:         Test local firewall.\n");
		#endif
			fwprintf_s(stderr, L"   -c/--config-file Path: Set path of configuration file.\n");
		#if defined(PLATFORM_LINUX)
			fwprintf(stderr, L"   --disable-daemon:      Disable daemon mode.\n");
		#endif

			return false;
		}
	//Set working directory from commands.
		else if (Commands == COMMAND_LONG_SET_PATH || Commands == COMMAND_SHORT_SET_PATH)
		{
		//Commands check
			if ((SSIZE_T)Index + 1 >= argc)
			{
				std::unique_lock<std::mutex> ScreenMutex(ScreenLock);
				fwprintf(stderr, L"Commands error.\n");
				ScreenMutex.unlock();
				PrintError(LOG_LEVEL_1, LOG_ERROR_SYSTEM, L"Commands error", 0, nullptr, 0);

				return false;
			}
			else {
				++Index;
				Commands = argv[Index];

			//Path check.
				if (Commands.length() > MAX_PATH)
				{
					std::unique_lock<std::mutex> ScreenMutex(ScreenLock);
					fwprintf_s(stderr, L"Commands error.\n");
					ScreenMutex.unlock();
					PrintError(LOG_LEVEL_1, LOG_ERROR_SYSTEM, L"Commands error", 0, nullptr, 0);

					return false;
				}
				else {
					if (!FileNameInit(Commands.c_str()))
						return false;
				}
			}
		}
	}

//Set system daemon.
#if defined(PLATFORM_LINUX)
	if (GlobalRunningStatus.Daemon && daemon(0, 0) == RETURN_ERROR)
	{
		PrintError(LOG_LEVEL_2, LOG_ERROR_SYSTEM, L"Set system daemon error", 0, nullptr, 0);
		return false;
	}
#endif

	return true;
}

//Get path of program from the main function parameter and Winsock initialization
#if defined(PLATFORM_WIN)
bool __fastcall FileNameInit(
	const wchar_t *OriginalPath)
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
bool FileNameInit(
	const char *OriginalPath)
#endif
{
//Path process
#if defined(PLATFORM_WIN)
	GlobalRunningStatus.Path_Global->clear();
	GlobalRunningStatus.Path_Global->push_back(OriginalPath);
	GlobalRunningStatus.Path_Global->front().erase(GlobalRunningStatus.Path_Global->front().rfind(L"\\") + 1U);
	for (size_t Index = 0;Index < GlobalRunningStatus.Path_Global->front().length();++Index)
	{
		if ((GlobalRunningStatus.Path_Global->front()).at(Index) == L'\\')
		{
			GlobalRunningStatus.Path_Global->front().insert(Index, L"\\");
			++Index;
		}
	}
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
	GlobalRunningStatus.sPath_Global->clear();
	GlobalRunningStatus.sPath_Global->push_back(OriginalPath);
	GlobalRunningStatus.sPath_Global->front().append("/");
	std::wstring StringTemp;
	if (!MBSToWCSString(OriginalPath, PATH_MAX + 1U, StringTemp))
		return false;
	StringTemp.append(L"/");
	GlobalRunningStatus.Path_Global->clear();
	GlobalRunningStatus.Path_Global->push_back(StringTemp);
	StringTemp.clear();
#endif

//Get path of error/running status log file and mark start time.
	GlobalRunningStatus.Path_ErrorLog->clear();
	*GlobalRunningStatus.Path_ErrorLog = GlobalRunningStatus.Path_Global->front();
	GlobalRunningStatus.Path_ErrorLog->append(L"Error.log");
#if (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
	GlobalRunningStatus.sPath_ErrorLog->clear();
	*GlobalRunningStatus.sPath_ErrorLog = GlobalRunningStatus.sPath_Global->front();
	GlobalRunningStatus.sPath_ErrorLog->append("Error.log");
#endif
	Parameter.PrintLogLevel = DEFAULT_LOG_LEVEL;
	GlobalRunningStatus.StartupTime = time(nullptr);

	return true;
}

#if defined(PLATFORM_WIN)
//Windows Firewall Test
bool __fastcall FirewallTest(
	const uint16_t Protocol)
{
//Ramdom number distribution initialization
	std::uniform_int_distribution<uint16_t> RamdomDistribution(DYNAMIC_MIN_PORT, UINT16_MAX - 1U);
	sockaddr_storage SockAddr;
	memset(&SockAddr, 0, sizeof(sockaddr_storage));
	SYSTEM_SOCKET FirewallSocket = 0;

//IPv6
	if (Protocol == AF_INET6)
	{
		((PSOCKADDR_IN6)&SockAddr)->sin6_addr = in6addr_any;
		((PSOCKADDR_IN6)&SockAddr)->sin6_port = htons(RamdomDistribution(*GlobalRunningStatus.RamdomEngine));
		SockAddr.ss_family = AF_INET6;
		FirewallSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	//Bind local socket.
		if (!SocketSetting(FirewallSocket, SOCKET_SETTING_INVALID_CHECK, true, nullptr))
		{
			return false;
		}
		else if (bind(FirewallSocket, (PSOCKADDR)&SockAddr, sizeof(sockaddr_in6)) == SOCKET_ERROR)
		{
			((PSOCKADDR_IN6)&SockAddr)->sin6_port = htons(RamdomDistribution(*GlobalRunningStatus.RamdomEngine));
			size_t Index = 0;
			while (bind(FirewallSocket, (PSOCKADDR)&SockAddr, sizeof(sockaddr_in6)) == SOCKET_ERROR)
			{
				if (Index < LOOP_MAX_TIMES && WSAGetLastError() == WSAEADDRINUSE)
				{
					((PSOCKADDR_IN6)&SockAddr)->sin6_port = htons(RamdomDistribution(*GlobalRunningStatus.RamdomEngine));
					++Index;
				}
				else {
					shutdown(FirewallSocket, SD_BOTH);
					closesocket(FirewallSocket);

					return false;
				}
			}
		}
	}
//IPv4
	else {
		((PSOCKADDR_IN)&SockAddr)->sin_addr.s_addr = INADDR_ANY;
		((PSOCKADDR_IN)&SockAddr)->sin_port = htons(RamdomDistribution(*GlobalRunningStatus.RamdomEngine));
		SockAddr.ss_family = AF_INET;
		FirewallSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//Bind local socket.
		if (!SocketSetting(FirewallSocket, SOCKET_SETTING_INVALID_CHECK, true, nullptr))
		{
			return false;
		}
		else if (bind(FirewallSocket, (PSOCKADDR)&SockAddr, sizeof(sockaddr_in)) == SOCKET_ERROR)
		{
			((PSOCKADDR_IN)&SockAddr)->sin_port = htons(RamdomDistribution(*GlobalRunningStatus.RamdomEngine));
			size_t Index = 0;
			while (bind(FirewallSocket, (PSOCKADDR)&SockAddr, sizeof(sockaddr_in)) == SOCKET_ERROR)
			{
				if (Index < LOOP_MAX_TIMES && WSAGetLastError() == WSAEADDRINUSE)
				{
					((PSOCKADDR_IN)&SockAddr)->sin_port = htons(RamdomDistribution(*GlobalRunningStatus.RamdomEngine));
					++Index;
				}
				else {
					shutdown(FirewallSocket, SD_BOTH);
					closesocket(FirewallSocket);

					return false;
				}
			}
		}
	}

//Close socket.
	shutdown(FirewallSocket, SD_BOTH);
	closesocket(FirewallSocket);
	return true;
}
#endif
