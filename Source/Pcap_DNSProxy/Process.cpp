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


#include "Process.h"

//Independent request process
bool __fastcall EnterRequestProcess(
	DNS_PACKET_DATA Packet, 
	const SOCKET_DATA LocalSocketData)
{
//Initialization(Send buffer part)
	std::shared_ptr<char> SendBuffer;
	if (Packet.Protocol == IPPROTO_UDP)
	{
		if (Parameter.CompressionPointerMutation)
		{
			if (Parameter.CPM_PointerToAdditional)
			{
				std::shared_ptr<char> SendBufferTemp(new char[Packet.Length + 2U + sizeof(dns_record_aaaa) + sizeof(uint16_t)]());
				memset(SendBufferTemp.get(), 0, Packet.Length + 2U + sizeof(dns_record_aaaa) + sizeof(uint16_t));
				SendBufferTemp.swap(SendBuffer);
				Packet.BufferSize = Packet.Length + 2U + sizeof(dns_record_aaaa) + sizeof(uint16_t);
			}
			else if (Parameter.CPM_PointerToRR)
			{
				std::shared_ptr<char> SendBufferTemp(new char[Packet.Length + 2U + sizeof(uint16_t)]());
				memset(SendBufferTemp.get(), 0, Packet.Length + 2U + sizeof(uint16_t));
				SendBufferTemp.swap(SendBuffer);
				Packet.BufferSize = Packet.Length + 2U + sizeof(uint16_t);
			}
			else { //Pointer to header
				std::shared_ptr<char> SendBufferTemp(new char[Packet.Length + 1U + sizeof(uint16_t)]());
				memset(SendBufferTemp.get(), 0, Packet.Length + 1U + sizeof(uint16_t));
				SendBufferTemp.swap(SendBuffer);
				Packet.BufferSize = Packet.Length + 1U + sizeof(uint16_t);
			}
		}
		else {
			std::shared_ptr<char> SendBufferTemp(new char[Packet.Length + sizeof(uint16_t)]()); //Reserved 2 bytes for TCP header length.
			memset(SendBufferTemp.get(), 0, Packet.Length + sizeof(uint16_t));
			SendBufferTemp.swap(SendBuffer);
			Packet.BufferSize = Packet.Length + sizeof(uint16_t);
		}

		memcpy_s(SendBuffer.get(), Packet.BufferSize, Packet.Buffer, Packet.Length);
		Packet.Buffer = SendBuffer.get();
	}

//Initialization(Receive buffer part)
	std::shared_ptr<char> RecvBuffer;
	size_t RecvSize = 0;
	if (Parameter.RequestMode_Transport == REQUEST_MODE_TCP || Packet.Protocol == IPPROTO_TCP || //TCP request
		Parameter.LocalProtocol_Transport == REQUEST_MODE_TCP || //Local request
		(Parameter.SOCKS_Proxy && Parameter.SOCKS_Protocol_Transport == REQUEST_MODE_TCP) || //SOCKS TCP request
		Parameter.HTTP_Proxy //HTTP Proxy request
	#if defined(ENABLE_LIBSODIUM)
		|| (Parameter.DNSCurve && DNSCurveParameter.DNSCurveProtocol_Transport == REQUEST_MODE_TCP) //DNSCurve TCP request
	#endif
		) //TCP
	{
		std::shared_ptr<char> TCPRecvBuffer(new char[LARGE_PACKET_MAXSIZE + sizeof(uint16_t)]());
		memset(TCPRecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE + sizeof(uint16_t));
		RecvBuffer.swap(TCPRecvBuffer);
		RecvSize = LARGE_PACKET_MAXSIZE;
	}
	else { //UDP
		std::shared_ptr<char> UDPRecvBuffer(new char[PACKET_MAXSIZE + sizeof(uint16_t)]());
		memset(UDPRecvBuffer.get(), 0, PACKET_MAXSIZE + sizeof(uint16_t));
		RecvBuffer.swap(UDPRecvBuffer);
		RecvSize = PACKET_MAXSIZE;
	}

//Local request process
	if (Packet.IsLocal)
	{
		auto Result = LocalRequestProcess(Packet, RecvBuffer.get(), RecvSize, LocalSocketData);
		if (Result || Parameter.LocalForce)
		{
		//Fin TCP request connection.
			if (Packet.Protocol == IPPROTO_TCP && SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_INVALID_CHECK, false, nullptr))
			{
				shutdown(LocalSocketData.Socket, SD_BOTH);
				closesocket(LocalSocketData.Socket);
			}

			return Result;
		}
	}

//Compression Pointer Mutation
	if (Parameter.CompressionPointerMutation && ((pdns_hdr)Packet.Buffer)->Additional == 0)
	{
		auto DataLength = MakeCompressionPointerMutation(Packet.Buffer, Packet.Length);
		if (DataLength > Packet.Length)
			Packet.Length = DataLength;
	}

//SOCKS proxy request process
	if (Parameter.SOCKS_Proxy)
	{
	//SOCKS request
		if (SOCKSRequestProcess(Packet, RecvBuffer.get(), RecvSize, LocalSocketData))
			return true;

	//SOCKS Proxy Only mode
		if (Parameter.SOCKS_Only)
		{
		//Fin TCP request connection.
			if (Packet.Protocol == IPPROTO_TCP && SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_INVALID_CHECK, false, nullptr))
			{
				shutdown(LocalSocketData.Socket, SD_BOTH);
				closesocket(LocalSocketData.Socket);
			}

			return true;
		}
	}

//HTTP proxy request process
	if (Parameter.HTTP_Proxy)
	{
	//HTTP request
		if (HTTPRequestProcess(Packet, RecvBuffer.get(), RecvSize, LocalSocketData))
			return true;

	//HTTP Proxy Only mode
		if (Parameter.HTTP_Only)
		{
		//Fin TCP request connection.
			if (Packet.Protocol == IPPROTO_TCP && SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_INVALID_CHECK, false, nullptr))
			{
				shutdown(LocalSocketData.Socket, SD_BOTH);
				closesocket(LocalSocketData.Socket);
			}

			return true;
		}
	}

//Direct Request request process
	if (Parameter.DirectRequest > DIRECT_REQUEST_MODE_NONE && DirectRequestProcess(Packet, RecvBuffer.get(), RecvSize, true, LocalSocketData))
	{
	//Fin TCP request connection.
		if (Packet.Protocol == IPPROTO_TCP && SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_INVALID_CHECK, false, nullptr))
		{
			shutdown(LocalSocketData.Socket, SD_BOTH);
			closesocket(LocalSocketData.Socket);
		}

		return true;
	}

//DNSCurve request process
#if defined(ENABLE_LIBSODIUM)
	if (Parameter.DNSCurve)
	{
	//DNSCurve check
		if (DNSCurveParameter.IsEncryption && Packet.Length + DNSCRYPT_BUFFER_RESERVE_LEN > DNSCurveParameter.DNSCurvePayloadSize)
			goto SkipDNSCurve;

	//DNSCurve request
		if (DNSCurveRequestProcess(Packet, RecvBuffer.get(), RecvSize, LocalSocketData))
			return true;

	//DNSCurve Encryption Only mode
		if (DNSCurveParameter.IsEncryptionOnly)
		{
		//Fin TCP request connection.
			if (Packet.Protocol == IPPROTO_TCP && SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_INVALID_CHECK, false, nullptr))
			{
				shutdown(LocalSocketData.Socket, SD_BOTH);
				closesocket(LocalSocketData.Socket);
			}

			return true;
		}
	}
	
//Jump here to skip DNSCurve process.
SkipDNSCurve:
#endif

//TCP request process
	if ((Parameter.RequestMode_Transport == REQUEST_MODE_TCP || Packet.Protocol == IPPROTO_TCP) && 
		TCPRequestProcess(Packet, RecvBuffer.get(), RecvSize, LocalSocketData))
			return true;

//Direct request when Pcap Capture module is not available.
#if defined(ENABLE_PCAP)
	if (!Parameter.PcapCapture)
	{
#endif
		DirectRequestProcess(Packet, RecvBuffer.get(), RecvSize, false, LocalSocketData);

	//Fin TCP request connection.
		if (Packet.Protocol == IPPROTO_TCP && SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_INVALID_CHECK, false, nullptr))
		{
			shutdown(LocalSocketData.Socket, SD_BOTH);
			closesocket(LocalSocketData.Socket);
		}

		return true;
#if defined(ENABLE_PCAP)
	}

//UDP request
	RecvBuffer.reset();
	UDPRequestProcess(Packet, LocalSocketData);

	return true;
#endif
}

//Check white and banned hosts list
size_t __fastcall CheckWhiteBannedHostsProcess(
	const size_t Length, 
	const HostsTable &HostsTableIter, 
	dns_hdr *DNS_Header, 
	dns_qry *DNS_Query, 
	bool *IsLocal)
{
//Whitelist Hosts
	if (HostsTableIter.PermissionType == HOSTS_TYPE_WHITE)
	{
	//Reset IsLocal flag.
		if (IsLocal != nullptr)
			*IsLocal = false;

	//Ignore all types.
		if (HostsTableIter.RecordTypeList.empty())
		{
			return EXIT_FAILURE;
		}
		else {
		//Permit or Deny check
			if (HostsTableIter.PermissionOperation)
			{
			//Only ignore some types.
				for (auto RecordTypeIter = HostsTableIter.RecordTypeList.begin();RecordTypeIter != HostsTableIter.RecordTypeList.end();++RecordTypeIter)
				{
					if (DNS_Query->Type == *RecordTypeIter)
						break;
					else if (RecordTypeIter + 1U == HostsTableIter.RecordTypeList.end())
						return EXIT_FAILURE;
				}
			}
		//Ignore some types.
			else {
				for (auto RecordTypeIter:HostsTableIter.RecordTypeList)
				{
					if (DNS_Query->Type == RecordTypeIter)
						return EXIT_FAILURE;
				}
			}
		}
	}
//Banned Hosts
	else if (HostsTableIter.PermissionType == HOSTS_TYPE_BANNED)
	{
	//Reset IsLocal flag.
		if (IsLocal != nullptr)
			*IsLocal = false;

	//Block all types.
		if (HostsTableIter.RecordTypeList.empty())
		{
//			DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R_SNH);
			DNS_Header->Flags = htons(DNS_SET_R_SNH);
			return Length;
		}
		else {
		//Permit or Deny check
			if (HostsTableIter.PermissionOperation)
			{
			//Only some types are allowed.
				for (auto RecordTypeIter = HostsTableIter.RecordTypeList.begin();RecordTypeIter != HostsTableIter.RecordTypeList.end();++RecordTypeIter)
				{
					if (DNS_Query->Type == *RecordTypeIter)
					{
						break;
					}
					else if (RecordTypeIter + 1U == HostsTableIter.RecordTypeList.end())
					{
//						DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R);
						DNS_Header->Flags = htons(DNS_SQR_NE);
						return Length;
					}
				}
			}
		//Block some types.
			else {
				for (auto RecordTypeIter:HostsTableIter.RecordTypeList)
				{
					if (DNS_Query->Type == RecordTypeIter)
					{
//						DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R);
						DNS_Header->Flags = htons(DNS_SQR_NE);
						return Length;
					}
				}
			}
		}
	}

	return EXIT_SUCCESS;
}

//Check hosts from global list
size_t __fastcall CheckHostsProcess(
	DNS_PACKET_DATA *Packet, 
	char *Result, 
	const size_t ResultSize, 
	const SOCKET_DATA &LocalSocketData)
{
//Initilization
	std::string Domain;
	size_t DataLength = 0;
	auto DNS_Header = (pdns_hdr)Packet->Buffer;

//Request check
	if (DNS_Header->Question == htons(U16_NUM_ONE) && CheckQueryNameLength(Packet->Buffer + sizeof(dns_hdr)) + 1U < DOMAIN_MAXSIZE)
	{
		if (DNSQueryToChar(Packet->Buffer + sizeof(dns_hdr), Domain) <= DOMAIN_MINSIZE)
			return EXIT_SUCCESS;
		else 
			CaseConvert(false, Domain);
	}
	else {
		return EXIT_FAILURE;
	}

//Response setting
	memset(Result, 0, ResultSize);
	memcpy_s(Result, ResultSize, Packet->Buffer, Packet->Length);
	DNS_Header = (pdns_hdr)Result;
	auto DNS_Query = (pdns_qry)(Result + DNS_PACKET_QUERY_LOCATE(Result));

//Check Classes.
	if (DNS_Query->Classes != htons(DNS_CLASS_IN))
		return EXIT_FAILURE;

//Check Accept Types list.
	if (Parameter.AcceptTypeList != nullptr)
	{
	//Permit
		if (Parameter.AcceptType)
		{
			for (auto AcceptTypeTableIter = Parameter.AcceptTypeList->begin();AcceptTypeTableIter != Parameter.AcceptTypeList->end();++AcceptTypeTableIter)
			{
				if (AcceptTypeTableIter + 1U == Parameter.AcceptTypeList->end())
				{
					if (*AcceptTypeTableIter != DNS_Query->Type)
					{
//						DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R_SNH);
						DNS_Header->Flags = htons(DNS_SET_R_SNH);
						return Packet->Length;
					}
				}
				else if (*AcceptTypeTableIter == DNS_Query->Type)
				{
					break;
				}
			}
		}
	//Deny
		else {
			for (auto AcceptTypeTableIter:*Parameter.AcceptTypeList)
			{
				if (DNS_Query->Type == AcceptTypeTableIter)
				{
//					DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R_SNH);
					DNS_Header->Flags = htons(DNS_SET_R_SNH);
					return Packet->Length;
				}
			}
		}
	}

//PTR Records
//LLMNR protocol of Mac OS X powered by mDNS with PTR records
#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
	if (DNS_Query->Type == htons(DNS_RECORD_PTR) && Parameter.LocalServer_Length + Packet->Length <= ResultSize)
	{
		auto IsSendPTR = false;

	//IPv6 check
		if (Domain == ("1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa") || //Loopback address(::1, Section 2.5.3 in RFC 4291)
	//IPv4 check
			Domain.find(".127.in-addr.arpa") != std::string::npos || //Loopback address(127.0.0.0/8, Section 3.2.1.3 in RFC 1122)
			Domain.find(".254.169.in-addr.arpa") != std::string::npos) //Link-local address(169.254.0.0/16, RFC 3927)
		{
			IsSendPTR = true;
		}
		else {
		//IPv6 check
			std::unique_lock<std::mutex> LocalAddressMutexIPv6(LocalAddressLock[0]);
			for (auto StringIter:*GlobalRunningStatus.LocalAddress_ResponsePTR[0])
			{
				if (Domain == StringIter)
				{
					IsSendPTR = true;
					break;
				}
			}
			LocalAddressMutexIPv6.unlock();

		//IPv4 check
			if (!IsSendPTR)
			{
				std::lock_guard<std::mutex> LocalAddressMutexIPv4(LocalAddressLock[1U]);
				for (auto StringIter:*GlobalRunningStatus.LocalAddress_ResponsePTR[1U])
				{
					if (Domain == StringIter)
					{
						IsSendPTR = true;
						break;
					}
				}
			}
		}

	//Send Localhost PTR.
		if (IsSendPTR)
		{
		//Set header flags and copy response to buffer.
			DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SER_RA);
			DNS_Header->Answer = htons(U16_NUM_ONE);
			DNS_Header->Authority = 0;
			DNS_Header->Additional = 0;
			memset(Result + sizeof(dns_hdr) + Packet->Question, 0, Packet->Length - (sizeof(dns_hdr) + Packet->Question));
			memcpy_s(Result + sizeof(dns_hdr) + Packet->Question, ResultSize - (sizeof(dns_hdr) + Packet->Question), Parameter.LocalServer_Response, Parameter.LocalServer_Length);
			DataLength = sizeof(dns_hdr) + Packet->Question + Parameter.LocalServer_Length;

		//EDNS Label
			if (Parameter.EDNS_Label || Packet->EDNS_Record > 0)
				DataLength = AddEDNSLabelToAdditionalRR(Result, DataLength, ResultSize, nullptr);
			
			return DataLength;
		}
	}
#endif

//LocalFQDN check
	if (Parameter.LocalFQDN_String != nullptr && Domain == *Parameter.LocalFQDN_String)
	{
	//IPv6
		if (DNS_Query->Type == htons(DNS_RECORD_AAAA))
		{
			std::lock_guard<std::mutex> LocalAddressMutexIPv6(LocalAddressLock[0]);
			if (GlobalRunningStatus.LocalAddress_Length[0] >= DNS_PACKET_MINSIZE)
			{
				memset(Result + sizeof(uint16_t), 0, ResultSize - sizeof(uint16_t));
				memcpy_s(Result + sizeof(uint16_t), ResultSize - sizeof(uint16_t), GlobalRunningStatus.LocalAddress_Response[0] + sizeof(uint16_t), GlobalRunningStatus.LocalAddress_Length[0] - sizeof(uint16_t));
				return GlobalRunningStatus.LocalAddress_Length[0];
			}
		}
	//IPv4
		else if (DNS_Query->Type == htons(DNS_RECORD_A))
		{
			std::lock_guard<std::mutex> LocalAddressMutexIPv4(LocalAddressLock[1U]);
			if (GlobalRunningStatus.LocalAddress_Length[1U] >= DNS_PACKET_MINSIZE)
			{
				memset(Result + sizeof(uint16_t), 0, ResultSize - sizeof(uint16_t));
				memcpy_s(Result + sizeof(uint16_t), ResultSize - sizeof(uint16_t), GlobalRunningStatus.LocalAddress_Response[1U] + sizeof(uint16_t), GlobalRunningStatus.LocalAddress_Length[1U] - sizeof(uint16_t));
				return GlobalRunningStatus.LocalAddress_Length[1U];
			}
		}
	}

//Local Main parameter check
	if (Parameter.LocalMain)
		Packet->IsLocal = true;

//Normal Hosts check
	std::unique_lock<std::mutex> HostsFileMutex(HostsFileLock);
	for (auto HostsFileSetIter:*HostsFileSetUsing)
	{
		for (auto HostsTableIter:HostsFileSetIter.HostsList_Normal)
		{
			if (std::regex_match(Domain, HostsTableIter.Pattern))
			{
			//Source Hosts check
				if (!HostsTableIter.SourceList.empty())
				{
					for (auto SourceListIter:HostsTableIter.SourceList)
					{
						if (LocalSocketData.SockAddr.ss_family == AF_INET6 && SourceListIter.Address.ss_family == AF_INET6) //IPv6
						{
							if (SourceListIter.Prefix < sizeof(in6_addr) * BYTES_TO_BITS / 2U)
							{
								auto AddressPart = hton64(ntoh64(*(PUINT64)&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr) & (UINT64_MAX << (sizeof(in6_addr) * BYTES_TO_BITS / 2U - SourceListIter.Prefix)));
								if (memcmp(&AddressPart, &((sockaddr_in6 *)&SourceListIter.Address)->sin6_addr, sizeof(uint64_t)) == 0)
									goto JumpToContinue;
							}
							else {
								auto AddressPart = hton64(ntoh64(*(PUINT64)((uint8_t *)&((PSOCKADDR_IN6)&LocalSocketData.SockAddr)->sin6_addr + sizeof(in6_addr) / 2U)) & (UINT64_MAX << (sizeof(in6_addr) * BYTES_TO_BITS / 2U - SourceListIter.Prefix)));
								if (memcmp(&AddressPart, (uint8_t *)&((sockaddr_in6 *)&SourceListIter.Address)->sin6_addr + sizeof(in6_addr) / 2U, sizeof(uint64_t)) == 0)
									goto JumpToContinue;
							}
						}
						else if (LocalSocketData.SockAddr.ss_family == AF_INET && SourceListIter.Address.ss_family == AF_INET && //IPv4
							htonl(ntohl(((sockaddr_in *)&LocalSocketData.SockAddr)->sin_addr.s_addr) & (UINT32_MAX << (sizeof(in_addr) * BYTES_TO_BITS - SourceListIter.Prefix))) == 
							((sockaddr_in *)&SourceListIter.Address)->sin_addr.s_addr)
						{
							goto JumpToContinue;
						}
					}

					continue;
				}

			//Jump here to continue.
			JumpToContinue:

			//Check white and banned hosts list, empty record type list check
				DataLength = CheckWhiteBannedHostsProcess(Packet->Length, HostsTableIter, DNS_Header, DNS_Query, &Packet->IsLocal);
				if (DataLength >= DNS_PACKET_MINSIZE)
					return DataLength;
				else if (DataLength == EXIT_FAILURE)
					goto StopLoop_NormalHosts;

			//Initialization
				void *DNS_Record = nullptr;
				size_t RamdomIndex = 0, Index = 0;

			//IPv6(AAAA records)
				if (DNS_Query->Type == htons(DNS_RECORD_AAAA) && HostsTableIter.RecordTypeList.front() == htons(DNS_RECORD_AAAA))
				{
				//Set header flags and convert DNS query to DNS response packet.
//					DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R);
					DNS_Header->Flags = htons(DNS_SQR_NE);
					DataLength = sizeof(dns_hdr) + Packet->Question;
					memset(Result + DataLength, 0, ResultSize - DataLength);

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
						DNS_Record = (pdns_record_aaaa)(Result + DataLength);
						DataLength += sizeof(dns_record_aaaa);
						((pdns_record_aaaa)DNS_Record)->Name = htons(DNS_POINTER_QUERY);
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
						if (((Parameter.EDNS_Label || Packet->EDNS_Record > 0) && DataLength + sizeof(dns_record_aaaa) + EDNS_ADDITIONAL_MAXSIZE >= ResultSize) || //EDNS Label
							DataLength + sizeof(dns_record_aaaa) >= ResultSize) //Normal query
						{
							++Index;
							break;
						}
					}

				//Set DNS counts and EDNS Label
					DNS_Header->Answer = htons((uint16_t)Index);
					DNS_Header->Authority = 0;
					DNS_Header->Additional = 0;
					if (Parameter.EDNS_Label || Packet->EDNS_Record > 0)
						DataLength = AddEDNSLabelToAdditionalRR(Result, DataLength, ResultSize, nullptr);

					return DataLength;
				}
			//IPv4(A records)
				else if (DNS_Query->Type == htons(DNS_RECORD_A) && HostsTableIter.RecordTypeList.front() == htons(DNS_RECORD_A))
				{
				//Set header flags and convert DNS query to DNS response packet.
//					DNS_Header->Flags = htons(ntohs(DNS_Header->Flags) | DNS_SET_R);
					DNS_Header->Flags = htons(DNS_SQR_NE);
					DataLength = sizeof(dns_hdr) + Packet->Question;
					memset(Result + DataLength, 0, ResultSize - DataLength);

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
						DNS_Record = (pdns_record_a)(Result + DataLength);
						DataLength += sizeof(dns_record_a);
						((pdns_record_a)DNS_Record)->Name = htons(DNS_POINTER_QUERY);
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
						if (((Parameter.EDNS_Label || Packet->EDNS_Record > 0) && DataLength + sizeof(dns_record_a) + EDNS_ADDITIONAL_MAXSIZE >= ResultSize) || //EDNS Label
							DataLength + sizeof(dns_record_a) >= ResultSize) //Normal query
						{
							++Index;
							break;
						}
					}

				//Set DNS counts and EDNS Label
					DNS_Header->Answer = htons((uint16_t)Index);
					DNS_Header->Authority = 0;
					DNS_Header->Additional = 0;
					if (Parameter.EDNS_Label || Packet->EDNS_Record > 0)
						DataLength = AddEDNSLabelToAdditionalRR(Result, DataLength, ResultSize, nullptr);

					return DataLength;
				}
			}
		}
	}

//Jump here to stop loop.
StopLoop_NormalHosts:
	HostsFileMutex.unlock();

//Check DNS cache.
	std::unique_lock<std::mutex> DNSCacheListMutex(DNSCacheListLock);
	if (Parameter.CacheType == CACHE_TYPE_TIMER) //Delete DNS cache.
	{
	#if defined(PLATFORM_WIN_XP)
/* Old version(2016-05-29)
		while ((!DNSCacheList.empty() && (GlobalRunningStatus.FunctionPTR_GetTickCount64 != nullptr && 
			(*GlobalRunningStatus.FunctionPTR_GetTickCount64)() >= DNSCacheList.back().ClearCacheTime) ||
			GetTickCount() >= DNSCacheList.back().ClearCacheTime))
*/
		while (!DNSCacheList.empty() && GetTickCount() >= DNSCacheList.back().ClearCacheTime)
	#else
		while (!DNSCacheList.empty() && GetTickCount64() >= DNSCacheList.back().ClearCacheTime)
	#endif
			DNSCacheList.pop_back();
	}
	for (auto DNSCacheDataIter:DNSCacheList) //Scan all DNS cache.
	{
		if (Domain == DNSCacheDataIter.Domain && DNS_Query->Type == DNSCacheDataIter.RecordType)
		{
			memset(Result + sizeof(uint16_t), 0, ResultSize - sizeof(uint16_t));
			memcpy_s(Result + sizeof(uint16_t), ResultSize - sizeof(uint16_t), DNSCacheDataIter.Response.get(), DNSCacheDataIter.Length);

			return DNSCacheDataIter.Length + sizeof(uint16_t);
		}
	}
	DNSCacheListMutex.unlock();

//Local Hosts check
	HostsFileMutex.lock();
	for (auto HostsFileSetIter:*HostsFileSetUsing)
	{
		for (auto HostsTableIter:HostsFileSetIter.HostsList_Local)
		{
			if (std::regex_match(Domain, HostsTableIter.Pattern))
			{
				DataLength = CheckWhiteBannedHostsProcess(Packet->Length, HostsTableIter, DNS_Header, DNS_Query, &Packet->IsLocal); //Check white and banned hosts list.
				if (DataLength >= DNS_PACKET_MINSIZE)
					return DataLength;
				else if (DataLength == EXIT_FAILURE)
					Packet->IsLocal = false;
				else //IsLocal flag setting
					Packet->IsLocal = true;

				goto StopLoop_LocalHosts;
			}
		}
	}

//Jump here to stop loop.
StopLoop_LocalHosts:
	HostsFileMutex.unlock();

//Domain Case Conversion
	if (Parameter.DomainCaseConversion)
		MakeDomainCaseConversion(Packet->Buffer + sizeof(dns_hdr));

	return EXIT_SUCCESS;
}

//Request Process(Local part)
bool __fastcall LocalRequestProcess(
	const DNS_PACKET_DATA &Packet, 
	char *OriginalRecv, 
	const size_t RecvSize, 
	const SOCKET_DATA &LocalSocketData)
{
	size_t DataLength = 0;
	memset(OriginalRecv, 0, RecvSize);

//EDNS switching(Part 1)
	size_t EDNS_SwitchLength = Packet.Length;
	uint16_t EDNS_Packet_Flags = ((dns_hdr *)(Packet.Buffer))->Flags;
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_Local)
	{
	//Reset EDNS flags, resource record counts and packet length.
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_AD));
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_CD));
		if (((dns_hdr *)(Packet.Buffer))->Additional > 0)
			((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) - 1U);
		EDNS_SwitchLength -= Packet.EDNS_Record;
	}

//TCP request
	if (Parameter.LocalProtocol_Transport == REQUEST_MODE_TCP || Packet.Protocol == IPPROTO_TCP)
	{
		DataLength = TCPRequest(REQUEST_PROCESS_LOCAL, Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);

	//Send response.
		if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
		{
			SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
			return true;
		}
	}

//UDP request and Send response.
	DataLength = UDPCompleteRequest(REQUEST_PROCESS_LOCAL, Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);
	if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
	{
		SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
		return true;
	}

//EDNS switching(Part 2)
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_Local)
	{
		((dns_hdr *)(Packet.Buffer))->Flags = EDNS_Packet_Flags;
		((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) + 1U);
	}

	return false;
}

//Request Process(SOCKS part)
bool __fastcall SOCKSRequestProcess(
	const DNS_PACKET_DATA &Packet, 
	char *OriginalRecv, 
	const size_t RecvSize, 
	const SOCKET_DATA &LocalSocketData)
{
	size_t DataLength = 0;
	memset(OriginalRecv, 0, RecvSize);

//EDNS switching(Part 1)
	size_t EDNS_SwitchLength = Packet.Length;
	uint16_t EDNS_Packet_Flags = ((dns_hdr *)(Packet.Buffer))->Flags;
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_SOCKS)
	{
	//Reset EDNS flags, resource record counts and packet length.
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_AD));
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_CD));
		if (((dns_hdr *)(Packet.Buffer))->Additional > 0)
			((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) - 1U);
		EDNS_SwitchLength -= Packet.EDNS_Record;
	}

//UDP request
	if (Parameter.SOCKS_Version == SOCKS_VERSION_5 && Parameter.SOCKS_Protocol_Transport == REQUEST_MODE_UDP)
	{
	//UDP request process
		DataLength = SOCKSUDPRequest(Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);
		
	//Send response.
		if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
		{
			SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
			return true;
		}
	}

//TCP request
	DataLength = SOCKSTCPRequest(Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);

//Send response.
	if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
	{
		SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
		return true;
	}

//EDNS switching(Part 2)
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_SOCKS)
	{
		((dns_hdr *)(Packet.Buffer))->Flags = EDNS_Packet_Flags;
		((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) + 1U);
	}

	return false;
}

//Request Process(HTTP part)
bool __fastcall HTTPRequestProcess(
	const DNS_PACKET_DATA &Packet, 
	char *OriginalRecv, 
	const size_t RecvSize, 
	const SOCKET_DATA &LocalSocketData)
{
	size_t DataLength = 0;
	memset(OriginalRecv, 0, RecvSize);

//EDNS switching(Part 1)
	size_t EDNS_SwitchLength = Packet.Length;
	uint16_t EDNS_Packet_Flags = ((dns_hdr *)(Packet.Buffer))->Flags;
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_HTTP)
	{
	//Reset EDNS flags, resource record counts and packet length.
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_AD));
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_CD));
		if (((dns_hdr *)(Packet.Buffer))->Additional > 0)
			((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) - 1U);
		EDNS_SwitchLength -= Packet.EDNS_Record;
	}

//HTTP request
	DataLength = HTTPRequest(Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);

//Send response.
	if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
	{
		SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
		return true;
	}

//EDNS switching(Part 2)
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_HTTP)
	{
		((dns_hdr *)(Packet.Buffer))->Flags = EDNS_Packet_Flags;
		((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) + 1U);
	}

	return false;
}

//Request Process(Direct connections part)
bool __fastcall DirectRequestProcess(
	const DNS_PACKET_DATA &Packet, 
	char *OriginalRecv, 
	const size_t RecvSize, 
	const bool DirectRequest, 
	const SOCKET_DATA &LocalSocketData)
{
	size_t DataLength = 0;
	memset(OriginalRecv, 0, RecvSize);

//Direct Request mode check
	DataLength = SelectNetworkProtocol();
	if (DirectRequest && ((DataLength == AF_INET6 && Parameter.DirectRequest == DIRECT_REQUEST_MODE_IPV4) || //IPv6
		(DataLength == AF_INET && Parameter.DirectRequest == DIRECT_REQUEST_MODE_IPV6))) //IPv4
			return false;

//EDNS switching(Part 1)
	size_t EDNS_SwitchLength = Packet.Length;
	uint16_t EDNS_Packet_Flags = ((dns_hdr *)(Packet.Buffer))->Flags;
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_Direct)
	{
	//Reset EDNS flags, resource record counts and packet length.
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_AD));
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_CD));
		if (((dns_hdr *)(Packet.Buffer))->Additional > 0)
			((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) - 1U);
		EDNS_SwitchLength -= Packet.EDNS_Record;
	}

//TCP request
	if (Parameter.RequestMode_Transport == REQUEST_MODE_TCP || Packet.Protocol == IPPROTO_TCP)
	{
	//Multi request process
		if (Parameter.AlternateMultiRequest || Parameter.MultiRequestTimes > 1U)
			DataLength = TCPRequestMulti(REQUEST_PROCESS_DIRECT, Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);
	//Normal request process
		else 
			DataLength = TCPRequest(REQUEST_PROCESS_DIRECT, Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);

	//Send response.
		if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
		{
			SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
			return true;
		}
	}

//UDP request
	if (Parameter.AlternateMultiRequest || Parameter.MultiRequestTimes > 1U) //Multi request process
		DataLength = UDPCompleteRequestMulti(REQUEST_PROCESS_DIRECT, Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);
	else //Normal request process
		DataLength = UDPCompleteRequest(REQUEST_PROCESS_DIRECT, Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);

//Send response.
	if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
	{
		SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
		return true;
	}

//EDNS switching(Part 2)
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_Direct)
	{
		((dns_hdr *)(Packet.Buffer))->Flags = EDNS_Packet_Flags;
		((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) + 1U);
	}

	return false;
}

//Request Process(DNSCurve part)
#if defined(ENABLE_LIBSODIUM)
bool __fastcall DNSCurveRequestProcess(
	const DNS_PACKET_DATA &Packet, 
	char *OriginalRecv, 
	const size_t RecvSize, 
	const SOCKET_DATA &LocalSocketData)
{
	size_t DataLength = 0;
	memset(OriginalRecv, 0, RecvSize);

//EDNS switching(Part 1)
	size_t EDNS_SwitchLength = Packet.Length;
	uint16_t EDNS_Packet_Flags = ((dns_hdr *)(Packet.Buffer))->Flags;
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_DNSCurve)
	{
	//Reset EDNS flags, resource record counts and packet length.
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_AD));
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_CD));
		if (((dns_hdr *)(Packet.Buffer))->Additional > 0)
			((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) - 1U);
		EDNS_SwitchLength -= Packet.EDNS_Record;
	}

//TCP request
	if (DNSCurveParameter.DNSCurveProtocol_Transport == REQUEST_MODE_TCP || Packet.Protocol == IPPROTO_TCP)
	{
	//Multi request process
		if (Parameter.AlternateMultiRequest || Parameter.MultiRequestTimes > 1U)
			DataLength = DNSCurveTCPRequestMulti(Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);
	//Normal request process
		else 
			DataLength = DNSCurveTCPRequest(Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);

	//Send response.
		if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
		{
			SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
			return true;
		}
	}

//UDP request
	if (Parameter.AlternateMultiRequest || Parameter.MultiRequestTimes > 1U) //Multi request process
		DataLength = DNSCurveUDPRequestMulti(Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);
	else //Normal request process
		DataLength = DNSCurveUDPRequest(Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);

//Send response.
	if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
	{
		SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
		return true;
	}

//EDNS switching(Part 2)
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_DNSCurve)
	{
		((dns_hdr *)(Packet.Buffer))->Flags = EDNS_Packet_Flags;
		((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) + 1U);
	}

	return false;
}
#endif

//Request Process(TCP part)
bool __fastcall TCPRequestProcess(
	const DNS_PACKET_DATA &Packet, 
	char *OriginalRecv, 
	const size_t RecvSize, 
	const SOCKET_DATA &LocalSocketData)
{
	size_t DataLength = 0;
	memset(OriginalRecv, 0, RecvSize);

//EDNS switching(Part 1)
	size_t EDNS_SwitchLength = Packet.Length;
	uint16_t EDNS_Packet_Flags = ((dns_hdr *)(Packet.Buffer))->Flags;
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_TCP)
	{
	//Reset EDNS flags, resource record counts and packet length.
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_AD));
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_CD));
		if (((dns_hdr *)(Packet.Buffer))->Additional > 0)
			((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) - 1U);
		EDNS_SwitchLength -= Packet.EDNS_Record;
	}

//Multi request process
	if (Parameter.AlternateMultiRequest || Parameter.MultiRequestTimes > 1U)
		DataLength = TCPRequestMulti(REQUEST_PROCESS_TCP, Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);
//Normal request process
	else 
		DataLength = TCPRequest(REQUEST_PROCESS_TCP, Packet.Buffer, EDNS_SwitchLength, OriginalRecv, RecvSize);

//Send response.
	if (DataLength >= DNS_PACKET_MINSIZE && DataLength < RecvSize)
	{
		SendToRequester(OriginalRecv, DataLength, RecvSize, Packet.Protocol, LocalSocketData);
		return true;
	}

//EDNS switching(Part 2)
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_TCP)
	{
		((dns_hdr *)(Packet.Buffer))->Flags = EDNS_Packet_Flags;
		((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) + 1U);
	}

	return false;
}

//Select network layer protocol of packets sending
uint16_t __fastcall SelectNetworkProtocol(
	void)
{
//IPv6
	if (Parameter.DNSTarget.IPv6.AddressData.Storage.ss_family > 0 && 
		((Parameter.RequestMode_Network == REQUEST_MODE_NETWORK_BOTH && GlobalRunningStatus.GatewayAvailable_IPv6) || //Auto select
		Parameter.RequestMode_Network == REQUEST_MODE_IPV6 || //IPv6
		(Parameter.RequestMode_Network == REQUEST_MODE_IPV4 && Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family == 0))) //Non-IPv4
			return AF_INET6;
//IPv4
	else if (Parameter.DNSTarget.IPv4.AddressData.Storage.ss_family > 0 && 
		((Parameter.RequestMode_Network == REQUEST_MODE_NETWORK_BOTH && GlobalRunningStatus.GatewayAvailable_IPv4) || //Auto select
		Parameter.RequestMode_Network == REQUEST_MODE_IPV4 || //IPv4
		(Parameter.RequestMode_Network == REQUEST_MODE_IPV6 && Parameter.DNSTarget.IPv6.AddressData.Storage.ss_family == 0))) //Non-IPv6
			return AF_INET;

	return 0;
}

//Request Process(UDP part)
#if defined(ENABLE_PCAP)
void __fastcall UDPRequestProcess(
	const DNS_PACKET_DATA &Packet, 
	const SOCKET_DATA &LocalSocketData)
{
//EDNS switching(Part 1)
	size_t EDNS_SwitchLength = Packet.Length;
	uint16_t EDNS_Packet_Flags = ((dns_hdr *)(Packet.Buffer))->Flags;
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_UDP)
	{
	//Reset EDNS flags, resource record counts and packet length.
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_AD));
		((dns_hdr *)(Packet.Buffer))->Flags = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Flags) & (~DNS_GET_BIT_CD));
		if (((dns_hdr *)(Packet.Buffer))->Additional > 0)
			((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) - 1U);
		EDNS_SwitchLength -= Packet.EDNS_Record;
	}

//Multi request process
	if (Parameter.AlternateMultiRequest || Parameter.MultiRequestTimes > 1U)
		UDPRequestMulti(Packet.Buffer, EDNS_SwitchLength, &LocalSocketData, Packet.Protocol);
//Normal request process
	else 
		UDPRequest(Packet.Buffer, EDNS_SwitchLength, &LocalSocketData, Packet.Protocol);

//Fin TCP request connection.
	if (Packet.Protocol == IPPROTO_TCP && SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_INVALID_CHECK, false, nullptr))
	{
		shutdown(LocalSocketData.Socket, SD_BOTH);
		closesocket(LocalSocketData.Socket);
	}

//EDNS switching(Part 2)
	if (Parameter.EDNS_Label && !Parameter.EDNS_Switch.EDNS_UDP)
	{
		((dns_hdr *)(Packet.Buffer))->Flags = EDNS_Packet_Flags;
		((dns_hdr *)(Packet.Buffer))->Additional = htons(ntohs(((dns_hdr *)(Packet.Buffer))->Additional) + 1U);
	}

	return;
}
#endif

//Send responses to requester
bool __fastcall SendToRequester(
	char *RecvBuffer, 
	const size_t RecvSize, 
	const size_t MaxLen, 
	const uint16_t Protocol, 
	const SOCKET_DATA &LocalSocketData)
{
//Response check
	if (RecvSize < DNS_PACKET_MINSIZE || CheckEmptyBuffer(RecvBuffer, RecvSize) || 
		((pdns_hdr)RecvBuffer)->ID == 0 || ((pdns_hdr)RecvBuffer)->Flags == 0) //DNS header ID and flags must not be set 0.
			return false;

//TCP protocol
	if (Protocol == IPPROTO_TCP)
	{
		if (AddLengthDataToHeader(RecvBuffer, RecvSize, MaxLen) == EXIT_FAILURE)
		{
			shutdown(LocalSocketData.Socket, SD_BOTH);
			closesocket(LocalSocketData.Socket);
			return false;
		}
		else {
			send(LocalSocketData.Socket, RecvBuffer, (int)(RecvSize + sizeof(uint16_t)), 0);
			shutdown(LocalSocketData.Socket, SD_BOTH);
			closesocket(LocalSocketData.Socket);
		}
	}
//UDP protocol
	else {
		sendto(LocalSocketData.Socket, RecvBuffer, (int)RecvSize, 0, (PSOCKADDR)&LocalSocketData.SockAddr, LocalSocketData.AddrLen);
	}

	return true;
}

//Mark responses to domains Cache
bool __fastcall MarkDomainCache(
	const char *Buffer, 
	const size_t Length)
{
//Check conditions.
	auto DNS_Header = (pdns_hdr)Buffer;
	if (
	//Not a response packet
		(ntohs(DNS_Header->Flags) & DNS_GET_BIT_RESPONSE) == 0 || 
	//Question Resource Records must be one.
		DNS_Header->Question != htons(U16_NUM_ONE) || 
	//Not any Answer Resource Records
		(DNS_Header->Answer == 0 && DNS_Header->Authority == 0 /* && DNS_Header->Additional == 0 */ ) || 
	//OPCode must be set Query/0.
		(ntohs(DNS_Header->Flags) & DNS_GET_BIT_OPCODE) != DNS_OPCODE_QUERY || 
	//Truncated bit must not be set.
		(ntohs(DNS_Header->Flags) & DNS_GET_BIT_TC) != 0 || 
	//RCode must be set No Error or Non-Existent Domain.
		((ntohs(DNS_Header->Flags) & DNS_GET_BIT_RCODE) != DNS_RCODE_NOERROR && (ntohs(DNS_Header->Flags) & DNS_GET_BIT_RCODE) != DNS_RCODE_NXDOMAIN))
			return false;

//Initialization(A part)
	DNS_CACHE_DATA DNSCacheDataTemp;
	DNSCacheDataTemp.Length = 0;
	DNSCacheDataTemp.ClearCacheTime = 0;
	DNSCacheDataTemp.RecordType = ((pdns_qry)(Buffer + DNS_PACKET_QUERY_LOCATE(Buffer)))->Type;
	uint32_t ResponseTTL = 0;

//Mark DNS A records and AAAA records only.
	if (DNSCacheDataTemp.RecordType == htons(DNS_RECORD_AAAA) || DNSCacheDataTemp.RecordType == htons(DNS_RECORD_A))
	{
		size_t DataLength = DNS_PACKET_RR_LOCATE(Buffer), TTLCounts = 0;
		pdns_record_standard DNS_Record_Standard = nullptr;
		uint16_t DNS_Pointer = 0;

	//Scan all Answers Resource Records.
		for (size_t Index = 0;Index < (size_t)ntohs(DNS_Header->Answer);++Index)
		{
		//Pointer check
			if (DataLength + sizeof(uint16_t) < Length && (uint8_t)Buffer[DataLength] >= DNS_POINTER_8_BITS)
			{
				DNS_Pointer = ntohs(*(uint16_t *)(Buffer + DataLength)) & DNS_POINTER_BITS_GET_LOCATE;
				if (DNS_Pointer >= Length || DNS_Pointer < sizeof(dns_hdr) || DNS_Pointer == DataLength || DNS_Pointer == DataLength + 1U)
					return false;
			}

		//Resource Records Name(Domain Name)
			DataLength += CheckQueryNameLength(Buffer + DataLength) + 1U;
			if (DataLength + sizeof(dns_record_standard) > Length)
				break;

		//Standard Resource Records
			DNS_Record_Standard = (pdns_record_standard)(Buffer + DataLength);
			DataLength += sizeof(dns_record_standard);
			if (DataLength > Length || (DNS_Record_Standard != nullptr && DataLength + ntohs(DNS_Record_Standard->Length) > Length))
				break;

		//Resource Records Data
			if (DNS_Record_Standard->Classes == htons(DNS_CLASS_IN) && DNS_Record_Standard->TTL > 0 && 
				((DNS_Record_Standard->Type == htons(DNS_RECORD_AAAA) && DNS_Record_Standard->Length == htons(sizeof(in6_addr))) || 
				(DNS_Record_Standard->Type == htons(DNS_RECORD_A) && DNS_Record_Standard->Length == htons(sizeof(in_addr)))))
			{
				ResponseTTL += ntohl(DNS_Record_Standard->TTL);
				++TTLCounts;
			}

			DataLength += ntohs(DNS_Record_Standard->Length);
		}

	//Calculate average TTL.
		if (TTLCounts > 0)
			ResponseTTL = ResponseTTL / (uint32_t)TTLCounts + ResponseTTL % (uint32_t)TTLCounts;
	}

//Set cache TTL.
	if (ResponseTTL == 0 && DNS_Header->Authority == 0) //Only mark A and AAAA records.
	{
		return false;
	}
	else {
		if (Parameter.CacheType == CACHE_TYPE_TIMER)
		{
			if (ResponseTTL * SECOND_TO_MILLISECOND < Parameter.CacheParameter)
				ResponseTTL = (uint32_t)(Parameter.CacheParameter / SECOND_TO_MILLISECOND - ResponseTTL + STANDARD_TIMEOUT / SECOND_TO_MILLISECOND);
		}
		else { //CACHE_TYPE_QUEUE
			if (ResponseTTL < Parameter.HostsDefaultTTL)
				ResponseTTL = Parameter.HostsDefaultTTL - ResponseTTL + STANDARD_TIMEOUT / SECOND_TO_MILLISECOND;
		}
	}

//Initialization(B part)
	if (Length <= DOMAIN_MAXSIZE)
	{
		std::shared_ptr<char> DNSCacheDataBufferTemp(new char[DOMAIN_MAXSIZE]());
		memset(DNSCacheDataBufferTemp.get(), 0, DOMAIN_MAXSIZE);
		DNSCacheDataTemp.Response.swap(DNSCacheDataBufferTemp);
	}
	else {
		std::shared_ptr<char> DNSCacheDataBufferTemp(new char[Length]());
		memset(DNSCacheDataBufferTemp.get(), 0, Length);
		DNSCacheDataTemp.Response.swap(DNSCacheDataBufferTemp);
	}

//Mark to global list.
	if (DNSQueryToChar(Buffer + sizeof(dns_hdr), DNSCacheDataTemp.Domain) > DOMAIN_MINSIZE)
	{
	//Domain Case Conversion
		CaseConvert(false, DNSCacheDataTemp.Domain);
		memcpy_s(DNSCacheDataTemp.Response.get(), PACKET_MAXSIZE, Buffer + sizeof(uint16_t), Length - sizeof(uint16_t));
		DNSCacheDataTemp.Length = Length - sizeof(uint16_t);

	#if defined(PLATFORM_WIN_XP)
/* Old version(2016-05-29)
		if (GlobalRunningStatus.FunctionPTR_GetTickCount64 != nullptr)
			DNSCacheDataTemp.ClearCacheTime = (size_t)((*GlobalRunningStatus.FunctionPTR_GetTickCount64)() + ResponseTTL * SECOND_TO_MILLISECOND);
		else 
*/
		DNSCacheDataTemp.ClearCacheTime = GetTickCount() + ResponseTTL * SECOND_TO_MILLISECOND;
	#else
		DNSCacheDataTemp.ClearCacheTime = GetTickCount64() + ResponseTTL * SECOND_TO_MILLISECOND;
	#endif

/* Old version(2016-05-19)
	//Check repeating items, delete duque rear and add new item to deque front.
		for (auto DNSCacheDataIter = DNSCacheList.begin();DNSCacheDataIter != DNSCacheList.end();++DNSCacheDataIter)
		{
			if (DNSCacheDataTemp.Domain == DNSCacheDataIter->Domain && DNSCacheDataTemp.RecordType == DNSCacheDataIter->RecordType)
			{
				DNSCacheList.erase(DNSCacheDataIter);
				break;
			}
		}
*/
	//Delete old cache.
		std::lock_guard<std::mutex> DNSCacheListMutex(DNSCacheListLock);
		if (Parameter.CacheType == CACHE_TYPE_QUEUE)
		{
			while (DNSCacheList.size() > Parameter.CacheParameter)
				DNSCacheList.pop_back();
		}
		else { //CACHE_TYPE_TIMER
		#if defined(PLATFORM_WIN_XP)
/* Old version(2016-05-29)
			while ((!DNSCacheList.empty() && (GlobalRunningStatus.FunctionPTR_GetTickCount64 != nullptr && 
				(*GlobalRunningStatus.FunctionPTR_GetTickCount64)() >= DNSCacheList.back().ClearCacheTime) ||
				GetTickCount() >= DNSCacheList.back().ClearCacheTime))
*/
			while (!DNSCacheList.empty() && GetTickCount() >= DNSCacheList.back().ClearCacheTime)
		#else
			while (!DNSCacheList.empty() && GetTickCount64() >= DNSCacheList.back().ClearCacheTime)
		#endif
				DNSCacheList.pop_back();
		}

		DNSCacheList.push_front(DNSCacheDataTemp);
		return true;
	}

	return false;
}
