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


#include "Protocol.h"

//Convert address strings to binary.
bool __fastcall AddressStringToBinary(
	const char *AddrString, 
	const uint16_t Protocol, 
	void *OriginalAddr, 
	SSIZE_T *ErrorCode)
{
	std::string sAddrString(AddrString);
	if (Protocol == AF_INET6)
		memset(OriginalAddr, 0, sizeof(in6_addr));
	else //IPv4
		memset(OriginalAddr, 0, sizeof(in_addr));
	if (ErrorCode != nullptr)
		*ErrorCode = 0;

//inet_ntop function and inet_pton function was only support in Windows Vista and newer system. [Roy Tam]
#if defined(PLATFORM_WIN_XP)
	sockaddr_storage SockAddr;
	memset(&SockAddr, 0, sizeof(sockaddr_storage));
	int SockLength = 0;
#else
	SSIZE_T Result = 0;
#endif

	if (Protocol == AF_INET6) //IPv6
	{
	//Check IPv6 addresses.
		if (sAddrString.find(ASCII_COLON) == std::string::npos || 
			sAddrString.find(ASCII_PERIOD) != std::string::npos || sAddrString.find("::") != sAddrString.rfind("::"))
				return false;
		for (auto StringIter:sAddrString)
		{
			if (StringIter < ASCII_ZERO || 
				(StringIter > ASCII_COLON && StringIter < ASCII_UPPERCASE_A) || 
				(StringIter > ASCII_UPPERCASE_F && StringIter < ASCII_LOWERCASE_A) || 
				StringIter > ASCII_LOWERCASE_F)
					return false;
		}

	//Check abbreviation format.
		if (sAddrString.find(ASCII_COLON) == std::string::npos)
		{
			sAddrString.clear();
			sAddrString.append("::");
			sAddrString.append(AddrString);
		}
		else if (sAddrString.find(ASCII_COLON) == sAddrString.rfind(ASCII_COLON))
		{
			sAddrString.replace(sAddrString.find(ASCII_COLON), 1U, ("::"));
		}

	//Convert to binary.
	#if defined(PLATFORM_WIN_XP)
/* Old version(2016-05-29)
		if (GlobalRunningStatus.FunctionPTR_InetPton != nullptr)
		{
			Result = (*GlobalRunningStatus.FunctionPTR_InetPton)(AF_INET6, sAddrString.c_str(), OriginalAddr);
			if (Result == SOCKET_ERROR || Result == 0)
			{
				if (Result != 0 && ErrorCode != nullptr)
					*ErrorCode = WSAGetLastError();

				return false;
			}
		}
		else {
*/
		SockLength = sizeof(sockaddr_in6);
		if (WSAStringToAddressA((char *)sAddrString.c_str(), AF_INET6, nullptr, (PSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		{
			if (ErrorCode != nullptr)
				*ErrorCode = WSAGetLastError();

			return false;
		}

		memcpy_s(OriginalAddr, sizeof(in6_addr), &((PSOCKADDR_IN6)&SockAddr)->sin6_addr, sizeof(in6_addr));
	#else
		Result = inet_pton(AF_INET6, sAddrString.c_str(), OriginalAddr);
		if (Result == SOCKET_ERROR || Result == 0)
		{
			if (Result != 0 && ErrorCode != nullptr)
				*ErrorCode = WSAGetLastError();

			return false;
		}
	#endif
	}
	else { //IPv4
	//Check IPv4 addresses.
		if (sAddrString.find(ASCII_PERIOD) == std::string::npos || sAddrString.find(ASCII_COLON) != std::string::npos)
			return false;
		size_t CommaNum = 0;
		for (auto StringIter:sAddrString)
		{
			if ((StringIter != ASCII_PERIOD && StringIter < ASCII_ZERO) || StringIter > ASCII_NINE)
				return false;
			else if (StringIter == ASCII_PERIOD)
				++CommaNum;
		}

	//Delete zeros before whole data.
		while (sAddrString.length() > 1U && sAddrString[0] == ASCII_ZERO && sAddrString[1U] != ASCII_PERIOD)
			sAddrString.erase(0, 1U);

	//Check abbreviation format.
		switch (CommaNum)
		{
			case 0:
			{
				sAddrString.clear();
				sAddrString.append("0.0.0.");
				sAddrString.append(AddrString);
			}break;
			case 1U:
			{
				sAddrString.replace(sAddrString.find(ASCII_PERIOD), 1U, (".0.0."));
			}break;
			case 2U:
			{
				sAddrString.replace(sAddrString.find(ASCII_PERIOD), 1U, (".0."));
			}break;
		}

	//Delete zeros before data.
		while (sAddrString.find(".00") != std::string::npos)
			sAddrString.replace(sAddrString.find(".00"), 3U, ("."));
		while (sAddrString.find(".0") != std::string::npos)
			sAddrString.replace(sAddrString.find(".0"), 2U, ("."));
		while (sAddrString.find("..") != std::string::npos)
			sAddrString.replace(sAddrString.find(".."), 2U, (".0."));
		if (sAddrString.at(sAddrString.length() - 1U) == ASCII_PERIOD)
			sAddrString.append("0");

	//Convert to binary.
	#if defined(PLATFORM_WIN_XP)
/* Old version(2016-05-29)
		if (GlobalRunningStatus.FunctionPTR_InetPton != nullptr)
		{
			Result = (*GlobalRunningStatus.FunctionPTR_InetPton)(AF_INET, sAddrString.c_str(), OriginalAddr);
			if (Result == SOCKET_ERROR || Result == 0)
			{
				if (Result != 0 && ErrorCode != nullptr)
					*ErrorCode = WSAGetLastError();

				return false;
			}
		}
		else {
*/
		SockLength = sizeof(sockaddr_in);
		if (WSAStringToAddressA((char *)sAddrString.c_str(), AF_INET, nullptr, (PSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		{
			if (ErrorCode != nullptr)
				*ErrorCode = WSAGetLastError();

			return false;
		}

		memcpy_s(OriginalAddr, sizeof(in_addr), &((PSOCKADDR_IN)&SockAddr)->sin_addr, sizeof(in_addr));
	#else
		Result = inet_pton(AF_INET, sAddrString.c_str(), OriginalAddr);
		if (Result == SOCKET_ERROR || Result == 0)
		{
			if (Result != 0 && ErrorCode != nullptr)
				*ErrorCode = WSAGetLastError();

			return false;
		}
	#endif
	}

	return true;
}

//Compare two addresses
size_t __fastcall AddressesComparing(
	const void *OriginalAddrBegin, 
	const void *OriginalAddrEnd, 
	const uint16_t Protocol)
{
	if (Protocol == AF_INET6) //IPv6
	{
		for (size_t Index = 0;Index < sizeof(in6_addr) / sizeof(uint16_t);++Index)
		{
			if (ntohs(((in6_addr *)OriginalAddrBegin)->s6_words[Index]) > ntohs(((in6_addr *)OriginalAddrEnd)->s6_words[Index]))
			{
				return ADDRESS_COMPARE_GREATER;
			}
			else if (((in6_addr *)OriginalAddrBegin)->s6_words[Index] == ((in6_addr *)OriginalAddrEnd)->s6_words[Index])
			{
				if (Index == sizeof(in6_addr) / sizeof(uint16_t) - 1U)
					return ADDRESS_COMPARE_EQUAL;
				else 
					continue;
			}
			else {
				return ADDRESS_COMPARE_LESS;
			}
		}
	}
	else { //IPv4
		if (((in_addr *)OriginalAddrBegin)->s_net > ((in_addr *)OriginalAddrEnd)->s_net)
		{
			return ADDRESS_COMPARE_GREATER;
		}
		else if (((in_addr *)OriginalAddrBegin)->s_net == ((in_addr *)OriginalAddrEnd)->s_net)
		{
			if (((in_addr *)OriginalAddrBegin)->s_host > ((in_addr *)OriginalAddrEnd)->s_host)
			{
				return ADDRESS_COMPARE_GREATER;
			}
			else if (((in_addr *)OriginalAddrBegin)->s_host == ((in_addr *)OriginalAddrEnd)->s_host)
			{
				if (((in_addr *)OriginalAddrBegin)->s_lh > ((in_addr *)OriginalAddrEnd)->s_lh)
				{
					return ADDRESS_COMPARE_GREATER;
				}
				else if (((in_addr *)OriginalAddrBegin)->s_lh == ((in_addr *)OriginalAddrEnd)->s_lh)
				{
					if (((in_addr *)OriginalAddrBegin)->s_impno > ((in_addr *)OriginalAddrEnd)->s_impno)
						return ADDRESS_COMPARE_GREATER;
					else if (((in_addr *)OriginalAddrBegin)->s_impno == ((in_addr *)OriginalAddrEnd)->s_impno)
						return ADDRESS_COMPARE_EQUAL;
					else 
						return ADDRESS_COMPARE_LESS;
				}
				else {
					return ADDRESS_COMPARE_LESS;
				}
			}
			else {
				return ADDRESS_COMPARE_LESS;
			}
		}
		else {
			return ADDRESS_COMPARE_LESS;
		}
	}

	return EXIT_SUCCESS;
}

//Check IPv4/IPv6 special addresses
bool __fastcall CheckSpecialAddress(
	void *Addr, 
	const uint16_t Protocol, 
	const bool IsPrivateUse, 
	const char *Domain)
{
	if (Protocol == AF_INET6) //IPv6
	{
		if (
		//DNS Poisoning addresses from CERNET2, see https://code.google.com/p/goagent/issues/detail?id=17571.
			(((in6_addr *)Addr)->s6_words[0] == 0 && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[2U] == 0 && ((in6_addr *)Addr)->s6_words[3U] == 0 && ((in6_addr *)Addr)->s6_bytes[8U] == 0x90 && ((in6_addr *)Addr)->s6_words[6U] == 0 && ((in6_addr *)Addr)->s6_words[7U] == 0) || //::90xx:xxxx:0:0
			(((in6_addr *)Addr)->s6_words[0] == htons(0x0010) && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[2U] == 0 && ((in6_addr *)Addr)->s6_words[3U] == 0 && ((in6_addr *)Addr)->s6_words[4U] == 0 && ((in6_addr *)Addr)->s6_words[5U] == 0 && ((in6_addr *)Addr)->s6_words[6U] == 0 && ((in6_addr *)Addr)->s6_words[7U] == htons(0x2222)) || //10::2222
			(((in6_addr *)Addr)->s6_words[0] == htons(0x0021) && ((in6_addr *)Addr)->s6_words[1U] == htons(0x0002) && ((in6_addr *)Addr)->s6_words[2U] == 0 && ((in6_addr *)Addr)->s6_words[3U] == 0 && ((in6_addr *)Addr)->s6_words[4U] == 0 && ((in6_addr *)Addr)->s6_words[5U] == 0 && ((in6_addr *)Addr)->s6_words[6U] == 0 && ((in6_addr *)Addr)->s6_words[7U] == htons(0x0002)) || //21:2::2
			(((in6_addr *)Addr)->s6_words[0] == htons(0x0101) && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[2U] == 0 && ((in6_addr *)Addr)->s6_words[3U] == 0 && ((in6_addr *)Addr)->s6_words[4U] == 0 && ((in6_addr *)Addr)->s6_words[5U] == 0 && ((in6_addr *)Addr)->s6_words[6U] == 0 && ((in6_addr *)Addr)->s6_words[7U] == htons(0x1234)) || //101::1234
			(((in6_addr *)Addr)->s6_words[0] == htons(0x2001) && 
			((IsPrivateUse && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[2U] == 0 && ((in6_addr *)Addr)->s6_words[3U] == 0 && ((in6_addr *)Addr)->s6_words[4U] == 0 && ((in6_addr *)Addr)->s6_words[5U] == 0 && ((in6_addr *)Addr)->s6_words[6U] == 0 && ((in6_addr *)Addr)->s6_words[7U] == htons(0x0212)) || //2001::212
			(((in6_addr *)Addr)->s6_words[1U] == htons(0x0DA8) && ((in6_addr *)Addr)->s6_words[2U] == htons(0x0112) && ((in6_addr *)Addr)->s6_words[3U] == 0 && ((in6_addr *)Addr)->s6_words[4U] == 0 && ((in6_addr *)Addr)->s6_words[5U] == 0 && ((in6_addr *)Addr)->s6_words[6U] == 0 && ((in6_addr *)Addr)->s6_words[7U] == htons(0x21AE)))) || //2001:DA8:112::21AE
			(((in6_addr *)Addr)->s6_words[0] == htons(0x2003) && ((in6_addr *)Addr)->s6_words[1U] == htons(0x00FF) && ((in6_addr *)Addr)->s6_words[2U] == htons(0x0001) && ((in6_addr *)Addr)->s6_words[3U] == htons(0x0002) && ((in6_addr *)Addr)->s6_words[4U] == htons(0x0003) && ((in6_addr *)Addr)->s6_words[5U] == htons(0x0004) && ((in6_addr *)Addr)->s6_words[6U] == htons(0x5FFF)) || //2003:FF:1:2:3:4:5FFF:xxxx
			(((in6_addr *)Addr)->s6_words[0] == htons(0x2123) && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[2U] == 0 && ((in6_addr *)Addr)->s6_words[3U] == 0 && ((in6_addr *)Addr)->s6_words[4U] == 0 && ((in6_addr *)Addr)->s6_words[5U] == 0 && ((in6_addr *)Addr)->s6_words[6U] == 0 && ((in6_addr *)Addr)->s6_words[7U] == htons(0x3E12)) || //2123::3E12
		//Special-use or reserved addresses, see https://en.wikipedia.org/wiki/IPv6_address#Presentation and https://en.wikipedia.org/wiki/Reserved_IP_addresses#Reserved_IPv6_addresses.
			(((in6_addr *)Addr)->s6_words[0] == 0 && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[2U] == 0 && ((in6_addr *)Addr)->s6_words[3U] == 0 && ((in6_addr *)Addr)->s6_words[4U] == 0 && 
			(((in6_addr *)Addr)->s6_words[5U] == 0 || //IPv4-Compatible Contrast addresses(::/96, Section 2.5.5.1 in RFC 4291)
//			((in6_addr *)Addr)->s6_words[5U] == 0 && ((in6_addr *)Addr)->s6_words[6U] == 0 && ((in6_addr *)Addr)->s6_words[7U] == 0 || //Unspecified addresses(::, Section 2.5.2 in RFC 4291)
//			((in6_addr *)Addr)->s6_words[5U] == 0 && ((in6_addr *)Addr)->s6_words[6U] == 0 && ((in6_addr *)Addr)->s6_words[7U] == htons(0x0001) || //Loopback addresses(::1, Section 2.5.3 in RFC 4291)
			((in6_addr *)Addr)->s6_words[5U] == htons(0xFFFF))) || //IPv4-mapped addresses(::FFFF:0:0/96, Section 2.5.5 in RFC 4291)
			(IsPrivateUse && ((in6_addr *)Addr)->s6_words[0] == htons(0x0064) && ((in6_addr *)Addr)->s6_words[1U] == htons(0xFF9B) && ((in6_addr *)Addr)->s6_words[2U] == 0 && ((in6_addr *)Addr)->s6_words[3U] == 0 && ((in6_addr *)Addr)->s6_words[4U] == 0 && ((in6_addr *)Addr)->s6_words[5U] == 0) || //Well Known Prefix addresses(64:FF9B::/96, Section 2.1 in RFC 4773)
			(((in6_addr *)Addr)->s6_words[0] == htons(0x0100) && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[1U] == 0 && ((in6_addr *)Addr)->s6_words[1U] == 0) || //Discard Prefix addresses(100::/64, Section 4 RFC 6666)
			(((in6_addr *)Addr)->s6_words[0] == htons(0x2001) && 
			(((in6_addr *)Addr)->s6_words[1U] == 0 || //Teredo relay/tunnel addresses(2001::/32, RFC 4380)
			(((in6_addr *)Addr)->s6_bytes[2U] == 0 && ((in6_addr *)Addr)->s6_bytes[3U] <= 0x07) || //Sub-TLA IDs assigned to IANA addresses(2001:0000::/29, Section 2 in RFC 4773)
			(((in6_addr *)Addr)->s6_bytes[2U] == 0 && ((in6_addr *)Addr)->s6_bytes[3U] == 0 && ((in6_addr *)Addr)->s6_bytes[4U] >= 0x10 && ((in6_addr *)Addr)->s6_bytes[4U] <= 0x1F) || //Overlay Routable Cryptographic Hash IDentifiers/ORCHID addresses(2001:10::/28 in RFC 4843)
			(((in6_addr *)Addr)->s6_bytes[2U] == 0x01 && ((in6_addr *)Addr)->s6_bytes[3U] >= 0xF8) || //Sub-TLA IDs assigned to IANA addresses(2001:01F8::/29, Section 2 in RFC 4773)
			((in6_addr *)Addr)->s6_words[1U] == htons(0x0DB8))) || //Contrast Address prefix reserved for documentation addresses(2001:DB8::/32, RFC 3849)
			(IsPrivateUse && ((in6_addr *)Addr)->s6_words[0] == htons(0x2002)) || //6to4 relay/tunnel addresses(2002::/16, Section 2 in RFC 3056)
			(((in6_addr *)Addr)->s6_words[0] == htons(0x3FFE) && ((in6_addr *)Addr)->s6_words[1U] == 0) || //6bone addresses(3FFE::/16, RFC 3701)
			((in6_addr *)Addr)->s6_bytes[0] == 0x5F || //6bone(5F00::/8, RFC 3701)
			(IsPrivateUse && ((in6_addr *)Addr)->s6_bytes[0] >= 0xFC && ((in6_addr *)Addr)->s6_bytes[0] <= 0xFD) || //Unique Local Unicast addresses/ULA(FC00::/7, Section 2.5.7 in RFC 4193)
			(((in6_addr *)Addr)->s6_bytes[0] == 0xFE && IsPrivateUse && 
			((((in6_addr *)Addr)->s6_bytes[1U] >= 0x80 && ((in6_addr *)Addr)->s6_bytes[1U] <= 0xBF) || //Link-Local Unicast Contrast addresses/LUC(FE80::/10, Section 2.5.6 in RFC 4291)
//			((in6_addr *)Addr)->s6_bytes[1U] <= 0xBF && ((in6_addr *)Addr)->s6_words[4U] == 0 && ((in6_addr *)Addr)->s6_words[5U] == htons(0x5EFE)) || //ISATAP Interface Identifiers addresses(Prefix:0:5EFE:0:0:0:0/64, which also in Link-Local Unicast Contrast addresses/LUC, Section 6.1 in RFC 5214)
			((in6_addr *)Addr)->s6_bytes[1U] >= 0xC0)) || //Site-Local scoped addresses(FEC0::/10, RFC 3879)
			(IsPrivateUse && ((in6_addr *)Addr)->s6_bytes[0] == 0xFF)) //Multicast addresses(FF00::/8, Section 2.7 in RFC 4291)
				return true;

	//Result Blacklist check
		if (Domain != nullptr)
		{
		//Domain Case Conversion
			std::string InnerDomain(Domain);
			CaseConvert(false, InnerDomain);

		//Main check
			std::lock_guard<std::mutex> IPFilterFileMutex(IPFilterFileLock);
			for (auto IPFilterFileSetIter:*IPFilterFileSetUsing)
			{
				for (auto ResultBlacklistTableIter:IPFilterFileSetIter.ResultBlacklist)
				{
					if (ResultBlacklistTableIter.Addresses.front().Begin.ss_family == AF_INET6 && 
						(ResultBlacklistTableIter.PatternString.empty() || std::regex_match(InnerDomain, ResultBlacklistTableIter.Pattern)))
					{
						for (auto AddressRangeTableIter:ResultBlacklistTableIter.Addresses)
						{
							if ((AddressRangeTableIter.End.ss_family == AF_INET6 && 
								AddressesComparing(Addr, &((PSOCKADDR_IN6)&AddressRangeTableIter.Begin)->sin6_addr, AF_INET6) >= ADDRESS_COMPARE_EQUAL && 
								AddressesComparing(Addr, &((PSOCKADDR_IN6)&AddressRangeTableIter.End)->sin6_addr, AF_INET6) <= ADDRESS_COMPARE_EQUAL) || 
								memcmp(Addr, &((PSOCKADDR_IN6)&AddressRangeTableIter.Begin)->sin6_addr, sizeof(in6_addr)) == 0)
									return true;
						}
					}
				}
			}
		}

	//Address Hosts check
		std::lock_guard<std::mutex> HostsFileMutex(HostsFileLock);
		for (auto HostsFileSetIter:*HostsFileSetUsing)
		{
			for (auto AddressHostsTableIter:HostsFileSetIter.AddressHostsList)
			{
				if (AddressHostsTableIter.Address_Target.front().ss_family == AF_INET6)
				{
					for (auto AddressRangeTableIter:AddressHostsTableIter.Address_Source)
					{
						if ((AddressRangeTableIter.Begin.ss_family == AF_INET6 && AddressRangeTableIter.End.ss_family == AF_INET6 && 
							AddressesComparing(Addr, &((PSOCKADDR_IN6)&AddressRangeTableIter.Begin)->sin6_addr, AF_INET6) >= ADDRESS_COMPARE_EQUAL && 
							AddressesComparing(Addr, &((PSOCKADDR_IN6)&AddressRangeTableIter.End)->sin6_addr, AF_INET6) <= ADDRESS_COMPARE_EQUAL) || 
							memcmp(Addr, &((PSOCKADDR_IN6)&AddressRangeTableIter.Begin)->sin6_addr, sizeof(in6_addr)) == 0)
						{
							if (AddressHostsTableIter.Address_Target.size() > 1U)
							{
							//Get a ramdom one.
								std::uniform_int_distribution<size_t> RamdomDistribution(0, AddressHostsTableIter.Address_Target.size() - 1U);
								*(in6_addr *)Addr = ((PSOCKADDR_IN6)&AddressHostsTableIter.Address_Target.at(RamdomDistribution(*GlobalRunningStatus.RamdomEngine)))->sin6_addr;
							}
							else {
								*(in6_addr *)Addr = ((PSOCKADDR_IN6)&AddressHostsTableIter.Address_Target.front())->sin6_addr;
							}

							goto StopLoop;
						}
					}
				}
			}
		}
	}
	else { //IPv4
		if (
		//Traditional DNS Poisoning addresses, see https://zh.wikipedia.org/wiki/%E5%9F%9F%E5%90%8D%E6%9C%8D%E5%8A%A1%E5%99%A8%E7%BC%93%E5%AD%98%E6%B1%A1%E6%9F%93#.E8.99.9A.E5.81.87IP.E5.9C.B0.E5.9D.80.
			((in_addr *)Addr)->s_addr == htonl(0x042442B2) || //4.36.66.178
			((in_addr *)Addr)->s_addr == htonl(0x0807C62D) || //8.7.198.45
			((in_addr *)Addr)->s_addr == htonl(0x253D369E) || //37.61.54.158
			((in_addr *)Addr)->s_addr == htonl(0x2E52AE44) || //46.82.174.68
			((in_addr *)Addr)->s_addr == htonl(0x3B1803AD) || //59.24.3.173
			((in_addr *)Addr)->s_addr == htonl(0x402158A1) || //64.33.88.161
			((in_addr *)Addr)->s_addr == htonl(0x4021632F) || //64.33.99.47
			((in_addr *)Addr)->s_addr == htonl(0x4042A3FB) || //64.66.163.251
			((in_addr *)Addr)->s_addr == htonl(0x4168CAFC) || //65.104.202.252
			((in_addr *)Addr)->s_addr == htonl(0x41A0DB71) || //65.160.219.113
			((in_addr *)Addr)->s_addr == htonl(0x422DFCED) || //66.45.252.237
			((in_addr *)Addr)->s_addr == htonl(0x480ECD63) || //72.14.205.99
			((in_addr *)Addr)->s_addr == htonl(0x480ECD68) || //72.14.205.104
			((in_addr *)Addr)->s_addr == htonl(0x4E10310F) || //78.16.49.15
			((in_addr *)Addr)->s_addr == htonl(0x5D2E0859) || //93.46.8.89
			((in_addr *)Addr)->s_addr == htonl(0x80797E8B) || //128.121.126.139
			((in_addr *)Addr)->s_addr == htonl(0x9F1803AD) || //159.24.3.173
			((in_addr *)Addr)->s_addr == htonl(0x9F6A794B) || //159.106.121.75
			((in_addr *)Addr)->s_addr == htonl(0xA9840D67) || //169.132.13.103
			((in_addr *)Addr)->s_addr == htonl(0xC043C606) || //192.67.198.6
			((in_addr *)Addr)->s_addr == htonl(0xCA6A0102) || //202.106.1.2
			((in_addr *)Addr)->s_addr == htonl(0xCAB50755) || //202.181.7.85
			((in_addr *)Addr)->s_addr == htonl(0xCB620741) || //203.98.7.65
			((in_addr *)Addr)->s_addr == htonl(0xCBA1E6AB) || //203.161.230.171
			((in_addr *)Addr)->s_addr == htonl(0xCF0C5862) || //207.12.88.98
			((in_addr *)Addr)->s_addr == htonl(0xD0381F2B) || //208.56.31.43
			((in_addr *)Addr)->s_addr == htonl(0xD1244921) || //209.36.73.33
			((in_addr *)Addr)->s_addr == htonl(0xD1913632) || //209.145.54.50
			((in_addr *)Addr)->s_addr == htonl(0xD1DC1EAE) || //209.220.30.174
			((in_addr *)Addr)->s_addr == htonl(0xD35E4293) || //211.94.66.147
			((in_addr *)Addr)->s_addr == htonl(0xD5A9FB23) || //213.169.251.35
			((in_addr *)Addr)->s_addr == htonl(0xD8DDBCD6) || //216.221.188.182
		//New DNS Poisoning addresses which had been added in May 2011, see http://forums.internetfreedom.org/index.php?topic=7953.0.
			((in_addr *)Addr)->s_addr == htonl(0x1759053C) || //23.89.5.60
			((in_addr *)Addr)->s_addr == htonl(0x31027B38) || //49.2.123.56
			((in_addr *)Addr)->s_addr == htonl(0x364C8701) || //54.76.135.1
			((in_addr *)Addr)->s_addr == htonl(0x4D04075C) || //77.4.7.92
			((in_addr *)Addr)->s_addr == htonl(0x76050460) || //118.5.4.96
			((in_addr *)Addr)->s_addr == htonl(0xBC050460) || //188.5.4.96
			((in_addr *)Addr)->s_addr == htonl(0xBDA31105) || //189.163.17.5
			((in_addr *)Addr)->s_addr == htonl(0xC504040C) || //197.4.4.12
			((in_addr *)Addr)->s_addr == htonl(0xD8EAB30D) || //216.234.179.13
//			((in_addr *)Addr)->s_addr == htonl(0xF3B9BB27) || //243.185.187.39, including in reserved address ranges
//			((in_addr *)Addr)->s_addr == htonl(0xF9812E30) || //249.129.46.48, including in reserved address ranges
//			((in_addr *)Addr)->s_addr == htonl(0xFD9D0EA5) || //253.157.14.165, including in reserved address ranges
		//China Network Anomaly in 2014-01-21, see https ://zh.wikipedia.org/wiki/2014%E5%B9%B4%E4%B8%AD%E5%9B%BD%E7%BD%91%E7%BB%9C%E5%BC%82%E5%B8%B8%E4%BA%8B%E4%BB%B6
			((in_addr *)Addr)->s_addr == htonl(0x413102B2) || //65.49.2.178
		//New addresses in IPv6 which has been added in September 2014, see https://code.google.com/p/goagent/issues/detail?id=17571.
			((in_addr *)Addr)->s_addr == htonl(0x01010101) || //1.1.1.1
			((in_addr *)Addr)->s_addr == htonl(0x0A0A0A0A) || //10.10.10.10
			((in_addr *)Addr)->s_addr == htonl(0x14141414) || //20.20.20.20
//			((in_addr *)Addr)->s_addr == htonl(0xFFFFFFFF) || //255.255.255.255, including in reserved address ranges
		//New DNS Poisoning addresses which had been added in December 2014, see https://www.v2ex.com/t/156926.
//			((in_addr *)Addr)->s_addr == 0 || //0.0.0.0, including in reserved address ranges
			((in_addr *)Addr)->s_addr == htonl(0x02010102) || //2.1.1.2
			((in_addr *)Addr)->s_addr == htonl(0x04C15000) || //4.193.80.0
			((in_addr *)Addr)->s_addr == htonl(0x08695400) || //8.105.84.0
			((in_addr *)Addr)->s_addr == htonl(0x0C578500) || //12.87.133.0
			((in_addr *)Addr)->s_addr == htonl(0x103F9B00) || //16.63.155.0
			((in_addr *)Addr)->s_addr == htonl(0x148B3800) || //20.139.56.0
			((in_addr *)Addr)->s_addr == htonl(0x1833B800) || //24.51.184.0
			((in_addr *)Addr)->s_addr == htonl(0x1C797E8B) || //28.121.126.139
			((in_addr *)Addr)->s_addr == htonl(0x1C0DD800) || //28.13.216.0
			((in_addr *)Addr)->s_addr == htonl(0x2E147EFC) || //46.20.126.252
			((in_addr *)Addr)->s_addr == htonl(0x2E2618D1) || //46.38.24.209
			((in_addr *)Addr)->s_addr == htonl(0x3D361C06) || //61.54.28.6
			((in_addr *)Addr)->s_addr == htonl(0x42CE0BC2) || //66.206.11.194
			((in_addr *)Addr)->s_addr == htonl(0x4A75398A) || //74.117.57.138
			((in_addr *)Addr)->s_addr == htonl(0x591F376A) || //89.31.55.106
			((in_addr *)Addr)->s_addr == htonl(0x710BC2BE) || //113.11.194.190
			((in_addr *)Addr)->s_addr == htonl(0x76053106) || //118.5.49.6
			((in_addr *)Addr)->s_addr == htonl(0x7ADA65BE) || //122.218.101.190
			((in_addr *)Addr)->s_addr == htonl(0x7B3231AB) || //123.50.49.171
			((in_addr *)Addr)->s_addr == htonl(0x7B7EF9EE) || //123.126.249.238
			((in_addr *)Addr)->s_addr == htonl(0x7DE69430) || //125.230.148.48
//			((in_addr *)Addr)->s_addr == htonl(0x7F000002) || //127.0.0.2, including in reserved address ranges
			((in_addr *)Addr)->s_addr == htonl(0xADC9D806) || //173.201.216.6
			((in_addr *)Addr)->s_addr == htonl(0xCBC73951) || //203.199.57.81
			((in_addr *)Addr)->s_addr == htonl(0xD06D8A37) || //208.109.138.55
			((in_addr *)Addr)->s_addr == htonl(0xD3058512) || //211.5.133.18
			((in_addr *)Addr)->s_addr == htonl(0xD308451B) || //211.8.69.27
			((in_addr *)Addr)->s_addr == htonl(0xD5BA2105) || //213.186.33.5
			((in_addr *)Addr)->s_addr == htonl(0xD88BD590) || //216.139.213.144
			((in_addr *)Addr)->s_addr == htonl(0xDD08451B) || //221.8.69.27
//			((in_addr *)Addr)->s_addr == htonl(0xF3B9BB03) || //243.185.187.3, including in reserved address ranges
//			((in_addr *)Addr)->s_addr == htonl(0xF3B9BB1E) || //243.185.187.30, including in reserved address ranges
		//DNS Poisoning addresses from CERNET2, see https://code.google.com/p/goagent/issues/detail?id=17571.
			((in_addr *)Addr)->s_addr == htonl(0x01020304) || //1.2.3.4
		//Special-use or reserved addresses, see https://en.wikipedia.org/wiki/IPv4#Special-use_addresses and https://en.wikipedia.org/wiki/Reserved_IP_addresses#Reserved_IPv4_addresses.
			((in_addr *)Addr)->s_net == 0 || //Current network whick only valid as source addresses(0.0.0.0/8, Section 3.2.1.3 in RFC 1122)
			(IsPrivateUse && ((in_addr *)Addr)->s_net == 0x0A) || //Private class A addresses(10.0.0.0/8, Section 3 in RFC 1918)
			((in_addr *)Addr)->s_net == 0x7F || //Loopback address(127.0.0.0/8, Section 3.2.1.3 in RFC 1122)
			( /* IsPrivateUse && */ ((in_addr *)Addr)->s_net == 0x64 && ((in_addr *)Addr)->s_host > 0x40 && ((in_addr *)Addr)->s_host < 0x7F) || //Carrier-grade NAT addresses(100.64.0.0/10, Section 7 in RFC 6598)
			(IsPrivateUse && ((in_addr *)Addr)->s_net == 0xA9 && ((in_addr *)Addr)->s_host >= 0xFE) || //Link-local addresses(169.254.0.0/16, Section 1.5 in RFC 3927)
			(IsPrivateUse && ((in_addr *)Addr)->s_net == 0xAC && ((in_addr *)Addr)->s_host >= 0x10 && ((in_addr *)Addr)->s_host <= 0x1F) || //Private class B addresses(172.16.0.0/12, Section 3 in RFC 1918)	
			(((in_addr *)Addr)->s_net == 0xC0 && ((in_addr *)Addr)->s_host == 0 && ((in_addr *)Addr)->s_lh == 0 && ((in_addr *)Addr)->s_impno >= 0 && ((in_addr *)Addr)->s_impno < 0x08) || //DS-Lite transition mechanism addresses(192.0.0.0/29, Section 3 in RFC 6333)
			(((in_addr *)Addr)->s_net == 0xC0 && (((in_addr *)Addr)->s_host == 0 && (((in_addr *)Addr)->s_lh == 0 || //Reserved for IETF protocol assignments addresses(192.0.0.0/24, Section 3 in RFC 5735)
			((in_addr *)Addr)->s_lh == 0x02))) || //TEST-NET-1 addresses(192.0.2.0/24, Section 3 in RFC 5735)
			(IsPrivateUse && ((in_addr *)Addr)->s_host == 0x58 && ((in_addr *)Addr)->s_lh == 0x63) || //6to4 relay/tunnel addresses(192.88.99.0/24, Section 2.3 in RFC 3068)
			(IsPrivateUse && ((in_addr *)Addr)->s_net == 0xC0 && ((in_addr *)Addr)->s_host == 0xA8) || //Private class C addresses(192.168.0.0/16, Section 3 in RFC 1918)
			(((in_addr *)Addr)->s_net == 0xC6 && (((in_addr *)Addr)->s_host == 0x12 || //Benchmarking Methodology for Network Interconnect Devices addresses(198.18.0.0/15, Section 11.4.1 in RFC 2544)
			(((in_addr *)Addr)->s_host == 0x33 && ((in_addr *)Addr)->s_lh == 0x64))) || //TEST-NET-2 addresses(198.51.100.0/24, Section 3 in RFC 5737)
			(((in_addr *)Addr)->s_net == 0xCB && ((in_addr *)Addr)->s_host == 0 && ((in_addr *)Addr)->s_lh == 0x71) || //TEST-NET-3 addresses(203.0.113.0/24, Section 3 in RFC 5737)
			(IsPrivateUse && ((in_addr *)Addr)->s_net == 0xE0) || //Multicast addresses(224.0.0.0/4, Section 2 in RFC 3171)
			((in_addr *)Addr)->s_net >= 0xF0) //Reserved for future use address(240.0.0.0/4, Section 4 in RFC 1112) and Broadcast addresses(255.255.255.255/32, Section 7 in RFC 919/RFC 922)
				return true;

	//Result Blacklist check
		if (Domain != nullptr)
		{
		//Domain Case Conversion
			std::string InnerDomain(Domain);
			CaseConvert(false, InnerDomain);

		//Main check
			std::lock_guard<std::mutex> IPFilterFileMutex(IPFilterFileLock);
			for (auto IPFilterFileSetIter:*IPFilterFileSetUsing)
			{
				for (auto ResultBlacklistTableIter:IPFilterFileSetIter.ResultBlacklist)
				{
					if (ResultBlacklistTableIter.Addresses.front().Begin.ss_family == AF_INET && 
						(ResultBlacklistTableIter.PatternString.empty() || std::regex_match(InnerDomain, ResultBlacklistTableIter.Pattern)))
					{
						for (auto AddressRangeTableIter:ResultBlacklistTableIter.Addresses)
						{
							if ((AddressRangeTableIter.End.ss_family == AF_INET && 
								AddressesComparing(Addr, &((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr, AF_INET) >= ADDRESS_COMPARE_EQUAL && 
								AddressesComparing(Addr, &((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr, AF_INET) <= ADDRESS_COMPARE_EQUAL) || 
								((in_addr *)Addr)->s_addr == ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_addr)
									return true;
						}
					}
				}
			}
		}

	//Address Hosts check
		std::lock_guard<std::mutex> HostsFileMutex(HostsFileLock);
		for (auto HostsFileSetIter:*HostsFileSetUsing)
		{
			for (auto AddressHostsTableIter:HostsFileSetIter.AddressHostsList)
			{
				if (AddressHostsTableIter.Address_Target.front().ss_family == AF_INET)
				{
					for (auto AddressRangeTableIter:AddressHostsTableIter.Address_Source)
					{
						if ((AddressRangeTableIter.Begin.ss_family == AF_INET && AddressRangeTableIter.End.ss_family == AF_INET && 
							AddressesComparing(Addr, &((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr, AF_INET) >= ADDRESS_COMPARE_EQUAL && 
							AddressesComparing(Addr, &((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr, AF_INET) <= ADDRESS_COMPARE_EQUAL) || 
							((in_addr *)Addr)->s_addr == ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_addr)
						{
							if (AddressHostsTableIter.Address_Target.size() > 1U)
							{
							//Get a ramdom one.
								std::uniform_int_distribution<size_t> RamdomDistribution(0, AddressHostsTableIter.Address_Target.size() - 1U);
								*(in_addr *)Addr = ((PSOCKADDR_IN)&AddressHostsTableIter.Address_Target.at(RamdomDistribution(*GlobalRunningStatus.RamdomEngine)))->sin_addr;
							}
							else {
								*(in_addr *)Addr = ((PSOCKADDR_IN)&AddressHostsTableIter.Address_Target.front())->sin_addr;
							}

							break;
						}
					}
				}
			}
		}
	}

//Jump here to stop loop.
StopLoop:
	return false;
}

//Check routing of addresses
bool __fastcall CheckAddressRouting(
	const void *Addr, 
	const uint16_t Protocol)
{
	std::lock_guard<std::mutex> IPFilterFileMutex(IPFilterFileLock);

//Check address routing.
	if (Protocol == AF_INET6) //IPv6
	{
		std::map<uint64_t, std::set<uint64_t>>::iterator AddrMapIter;
		for (auto IPFilterFileSetIter:*IPFilterFileSetUsing)
		{
			for (auto LocalRoutingTableIter:IPFilterFileSetIter.LocalRoutingList)
			{
				if (!LocalRoutingTableIter.AddressRoutingList_IPv6.empty())
				{
					if (LocalRoutingTableIter.Prefix < sizeof(in6_addr) * BYTES_TO_BITS / 2U)
					{
						if (LocalRoutingTableIter.AddressRoutingList_IPv6.count(ntoh64(*(PUINT64)Addr) & (UINT64_MAX << (sizeof(in6_addr) * BYTES_TO_BITS / 2U - LocalRoutingTableIter.Prefix))) > 0)
							return true;
					}
					else {
						AddrMapIter = LocalRoutingTableIter.AddressRoutingList_IPv6.find(ntoh64(*(PUINT64)Addr));
						if (AddrMapIter != LocalRoutingTableIter.AddressRoutingList_IPv6.end() && 
							AddrMapIter->second.count(ntoh64(*(PUINT64)((uint8_t *)Addr + sizeof(in6_addr) / 2U)) & (UINT64_MAX << (sizeof(in6_addr) * BYTES_TO_BITS - LocalRoutingTableIter.Prefix))) > 0)
								return true;
					}
				}
			}
		}
	}
	else { //IPv4
		for (auto IPFilterFileSetIter:*IPFilterFileSetUsing)
		{
			for (auto LocalRoutingTableIter:IPFilterFileSetIter.LocalRoutingList)
			{
				if (LocalRoutingTableIter.AddressRoutingList_IPv4.count(ntohl(((in_addr *)Addr)->s_addr) & (UINT32_MAX << (sizeof(in_addr) * BYTES_TO_BITS - LocalRoutingTableIter.Prefix))) > 0)
					return true;
			}
		}
	}

	return false;
}

//Custom Mode address filter
bool __fastcall CheckCustomModeFilter(
	const void *OriginalAddr, 
	const uint16_t Protocol)
{
	std::lock_guard<std::mutex> IPFilterFileMutex(IPFilterFileLock);
	if (Protocol == AF_INET6) //IPv6
	{
	//Permit
		if (Parameter.IPFilterType)
		{
			for (auto IPFilterFileSetIter:*IPFilterFileSetUsing)
			{
				for (auto AddressRangeTableIter:IPFilterFileSetIter.AddressRange)
				{
				//Check Protocol and Level.
					if (AddressRangeTableIter.Begin.ss_family != AF_INET6 || (Parameter.IPFilterLevel > 0 && AddressRangeTableIter.Level < Parameter.IPFilterLevel))
						continue;

				//Check address.
					for (size_t Index = 0;Index < sizeof(in6_addr) / sizeof(uint16_t);++Index)
					{
						if (ntohs(((in6_addr *)OriginalAddr)->s6_words[Index]) > ntohs(((PSOCKADDR_IN6)&AddressRangeTableIter.Begin)->sin6_addr.s6_words[Index]) && 
							ntohs(((in6_addr *)OriginalAddr)->s6_words[Index]) < ntohs(((PSOCKADDR_IN6)&AddressRangeTableIter.End)->sin6_addr.s6_words[Index]))
						{
							return true;
						}
						else if (((in6_addr *)OriginalAddr)->s6_words[Index] == ((PSOCKADDR_IN6)&AddressRangeTableIter.Begin)->sin6_addr.s6_words[Index] || 
							((in6_addr *)OriginalAddr)->s6_words[Index] == ((PSOCKADDR_IN6)&AddressRangeTableIter.End)->sin6_addr.s6_words[Index])
						{
							if (Index == sizeof(in6_addr) / sizeof(uint16_t) - 1U)
								return true;
							else 
								continue;
						}
						else {
							return false;
						}
					}
				}
			}
		}
	//Deny
		else {
			for (auto IPFilterFileSetIter:*IPFilterFileSetUsing)
			{
				for (auto AddressRangeTableIter:IPFilterFileSetIter.AddressRange)
				{
				//Check Protocol and Level.
					if (AddressRangeTableIter.Begin.ss_family != AF_INET6 || (Parameter.IPFilterLevel > 0 && AddressRangeTableIter.Level < Parameter.IPFilterLevel))
						continue;

				//Check address.
					for (size_t Index = 0;Index < sizeof(in6_addr) / sizeof(uint16_t);++Index)
					{
						if (ntohs(((in6_addr *)OriginalAddr)->s6_words[Index]) > ntohs(((PSOCKADDR_IN6)&AddressRangeTableIter.Begin)->sin6_addr.s6_words[Index]) && 
							ntohs(((in6_addr *)OriginalAddr)->s6_words[Index]) < ntohs(((PSOCKADDR_IN6)&AddressRangeTableIter.End)->sin6_addr.s6_words[Index]))
						{
							return false;
						}
						else if (((in6_addr *)OriginalAddr)->s6_words[Index] == ((PSOCKADDR_IN6)&AddressRangeTableIter.Begin)->sin6_addr.s6_words[Index] || 
							((in6_addr *)OriginalAddr)->s6_words[Index] == ((PSOCKADDR_IN6)&AddressRangeTableIter.End)->sin6_addr.s6_words[Index])
						{
							if (Index == sizeof(in6_addr) / sizeof(uint16_t) - 1U)
								return false;
							else 
								continue;
						}
						else {
							return true;
						}
					}
				}
			}
		}
	}
	else { //IPv4
	//Permit
		if (Parameter.IPFilterType)
		{
			for (auto IPFilterFileSetIter:*IPFilterFileSetUsing)
			{
				for (auto AddressRangeTableIter:IPFilterFileSetIter.AddressRange)
				{
				//Check Protocol and Level.
					if (AddressRangeTableIter.Begin.ss_family != AF_INET || (Parameter.IPFilterLevel > 0 && AddressRangeTableIter.Level < Parameter.IPFilterLevel))
						continue;

				//Check address.
					if (((in_addr *)OriginalAddr)->s_net > ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_net && 
						((in_addr *)OriginalAddr)->s_net < ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_net)
					{
						return true;
					}
					else if (((in_addr *)OriginalAddr)->s_net == ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_net || 
						((in_addr *)OriginalAddr)->s_net == ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_net)
					{
						if (((in_addr *)OriginalAddr)->s_host > ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_host && 
							((in_addr *)OriginalAddr)->s_host < ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_host)
						{
							return true;
						}
						else if (((in_addr *)OriginalAddr)->s_host == ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_host || 
							((in_addr *)OriginalAddr)->s_host == ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_host)
						{
							if (((in_addr *)OriginalAddr)->s_lh > ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_lh && 
								((in_addr *)OriginalAddr)->s_lh < ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_lh)
							{
								return true;
							}
							else if (((in_addr *)OriginalAddr)->s_lh == ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_lh || 
								((in_addr *)OriginalAddr)->s_lh == ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_lh)
							{
								if (((in_addr *)OriginalAddr)->s_impno >= ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_impno && 
									((in_addr *)OriginalAddr)->s_impno <= ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_impno)
								{
									return true;
								}
								else {
									return false;
								}
							}
							else {
								return false;
							}
						}
						else {
							return false;
						}
					}
					else {
						return false;
					}
				}
			}
		}
	//Deny
		else {
			for (auto IPFilterFileSetIter:*IPFilterFileSetUsing)
			{
				for (auto AddressRangeTableIter:IPFilterFileSetIter.AddressRange)
				{
				//Check Protocol and Level.
					if (AddressRangeTableIter.Begin.ss_family != AF_INET || (Parameter.IPFilterLevel > 0 && AddressRangeTableIter.Level < Parameter.IPFilterLevel))
						continue;

				//Check address.
					if (((in_addr *)OriginalAddr)->s_net > ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_net && 
						((in_addr *)OriginalAddr)->s_net < ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_net)
					{
						return false;
					}
					else if (((in_addr *)OriginalAddr)->s_net == ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_net || 
						((in_addr *)OriginalAddr)->s_net == ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_net)
					{
						if (((in_addr *)OriginalAddr)->s_host > ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_host && ((in_addr *)OriginalAddr)->s_host < ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_host)
						{
							return false;
						}
						else if (((in_addr *)OriginalAddr)->s_host == ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_host || 
							((in_addr *)OriginalAddr)->s_host == ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_host)
						{
							if (((in_addr *)OriginalAddr)->s_lh > ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_lh && 
								((in_addr *)OriginalAddr)->s_lh < ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_lh)
							{
								return false;
							}
							else if (((in_addr *)OriginalAddr)->s_lh == ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_lh || 
								((in_addr *)OriginalAddr)->s_lh == ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_lh)
							{
								if (((in_addr *)OriginalAddr)->s_impno >= ((PSOCKADDR_IN)&AddressRangeTableIter.Begin)->sin_addr.s_impno && ((in_addr *)OriginalAddr)->s_impno <= ((PSOCKADDR_IN)&AddressRangeTableIter.End)->sin_addr.s_impno)
								{
									return false;
								}
								else {
									return true;
								}
							}
							else {
								return true;
							}
						}
						else {
							return true;
						}
					}
					else {
						return true;
					}
				}
			}
		}
	}

	return true;
}

//Count DNS Query Name length
size_t __fastcall CheckQueryNameLength(
	const char *Buffer)
{
	size_t Index = 0;
	for (Index = 0;Index < DOMAIN_MAXSIZE;++Index)
	{
		if (Buffer[Index] == 0)
		{
			break;
		}
		else if ((uint8_t)Buffer[Index] >= DNS_POINTER_8_BITS)
		{
			return Index + sizeof(uint16_t) - 1U;
		}
	}

	return Index;
}

//Check response CNAME resource records
size_t __fastcall CheckResponseCNAME(
	char *Buffer, 
	const size_t Length, 
	const size_t CNAME_Index, 
	const size_t CNAME_Length, 
	const size_t BufferSize, 
	size_t &RecordNum)
{
//Mark whole DNS query.
	std::string Domain;
	if (MarkWholeDNSQuery(Buffer, Length, Buffer + CNAME_Index, CNAME_Index, Domain) <= DOMAIN_MINSIZE)
		return EXIT_FAILURE;
	auto DNS_Header = (pdns_hdr)Buffer;
	auto DNS_Query = (pdns_qry)(Buffer + DNS_PACKET_QUERY_LOCATE(Buffer));
	size_t DataLength = 0;
	RecordNum = 0;
	CaseConvert(false, Domain);

//CNAME Hosts
	std::lock_guard<std::mutex> HostsFileMutex(HostsFileLock);
	for (auto HostsFileSetIter:*HostsFileSetUsing)
	{
		for (auto HostsTableIter:HostsFileSetIter.HostsList_CNAME)
		{
			if (std::regex_match(Domain, HostsTableIter.Pattern))
			{
			//Check white and banned hosts list, empty record type list check
				DataLength = CheckWhiteBannedHostsProcess(Length, HostsTableIter, DNS_Header, DNS_Query, nullptr);
				if (DataLength >= DNS_PACKET_MINSIZE)
					return DataLength;
				else if (HostsTableIter.RecordTypeList.empty())
					continue;

			//Initialization
				void *DNS_Record = nullptr;
				size_t RamdomIndex = 0, Index = 0;

			//IPv6(AAAA records)
				if (DNS_Query->Type == htons(DNS_RECORD_AAAA) && HostsTableIter.RecordTypeList.front() == htons(DNS_RECORD_AAAA))
				{
				//Set header flags and convert DNS query to DNS response packet.
//					DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R);
					DNS_Header->Flags = htons(DNS_SQR_NE);
					DataLength = CNAME_Index + CNAME_Length;
					memset(Buffer + DataLength, 0, BufferSize - DataLength);

				//Hosts load balancing
					if (HostsTableIter.AddrList.size() > 1U)
					{
						std::uniform_int_distribution<size_t> RamdomDistribution(0, HostsTableIter.AddrList.size() - 1U);
						RamdomIndex = RamdomDistribution(*GlobalRunningStatus.RamdomEngine);
					}

				//Make response.
					for (Index = 0;Index < HostsTableIter.AddrList.size();++Index)
					{
					//Make resource records.
						DNS_Record = (pdns_record_aaaa)(Buffer + DataLength);
						DataLength += sizeof(dns_record_aaaa);
						((pdns_record_aaaa)DNS_Record)->Name = htons((uint16_t)CNAME_Index | DNS_POINTER_16_BITS);
						((pdns_record_aaaa)DNS_Record)->Classes = htons(DNS_CLASS_IN);
						((pdns_record_aaaa)DNS_Record)->TTL = htonl(Parameter.HostsDefaultTTL);
						((pdns_record_aaaa)DNS_Record)->Type = htons(DNS_RECORD_AAAA);
						((pdns_record_aaaa)DNS_Record)->Length = htons(sizeof(in6_addr));
						if (Index == 0)
							((pdns_record_aaaa)DNS_Record)->Addr = HostsTableIter.AddrList.at(RamdomIndex).IPv6.sin6_addr;
						else if (Index == RamdomIndex)
							((pdns_record_aaaa)DNS_Record)->Addr = HostsTableIter.AddrList.at(0).IPv6.sin6_addr;
						else 
							((pdns_record_aaaa)DNS_Record)->Addr = HostsTableIter.AddrList.at(Index).IPv6.sin6_addr;

					//Hosts items length check
						if (((Parameter.EDNS_Label || DNS_Header->Additional > 0) && DataLength + sizeof(dns_record_aaaa) + EDNS_ADDITIONAL_MAXSIZE >= BufferSize) || //EDNS Label
							DataLength + sizeof(dns_record_aaaa) >= BufferSize) //Normal query
						{
							++Index;
							break;
						}
					}

				//Set DNS counts and EDNS Label
					RecordNum = Index;
					DNS_Header->Authority = 0;
					if (Parameter.EDNS_Label || DNS_Header->Additional > 0)
					{
						DNS_Header->Additional = 0;
						DataLength = AddEDNSLabelToAdditionalRR(Buffer, DataLength, BufferSize, nullptr);
					}
					
					return DataLength;
				}
			//IPv4(A records)
				else if (DNS_Query->Type == htons(DNS_RECORD_A) && HostsTableIter.RecordTypeList.front() == htons(DNS_RECORD_A))
				{
				//Set header flags and convert DNS query to DNS response packet.
//					DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R);
					DNS_Header->Flags = htons(DNS_SQR_NE);
					DataLength = CNAME_Index + CNAME_Length;
					memset(Buffer + DataLength, 0, BufferSize - DataLength);

				//Hosts load balancing
					if (HostsTableIter.AddrList.size() > 1U)
					{
						std::uniform_int_distribution<size_t> RamdomDistribution(0, HostsTableIter.AddrList.size() - 1U);
						RamdomIndex = RamdomDistribution(*GlobalRunningStatus.RamdomEngine);
					}

				//Make response.
					for (Index = 0;Index < HostsTableIter.AddrList.size();++Index)
					{
					//Make resource records.
						DNS_Record = (pdns_record_a)(Buffer + DataLength);
						DataLength += sizeof(dns_record_a);
						((pdns_record_a)DNS_Record)->Name = htons((uint16_t)CNAME_Index | DNS_POINTER_16_BITS);
						((pdns_record_a)DNS_Record)->Classes = htons(DNS_CLASS_IN);
						((pdns_record_a)DNS_Record)->TTL = htonl(Parameter.HostsDefaultTTL);
						((pdns_record_a)DNS_Record)->Type = htons(DNS_RECORD_A);
						((pdns_record_a)DNS_Record)->Length = htons(sizeof(in_addr));
						if (Index == 0)
							((pdns_record_a)DNS_Record)->Addr = HostsTableIter.AddrList.at(RamdomIndex).IPv4.sin_addr;
						else if (Index == RamdomIndex)
							((pdns_record_a)DNS_Record)->Addr = HostsTableIter.AddrList.at(0).IPv4.sin_addr;
						else 
							((pdns_record_a)DNS_Record)->Addr = HostsTableIter.AddrList.at(Index).IPv4.sin_addr;

					//Hosts items length check
						if (((Parameter.EDNS_Label || DNS_Header->Additional > 0) && DataLength + sizeof(dns_record_a) + EDNS_ADDITIONAL_MAXSIZE >= BufferSize) || //EDNS Label
							DataLength + sizeof(dns_record_a) >= BufferSize) //Normal query
						{
							++Index;
							break;
						}
					}

				//Set DNS counts and EDNS Label
					RecordNum = Index;
					DNS_Header->Authority = 0;
					if (Parameter.EDNS_Label || DNS_Header->Additional > 0)
					{
						DNS_Header->Additional = 0;
						DataLength = AddEDNSLabelToAdditionalRR(Buffer, DataLength, BufferSize, nullptr);
					}
					
					return DataLength;
				}
			}
		}
	}

	return EXIT_SUCCESS;
}

//Check DNS query data
bool __fastcall CheckQueryData(
	DNS_PACKET_DATA *Packet, 
	char *SendBuffer, 
	const size_t SendSize, 
	const SOCKET_DATA &LocalSocketData)
{
//Check address.
	if (!(Packet != nullptr && SendBuffer != nullptr && Packet->Protocol == IPPROTO_TCP && Packet->Length >= DNS_PACKET_MINSIZE))
	{
		if (LocalSocketData.AddrLen == sizeof(sockaddr_in6)) //IPv6
		{
			if (CheckEmptyBuffer(&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr, sizeof(in6_addr)) || //Empty address
			//Check Private Mode(IPv6).
				(Parameter.OperationMode == LISTEN_MODE_PRIVATE && 
				!((((in6_addr *)&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr)->s6_bytes[0] >= 0xFC && ((in6_addr *)&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr)->s6_bytes[0] <= 0xFD) || //Unique Local Unicast address/ULA(FC00::/7, Section 2.5.7 in RFC 4193)
				(((in6_addr *)&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr)->s6_bytes[0] == 0xFE && ((in6_addr *)&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr)->s6_bytes[1U] >= 0x80 && ((in6_addr *)&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr)->s6_bytes[1U] <= 0xBF) || //Link-Local Unicast Contrast address(FE80::/10, Section 2.5.6 in RFC 4291)
				(((in6_addr *)&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr)->s6_words[6U] == 0 && ((in6_addr *)&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr)->s6_words[7U] == htons(0x0001)))) || //Loopback address(::1, Section 2.5.3 in RFC 4291)
			//Check Custom Mode(IPv6).
				(Parameter.OperationMode == LISTEN_MODE_CUSTOM && !CheckCustomModeFilter(&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr, AF_INET6)))
					return false;
		}
		else { //IPv4
			if ((*(in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr).s_addr == 0 || //Empty address
			//Check Private Mode(IPv4).
				(Parameter.OperationMode == LISTEN_MODE_PRIVATE && 
				!(((in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr)->s_net == 0x0A || //Private class A address(10.0.0.0/8, Section 3 in RFC 1918)
				((in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr)->s_net == 0x7F || //Loopback address(127.0.0.0/8, Section 3.2.1.3 in RFC 1122)
				(((in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr)->s_net == 0xA9 && ((in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr)->s_host >= 0xFE) || //Link-local addresses(169.254.0.0/16, Section 1.5 in RFC 3927)
				(((in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr)->s_net == 0xAC && ((in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr)->s_host >= 0x10 && ((in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr)->s_host <= 0x1F) || //Private class B address(172.16.0.0/12, Section 3 in RFC 1918)
				(((in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr)->s_net == 0xC0 && ((in_addr *)&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr)->s_host == 0xA8))) || //Private class C address(192.168.0.0/16, Section 3 in RFC 1918)
			//Check Custom Mode(IPv4).
				(Parameter.OperationMode == LISTEN_MODE_CUSTOM && !CheckCustomModeFilter(&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr, AF_INET)))
					return false;
		}
	}

//Check address only.
	if (Packet == nullptr || SendBuffer == nullptr || Packet->Protocol == 0 || Packet->Length < DNS_PACKET_MINSIZE)
		return true;

//Check request packet data.
	auto DNS_Header = (pdns_hdr)Packet->Buffer;
	if (
	//Base DNS header check
		DNS_Header->ID == 0 || //ID must not be set 0.
//		DNS_Header->Flags == 0 || //Flags must not be set 0.
	//Extended DNS header check
		(Parameter.HeaderCheck_DNS && 
	//Must not set Response bit.
		((ntohs(DNS_Header->Flags) & DNS_GET_BIT_RESPONSE) > 0 || 
	//Must not set Truncated bit.
		(ntohs(DNS_Header->Flags) & DNS_GET_BIT_TC) > 0 || 
	//Must not set Reserved bit.
		(ntohs(DNS_Header->Flags) & DNS_GET_BIT_Z) > 0 || 
	//Must not set RCode.
		(ntohs(DNS_Header->Flags) & DNS_GET_BIT_RCODE) > 0 || 
	//Question Resource Records Counts must be set 1.
		DNS_Header->Question != htons(U16_NUM_ONE) || 
	//Answer Resource Records Counts must be set 0.
		DNS_Header->Answer > 0 || 
	//Authority Resource Records Counts must be set 0.
		DNS_Header->Authority > 0 || 
	//Additional Resource Records Counts must be set 1 or 0.
		ntohs(DNS_Header->Additional) > U16_NUM_ONE)))
			return false;

//Check Compression Pointer Mutation.
	size_t Index = 0;
	for (Index = sizeof(dns_hdr);Index < DNS_PACKET_QUERY_LOCATE(Packet->Buffer);++Index)
	{
		if (*(Packet->Buffer + Index) == DNS_POINTER_8_BITS_STRING)
			continue;
	}
	if (Index != DNS_PACKET_QUERY_LOCATE(Packet->Buffer))
	{
		if (Packet->Length >= DNS_PACKET_MINSIZE)
		{
//			DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R_FE);
			DNS_Header->Flags = htons(DNS_SET_R_FE);
			SendToRequester(Packet->Buffer, Packet->Length, Packet->BufferSize, Packet->Protocol, LocalSocketData);
		}

		return false;
	}

//Scan all Resource Records.
	size_t PacketIndex = DNS_PACKET_RR_LOCATE(Packet->Buffer);
	pdns_record_standard DNS_Record_Standard = nullptr;
	uint16_t DNS_Pointer = 0;
	Packet->Question = CheckQueryNameLength(Packet->Buffer + sizeof(dns_hdr)) + 1U + sizeof(dns_qry);
	Packet->Answer = 0, Packet->Authority = 0, Packet->Additional = 0, Packet->EDNS_Record = 0;
	for (Index = 0;Index < (size_t)(ntohs(DNS_Header->Answer) + ntohs(DNS_Header->Authority) + ntohs(DNS_Header->Additional));++Index)
	{
	//Pointer check
		if (PacketIndex + sizeof(uint16_t) < Packet->Length && (uint8_t)Packet->Buffer[PacketIndex] >= DNS_POINTER_8_BITS)
		{
			DNS_Pointer = ntohs(*(uint16_t *)(Packet->Buffer + PacketIndex)) & DNS_POINTER_BITS_GET_LOCATE;
			if (DNS_Pointer >= Packet->Length || DNS_Pointer < sizeof(dns_hdr) || DNS_Pointer == PacketIndex || DNS_Pointer == PacketIndex + 1U)
				return false;
		}

	//Resource Records name
		auto RecordLength = CheckQueryNameLength(Packet->Buffer + PacketIndex) + 1U;
		if (PacketIndex + RecordLength + sizeof(dns_record_standard) > Packet->Length)
			return false;

	//Standard Resource Records
		DNS_Record_Standard = (pdns_record_standard)(Packet->Buffer + PacketIndex + RecordLength);
		RecordLength += sizeof(dns_record_standard);
		if (PacketIndex + RecordLength > Packet->Length || PacketIndex + RecordLength + ntohs(DNS_Record_Standard->Length) > Packet->Length)
			return false;

	//Mark records length.
		RecordLength += ntohs(DNS_Record_Standard->Length);
		PacketIndex += RecordLength;
		if (Index >= (size_t)(ntohs(DNS_Header->Answer) + ntohs(DNS_Header->Authority))) //Additional counts
		{
		//EDNS Label check
			if (Index == (size_t)(ntohs(DNS_Header->Answer) + ntohs(DNS_Header->Authority) + ntohs(DNS_Header->Additional) - 1U) && 
				DNS_Record_Standard->Type == htons(DNS_RECORD_OPT))
			{
				Packet->EDNS_Record += RecordLength;
				break;
			}
			else {
				Packet->Additional += RecordLength;
			}
		}
		else if (Index >= (size_t)(ntohs(DNS_Header->Answer))) //Authority counts
		{
			Packet->Authority += RecordLength;
		}
		else { //Answer counts
			Packet->Answer += RecordLength;
		}
	}

//UDP Truncated check
	if (Packet->Protocol == IPPROTO_UDP)
	{
		if (Packet->Length + EDNS_ADDITIONAL_MAXSIZE > Parameter.EDNSPayloadSize && (Parameter.EDNS_Label || Packet->Length > Parameter.EDNSPayloadSize))
		{
		//Make packets with EDNS Label.
//			DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_RTC);
			DNS_Header->Flags = htons(DNS_SET_RTC);
			AddEDNSLabelToAdditionalRR(Packet, nullptr);

		//Send request.
			if (Packet->Length >= DNS_PACKET_MINSIZE)
				SendToRequester(Packet->Buffer, Packet->Length, Packet->BufferSize, Packet->Protocol, LocalSocketData);

			return false;
		}
	}

//EDNS Label
	if (Parameter.EDNS_Label)
	{
	//Check special address.
		PSOCKET_DATA EDNSSocketData = nullptr;
		if ((LocalSocketData.AddrLen == sizeof(sockaddr_in6) && !CheckSpecialAddress(&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr, AF_INET6, true, nullptr)) || //IPv6
			(LocalSocketData.AddrLen == sizeof(sockaddr_in) && !CheckSpecialAddress(&((PSOCKADDR_IN)&LocalSocketData.SockAddr)->sin_addr, AF_INET, true, nullptr))) //IPv4
				EDNSSocketData = (PSOCKET_DATA)&LocalSocketData;

	//Add EDNS Label to query data.
		AddEDNSLabelToAdditionalRR(Packet, EDNSSocketData);
	}

//Check Hosts.
	memset(SendBuffer, 0, SendSize);
	auto DataLength = CheckHostsProcess(Packet, SendBuffer, SendSize, LocalSocketData);
	if (DataLength >= DNS_PACKET_MINSIZE)
	{
		SendToRequester(SendBuffer, DataLength, SendSize, Packet->Protocol, LocalSocketData);
		return false;
	}

	return true;
}

//Check DNS response results
size_t __fastcall CheckResponseData(
	const size_t ResponseType, 
	char *Buffer, 
	const size_t Length, 
	const size_t BufferSize, 
	bool *IsMarkHopLimit)
{
//DNS Options part
	auto DNS_Header = (pdns_hdr)Buffer;
	if (
	//Base DNS header check
		DNS_Header->ID == 0 || //ID must not be set 0.
		DNS_Header->Flags == 0 || //Flags must not be set 0.
	//NoCheck flag
		(ResponseType != REQUEST_PROCESS_DNSCURVE_SIGN && 
	//Extended DNS header check
		Parameter.HeaderCheck_DNS && 
	//Must not set Response bit.
		((ntohs(DNS_Header->Flags) & DNS_GET_BIT_RESPONSE) == 0 || 
	//Must not any Non-Question Resource Records when RCode is No Error and not Truncated
		((ntohs(DNS_Header->Flags) & DNS_GET_BIT_TC) == 0 && (ntohs(DNS_Header->Flags) & DNS_GET_BIT_RCODE) == DNS_RCODE_NOERROR && DNS_Header->Answer == 0 && 
		DNS_Header->Authority == 0 && DNS_Header->Additional == 0) || 
	//Responses are not authoritative when there are no Authoritative Nameservers Records and Additional Resource Records.
//		((ntohs(DNS_Header->Flags) & DNS_GET_BIT_AA) != 0 && DNS_Header->Authority == 0 && DNS_Header->Additional == 0) || 
	//Do query recursively bit must be set when RCode is No Error and there are Answers Resource Records.
		((ntohs(DNS_Header->Flags) & DNS_GET_BIT_RD) == 0 && (ntohs(DNS_Header->Flags) & DNS_GET_BIT_RCODE) == DNS_RCODE_NOERROR && DNS_Header->Answer == 0) || 
	//Local request failed or Truncated
		(ResponseType == REQUEST_PROCESS_LOCAL && 
		((ntohs(DNS_Header->Flags) & DNS_GET_BIT_RCODE) > DNS_RCODE_NOERROR || ((ntohs(DNS_Header->Flags) & DNS_GET_BIT_TC) > 0 && DNS_Header->Answer == 0))) || 
	//Must not set Reserved bit.
		(ntohs(DNS_Header->Flags) & DNS_GET_BIT_Z) > 0 || 
	//Question Resource Records Counts must be set 1.
		DNS_Header->Question != htons(U16_NUM_ONE) || 
	//Additional EDNS Label Resource Records check
		(Parameter.EDNS_Label && DNS_Header->Additional == 0 && 
		(ResponseType == 0 || //Normal
		(ResponseType == REQUEST_PROCESS_LOCAL && Parameter.EDNS_Switch.EDNS_Local) || //Local
		(ResponseType == REQUEST_PROCESS_SOCKS && Parameter.EDNS_Switch.EDNS_SOCKS) || //SOCKS Proxy
		(ResponseType == REQUEST_PROCESS_HTTP && Parameter.EDNS_Switch.EDNS_HTTP) || //HTTP Proxy
		(ResponseType == REQUEST_PROCESS_DIRECT && Parameter.EDNS_Switch.EDNS_Direct) || //Direct Request
		(ResponseType == REQUEST_PROCESS_DNSCURVE && Parameter.EDNS_Switch.EDNS_DNSCurve) || //DNSCurve
		(ResponseType == REQUEST_PROCESS_TCP && Parameter.EDNS_Switch.EDNS_TCP) || //TCP
		(ResponseType == REQUEST_PROCESS_UDP && Parameter.EDNS_Switch.EDNS_UDP)))))) //UDP
			return EXIT_FAILURE;

//Responses question pointer check
	if (ResponseType != REQUEST_PROCESS_DNSCURVE_SIGN && Parameter.HeaderCheck_DNS)
	{
		for (size_t Index = sizeof(dns_hdr);Index < DNS_PACKET_QUERY_LOCATE(Buffer);++Index)
		{
			if (*(Buffer + Index) == DNS_POINTER_8_BITS_STRING)
				return EXIT_FAILURE;
		}

	//Check repeating DNS Domain without Compression.
		if (DNS_Header->Answer == htons(U16_NUM_ONE) && DNS_Header->Authority == 0 && DNS_Header->Additional == 0 && 
			CheckQueryNameLength(Buffer + sizeof(dns_hdr)) == CheckQueryNameLength(Buffer + DNS_PACKET_RR_LOCATE(Buffer)))
		{
			if (((pdns_record_standard)(Buffer + DNS_PACKET_RR_LOCATE(Buffer) + CheckQueryNameLength(Buffer + sizeof(dns_hdr)) + 1U))->Classes == htons(DNS_CLASS_IN) && 
				(((pdns_record_standard)(Buffer + DNS_PACKET_RR_LOCATE(Buffer) + CheckQueryNameLength(Buffer + sizeof(dns_hdr)) + 1U))->Type == htons(DNS_RECORD_A) || 
				((pdns_record_standard)(Buffer + DNS_PACKET_RR_LOCATE(Buffer) + CheckQueryNameLength(Buffer + sizeof(dns_hdr)) + 1U))->Type == htons(DNS_RECORD_AAAA)) && 
				memcmp(Buffer + sizeof(dns_hdr), Buffer + DNS_PACKET_RR_LOCATE(Buffer), CheckQueryNameLength(Buffer + sizeof(dns_hdr)) + 1U) == 0)
					return EXIT_FAILURE;
		}
	}

//Mark domain.
	std::string Domain;
	const char *DomainString = nullptr;
	DNSQueryToChar(Buffer + sizeof(dns_hdr), Domain);
	if (!Domain.empty())
		DomainString = Domain.c_str();

//Initialization
	auto DNS_Query = (pdns_qry)(Buffer + DNS_PACKET_QUERY_LOCATE(Buffer));
	size_t DataLength = DNS_PACKET_RR_LOCATE(Buffer);
	uint16_t DNS_Pointer = 0, BeforeType = 0;
	pdns_record_standard DNS_Record_Standard = nullptr;
	void *Addr = nullptr;
	auto IsEDNS_Label = false, IsDNSSEC_Records = false, IsGotAddressResult = false;

//Scan all Resource Records.
	for (size_t Index = 0;Index < (size_t)(ntohs(DNS_Header->Answer) + ntohs(DNS_Header->Authority) + ntohs(DNS_Header->Additional));++Index)
	{
	//Pointer check
		if (DataLength + sizeof(uint16_t) < Length && (uint8_t)Buffer[DataLength] >= DNS_POINTER_8_BITS)
		{
			DNS_Pointer = ntohs(*(uint16_t *)(Buffer + DataLength)) & DNS_POINTER_BITS_GET_LOCATE;
			if (DNS_Pointer >= Length || DNS_Pointer < sizeof(dns_hdr) || DNS_Pointer == DataLength || DNS_Pointer == DataLength + 1U)
				return EXIT_FAILURE;
		}

	//Resource Records name
		DataLength += CheckQueryNameLength(Buffer + DataLength) + 1U;
		if (DataLength + sizeof(dns_record_standard) > Length)
			return EXIT_FAILURE;

	//Standard Resource Records
		DNS_Record_Standard = (pdns_record_standard)(Buffer + DataLength);
		DataLength += sizeof(dns_record_standard);
		if (DataLength > Length || DataLength + ntohs(DNS_Record_Standard->Length) > Length)
			return EXIT_FAILURE;

	//CNAME Hosts
		if (Index < ntohs(DNS_Header->Answer) && DNS_Record_Standard->Classes == htons(DNS_CLASS_IN) && DNS_Record_Standard->TTL > 0 && 
			DNS_Record_Standard->Type == htons(DNS_RECORD_CNAME) && DNS_Record_Standard->Length >= DOMAIN_MINSIZE && 
			DataLength + ntohs(DNS_Record_Standard->Length) < Length)
		{
			size_t RecordNum = 0;
			auto CNAME_DataLength = CheckResponseCNAME(Buffer, Length, DataLength, ntohs(DNS_Record_Standard->Length), BufferSize, RecordNum);
			if (CNAME_DataLength >= DNS_PACKET_MINSIZE && RecordNum > 0)
			{
				DNS_Header->Answer = htons((uint16_t)(Index + 1U + RecordNum));
				return CNAME_DataLength;
			}
		}

	//EDNS Label(OPT Records) and DNSSEC Records(RRSIG/DNSKEY/DS/NSEC/NSEC3/NSEC3PARAM) check
		if (ResponseType != REQUEST_PROCESS_DNSCURVE_SIGN && Parameter.EDNS_Label)
		{
			if (DNS_Record_Standard->Type == htons(DNS_RECORD_OPT))
				IsEDNS_Label = true;
			else if (Parameter.DNSSEC_Request && 
				(DNS_Record_Standard->Type == htons(DNS_RECORD_SIG) || DNS_Record_Standard->Type == htons(DNS_RECORD_KEY) || 
				DNS_Record_Standard->Type == htons(DNS_RECORD_DS) || DNS_Record_Standard->Type == htons(DNS_RECORD_RRSIG) || 
				DNS_Record_Standard->Type == htons(DNS_RECORD_NSEC) || DNS_Record_Standard->Type == htons(DNS_RECORD_DNSKEY) || 
				DNS_Record_Standard->Type == htons(DNS_RECORD_NSEC3) || DNS_Record_Standard->Type == htons(DNS_RECORD_NSEC3PARAM) || 
				DNS_Record_Standard->Type == htons(DNS_RECORD_CDS) || DNS_Record_Standard->Type == htons(DNS_RECORD_CDNSKEY)))
			{
				IsDNSSEC_Records = true;

			//DNSSEC Validation
				if (Parameter.DNSSEC_Validation && !CheckDNSSECRecords(Buffer + DataLength, ntohs(DNS_Record_Standard->Length), DNS_Record_Standard->Type, BeforeType))
					return EXIT_FAILURE;
			}
		}

	//Read Resource Records data
		if (ResponseType != REQUEST_PROCESS_DNSCURVE && DNS_Record_Standard->Classes == htons(DNS_CLASS_IN) && DNS_Record_Standard->TTL > 0)
		{
		//AAAA Records
			if (DNS_Record_Standard->Type == htons(DNS_RECORD_AAAA) && DNS_Record_Standard->Length == htons(sizeof(in6_addr)))
			{
			//Records Type in responses check
				if (Parameter.HeaderCheck_DNS && DNS_Query->Type == htons(DNS_RECORD_A))
					return EXIT_FAILURE;

			//Check addresses.
				Addr = (in6_addr *)(Buffer + DataLength);
				if ((Parameter.DataCheck_Blacklist && CheckSpecialAddress(Addr, AF_INET6, false, DomainString)) || 
					(Index < ntohs(DNS_Header->Answer) && !Parameter.LocalHosts && Parameter.LocalRouting && 
					ResponseType == REQUEST_PROCESS_LOCAL && !CheckAddressRouting(Addr, AF_INET6)))
						return EXIT_FAILURE;

				IsGotAddressResult = true;
			}
		//A Records
			else if (DNS_Record_Standard->Type == htons(DNS_RECORD_A) && DNS_Record_Standard->Length == htons(sizeof(in_addr)))
			{
			//Records Type in responses check
				if (Parameter.HeaderCheck_DNS && DNS_Query->Type == htons(DNS_RECORD_AAAA))
					return EXIT_FAILURE;

			//Check addresses.
				Addr = (in_addr *)(Buffer + DataLength);
				if ((Parameter.DataCheck_Blacklist && CheckSpecialAddress(Addr, AF_INET, false, DomainString)) || 
					(Index < ntohs(DNS_Header->Answer) && !Parameter.LocalHosts && Parameter.LocalRouting && 
					ResponseType == REQUEST_PROCESS_LOCAL && !CheckAddressRouting(Addr, AF_INET)))
						return EXIT_FAILURE;

				IsGotAddressResult = true;
			}
		}

	//Mark Resource Records type.
		if (ResponseType != REQUEST_PROCESS_DNSCURVE_SIGN && Parameter.EDNS_Label && Parameter.DNSSEC_Request && Parameter.DNSSEC_Validation)
			BeforeType = DNS_Record_Standard->Type;

		DataLength += ntohs(DNS_Record_Standard->Length);
	}

//Additional EDNS Label Resource Records check, DNSSEC Validation check and Local request result check
	if (ResponseType != REQUEST_PROCESS_DNSCURVE_SIGN && 
		((Parameter.EDNS_Label && 
		(ResponseType == 0 || //Normal
		(ResponseType == REQUEST_PROCESS_LOCAL && Parameter.EDNS_Switch.EDNS_Local) || //Local
		(ResponseType == REQUEST_PROCESS_SOCKS && Parameter.EDNS_Switch.EDNS_SOCKS) || //SOCKS Proxy
		(ResponseType == REQUEST_PROCESS_HTTP && Parameter.EDNS_Switch.EDNS_HTTP) || //HTTP Proxy
		(ResponseType == REQUEST_PROCESS_DIRECT && Parameter.EDNS_Switch.EDNS_Direct) || //Direct Request
		(ResponseType == REQUEST_PROCESS_DNSCURVE && Parameter.EDNS_Switch.EDNS_DNSCurve) || //DNSCurve
		(ResponseType == REQUEST_PROCESS_TCP && Parameter.EDNS_Switch.EDNS_TCP) || //TCP
		(ResponseType == REQUEST_PROCESS_UDP && Parameter.EDNS_Switch.EDNS_UDP)) && //UDP
		(!IsEDNS_Label || (Parameter.DNSSEC_Request && Parameter.DNSSEC_ForceValidation && !IsDNSSEC_Records))) || 
		(ResponseType == REQUEST_PROCESS_LOCAL && !IsGotAddressResult)))
			return EXIT_FAILURE;

#if defined(ENABLE_PCAP)
//Mark Hop Limits or TTL.
	if (ResponseType != REQUEST_PROCESS_DNSCURVE_SIGN && 
		((IsMarkHopLimit != nullptr && Parameter.HeaderCheck_DNS && 
//		DNS_Header->Answer != htons(U16_NUM_ONE) || //Some ISP will return fake responses with more than one Answer records.
		(DNS_Header->Answer == 0 || //No any Answer records
		DNS_Header->Authority > 0 || DNS_Header->Additional > 0 || //More than one Authority records and/or Additional records
		(ntohs(DNS_Header->Flags) & DNS_GET_BIT_RCODE) == DNS_RCODE_NXDOMAIN)) || //No Such Name, not standard query response and no error check.
	//Domain Test part
		(Parameter.DomainTest_Data != nullptr && Domain == Parameter.DomainTest_Data && DNS_Header->ID == Parameter.DomainTest_ID)))
			*IsMarkHopLimit = true;
#endif

	return Length;
}

//Check DNSSEC Records
bool __fastcall CheckDNSSECRecords(
	const char *Buffer, 
	const size_t Length, 
	const uint16_t Type, 
	const uint16_t BeforeType)
{
//DS and CDS Records
	if (Type == htons(DNS_RECORD_DS) || Type == htons(DNS_RECORD_CDS))
	{
		auto DNS_Record_DS = (pdns_record_ds)Buffer;

	//Key Tag, Algorithm and Digest Type check
		if (DNS_Record_DS->KeyTag == 0 || 
			DNS_Record_DS->Algorithm == DNSSEC_AlGORITHM_RESERVED_0 || DNS_Record_DS->Algorithm == DNSSEC_AlGORITHM_RESERVED_4 || 
			DNS_Record_DS->Algorithm == DNSSEC_AlGORITHM_RESERVED_9 || DNS_Record_DS->Algorithm == DNSSEC_AlGORITHM_RESERVED_11 || 
			(DNS_Record_DS->Algorithm >= DNSSEC_AlGORITHM_RESERVED_123 && DNS_Record_DS->Algorithm >= DNSSEC_AlGORITHM_RESERVED_251) || 
			DNS_Record_DS->Algorithm == DNSSEC_AlGORITHM_RESERVED_255 || DNS_Record_DS->Type == DNSSEC_DS_TYPE_RESERVED)
				return false;

	//Algorithm length check
		if ((DNS_Record_DS->Type == DNSSEC_DS_TYPE_SHA1 && Length != sizeof(dns_record_ds) + SHA1_LENGTH) || 
			(DNS_Record_DS->Type == DNSSEC_DS_TYPE_SHA256 && Length != sizeof(dns_record_ds) + SHA256_LENGTH) || 
			(DNS_Record_DS->Type == DNSSEC_DS_TYPE_GOST && Length != sizeof(dns_record_ds) + GOST_LENGTH) || 
			(DNS_Record_DS->Type == DNSSEC_DS_TYPE_SHA384 && Length != sizeof(dns_record_ds) + SHA384_LENGTH))
				return false;
	}
//SIG and RRSIG Records
	else if (Type == htons(DNS_RECORD_SIG) || Type == htons(DNS_RECORD_RRSIG))
	{
		auto DNS_Record_RRSIG = (pdns_record_rrsig)Buffer;

	//RRSIG header check
		if (
		//Type Coverded check
			DNS_Record_RRSIG->TypeCovered != BeforeType || 
		//Algorithm check
			DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RESERVED_0 || DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RESERVED_4 || 
			DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RESERVED_9 || DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RESERVED_11 || 
			(DNS_Record_RRSIG->Algorithm >= DNSSEC_AlGORITHM_RESERVED_123 && DNS_Record_RRSIG->Algorithm >= DNSSEC_AlGORITHM_RESERVED_251) || 
			DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RESERVED_255 || 
		//Labels, Original TTL and Key Tag check
			DNS_Record_RRSIG->Labels == 0 || DNS_Record_RRSIG->TTL == 0 || DNS_Record_RRSIG->KeyTag == 0 || 
		//Signature available time check
			time(nullptr) < (time_t)ntohl(DNS_Record_RRSIG->Inception) || time(nullptr) > (time_t)ntohl(DNS_Record_RRSIG->Expiration))
				return false;

	//Algorithm length check
		if (
		//The Signature length must longer than 512 bits/64 bytes in RSA suite.
			((DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RSA_MD5 || DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RSA_SHA1 || 
			DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RSA_SHA1_NSEC3_SHA1 || DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RSA_SHA256 || 
			DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_RSA_SHA512) && Length <= sizeof(dns_record_rrsig) + RSA_MIN_LENGTH) || 
		//The Signature length must longer than 768 bits/96 bytes in Diffie-Hellman suite.
			(DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_DH && Length <= sizeof(dns_record_rrsig) + DH_MIN_LENGTH) || 
		//The Signature length must longer than 1024 bits/128 bytes in DSA suite.
			((DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_DSA || DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_DSA_NSEC3_SHA1) && 
			Length <= sizeof(dns_record_rrsig) + DSA_MIN_LENGTH) || 
		//The Signature length must longer than 192 bits/24 bytes in ECC suite.
			((DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_ECC_GOST || DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_ECDSA_P256_SHA256 || 
			DNS_Record_RRSIG->Algorithm == DNSSEC_AlGORITHM_ECDSA_P386_SHA386) && Length <= sizeof(dns_record_rrsig) + ECC_MIN_LENGTH))
				return false;
	}
//DNSKEY and CDNSKEY Records
	else if (Type == htons(DNS_RECORD_DNSKEY) || Type == htons(DNS_RECORD_CDNSKEY))
	{
		auto DNS_Record_DNSKEY = (pdns_record_dnskey)Buffer;

	//Key Revoked bit, Protocol and Algorithm check
		if ((ntohs(DNS_Record_DNSKEY->Flags) & DNSSEC_DNSKEY_FLAGS_RSV) > 0 || DNS_Record_DNSKEY->Protocol != DNSSEC_DNSKEY_PROTOCOL || 
			DNS_Record_DNSKEY->Algorithm == DNSSEC_AlGORITHM_RESERVED_0 || DNS_Record_DNSKEY->Algorithm == DNSSEC_AlGORITHM_RESERVED_4 || 
			DNS_Record_DNSKEY->Algorithm == DNSSEC_AlGORITHM_RESERVED_9 || DNS_Record_DNSKEY->Algorithm == DNSSEC_AlGORITHM_RESERVED_11 || 
			(DNS_Record_DNSKEY->Algorithm >= DNSSEC_AlGORITHM_RESERVED_123 && DNS_Record_DNSKEY->Algorithm >= DNSSEC_AlGORITHM_RESERVED_251) || 
			DNS_Record_DNSKEY->Algorithm == DNSSEC_AlGORITHM_RESERVED_255)
				return false;
	}
//NSEC3 Records
	else if (Type == htons(DNS_RECORD_NSEC3))
	{
		auto DNS_Record_NSEC3 = (pdns_record_nsec3)Buffer;

	//Algorithm check
		if (DNS_Record_NSEC3->Algorithm == DNSSEC_AlGORITHM_RESERVED_0 || DNS_Record_NSEC3->Algorithm == DNSSEC_AlGORITHM_RESERVED_4 || 
			DNS_Record_NSEC3->Algorithm == DNSSEC_AlGORITHM_RESERVED_9 || DNS_Record_NSEC3->Algorithm == DNSSEC_AlGORITHM_RESERVED_11 || 
			(DNS_Record_NSEC3->Algorithm >= DNSSEC_AlGORITHM_RESERVED_123 && DNS_Record_NSEC3->Algorithm >= DNSSEC_AlGORITHM_RESERVED_251) || 
			DNS_Record_NSEC3->Algorithm == DNSSEC_AlGORITHM_RESERVED_255)
				return false;

	//Hash Length check
		if (sizeof(dns_record_nsec3param) + DNS_Record_NSEC3->SaltLength < Length && DNS_Record_NSEC3->Algorithm == DNSSEC_NSEC3_ALGORITHM_SHA1 && 
			*(uint8_t *)(Buffer + sizeof(dns_record_nsec3param) + DNS_Record_NSEC3->SaltLength) != SHA1_LENGTH)
				return false;
	}
//NSEC3PARAM Records
	else if (Type == htons(DNS_RECORD_NSEC3PARAM))
	{
		auto DNS_Record_NSEC3PARAM = (pdns_record_nsec3param)Buffer;

	//Algorithm check
		if (DNS_Record_NSEC3PARAM->Algorithm == DNSSEC_AlGORITHM_RESERVED_0 || DNS_Record_NSEC3PARAM->Algorithm == DNSSEC_AlGORITHM_RESERVED_4 || 
			DNS_Record_NSEC3PARAM->Algorithm == DNSSEC_AlGORITHM_RESERVED_9 || DNS_Record_NSEC3PARAM->Algorithm == DNSSEC_AlGORITHM_RESERVED_11 || 
			(DNS_Record_NSEC3PARAM->Algorithm >= DNSSEC_AlGORITHM_RESERVED_123 && DNS_Record_NSEC3PARAM->Algorithm >= DNSSEC_AlGORITHM_RESERVED_251) || 
			DNS_Record_NSEC3PARAM->Algorithm == DNSSEC_AlGORITHM_RESERVED_255)
				return false;
	}

	return true;
}
