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


#include "Base.h"

//Base definitions
//Read Texts input and Label types definitions
#define READ_TEXT_PARAMETER                   0
#define READ_TEXT_PARAMETER_MONITOR           1U
#define READ_TEXT_HOSTS                       2U
#define READ_TEXT_IPFILTER                    3U
#define LABEL_STOP                            1U
#define LABEL_IPFILTER                        2U
#define LABEL_IPFILTER_BLACKLIST              3U
#define LABEL_IPFILTER_LOCAL_ROUTING          4U
#define LABEL_HOSTS_TYPE_WHITE                5U
#define LABEL_HOSTS_TYPE_BANNED               6U
#define LABEL_HOSTS_TYPE_WHITE_EXTENDED       7U
#define LABEL_HOSTS_TYPE_BANNED_EXTENDED      8U
#define LABEL_HOSTS_TYPE_NORMAL               9U
#define LABEL_HOSTS_TYPE_CNAME                10U
#define LABEL_HOSTS_TYPE_LOCAL                11U
#define LABEL_HOSTS_TYPE_ADDRESS              12U
#define LABEL_HOSTS_TYPE_SOURCE               13U

//Length definitions
#define READ_DATA_MINSIZE                     4U
#define READ_TEXT_MINSIZE                     2U
#define READ_PARAMETER_MINSIZE                8U
#define READ_HOSTS_MINSIZE                    3U
#define READ_HOSTS_ADDRESS_MINSIZE            5U
#define READ_IPFILTER_MINSIZE                 5U
#define READ_IPFILTER_BLACKLIST_MINSIZE       3U
#define READ_IPFILTER_LOCAL_ROUTING_MINSIZE   4U

//Global variables
extern CONFIGURATION_TABLE Parameter, ParameterModificating;
extern GLOBAL_STATUS GlobalRunningStatus;
#if defined(ENABLE_LIBSODIUM)
	extern DNSCURVE_CONFIGURATION_TABLE DNSCurveParameter, DNSCurveParameterModificating;
#endif
extern std::vector<FILE_DATA> FileList_Config, FileList_IPFilter, FileList_Hosts;
extern std::vector<DIFFERNET_FILE_SET_IPFILTER> *IPFilterFileSetUsing, *IPFilterFileSetModificating;
extern std::vector<DIFFERNET_FILE_SET_HOSTS> *HostsFileSetUsing, *HostsFileSetModificating;
extern std::mutex IPFilterFileLock, HostsFileLock;

//Functions in Configuration.cpp
bool __fastcall ReadText(
	const FILE *FileHandle, 
	const size_t InputType, 
	const size_t FileIndex);
bool __fastcall ReadMultiLineComments(
	std::string &Data, 
	bool &IsLabelComments);
void __fastcall ClearModificatingListData(
	const size_t ClearType, 
	const size_t FileIndex);

//Functions in ReadParameter.cpp
bool __fastcall ParameterCheckAndSetting(
	const bool IsFirstRead, 
	const size_t FileIndex);
uint16_t __fastcall ServiceNameToBinary(
	const char *OriginalBuffer);
uint16_t __fastcall DNSTypeNameToBinary(
	const char *OriginalBuffer);
bool __fastcall ReadParameterData(
	std::string Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
	bool &IsLabelComments);
#if defined(PLATFORM_WIN)
bool __fastcall ReadPathAndFileName(
	std::string Data, 
	const size_t DataOffset, 
	const bool Path, 
	std::vector<std::wstring> *ListData, 
	const size_t FileIndex, 
	const size_t Line);
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACX))
bool ReadPathAndFileName(
	std::string Data, 
	const size_t DataOffset, 
	const bool Path, 
	std::vector<std::wstring> *ListData, 
	std::vector<std::string> *sListData, 
	const size_t FileIndex, 
	const size_t Line);
#endif
bool __fastcall ReadMultipleAddresses(
	std::string Data, 
	const size_t DataOffset, 
	const uint16_t Protocol, 
	const bool IsMultiAddresses, 
	sockaddr_storage &SockAddr, 
	std::vector<sockaddr_storage> *SockAddrList, 
	const size_t FileIndex, 
	const size_t Line);
bool __fastcall ReadSOCKSAddressAndDomain(
	std::string Data, 
	const size_t DataOffset, 
	CONFIGURATION_TABLE *ParameterPTR, 
	const size_t FileIndex, 
	const size_t Line);
#if defined(ENABLE_PCAP)
bool __fastcall ReadHopLimitData(
	std::string Data, 
	const size_t DataOffset, 
	const uint16_t Protocol, 
	uint8_t &HopLimit, 
	const size_t FileIndex, 
	const size_t Line);
#endif
#if defined(ENABLE_LIBSODIUM)
bool __fastcall ReadDNSCurveProviderName(
	std::string Data, 
	const size_t DataOffset, 
	char *ProviderNameData, 
	const size_t FileIndex, 
	const size_t Line);
bool __fastcall ReadDNSCurveKey(
	std::string Data, 
	const size_t DataOffset, 
	uint8_t *KeyData, 
	const size_t FileIndex, 
	const size_t Line);
#endif
bool __fastcall ReadDNSCurveMagicNumber(
	std::string Data, 
	const size_t DataOffset, 
	char *MagicNumber, 
	const size_t FileIndex, 
	const size_t Line);

//Functions in ReadIPFilter.cpp
bool __fastcall ReadIPFilterData(
	std::string Data, 
	const size_t FileIndex, 
	size_t &LabelType, 
	const size_t Line, 
	bool &IsLabelComments);
bool __fastcall ReadBlacklistData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);
bool __fastcall ReadLocalRoutingData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);
bool __fastcall ReadAddressPrefixBlock(
	std::string OriginalData, 
	const size_t DataOffset, 
	const uint16_t Protocol, 
	ADDRESS_PREFIX_BLOCK *AddressPrefix, 
	const size_t FileIndex, 
	const size_t Line);
bool __fastcall ReadMainIPFilterData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);

//Functions in ReadHosts.cpp
bool __fastcall ReadHostsData(
	std::string Data, 
	const size_t FileIndex, 
	size_t &LabelType, 
	const size_t Line, 
	bool &IsLabelComments);
bool __fastcall ReadOtherHostsData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line, 
	const size_t LabelType, 
	const size_t ItemType);
bool __fastcall ReadLocalHostsData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);
bool __fastcall ReadAddressHostsData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);
bool __fastcall ReadMainHostsData(
	std::string Data, 
	const size_t HostsType, 
	const size_t FileIndex, 
	const size_t Line);
