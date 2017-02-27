/*******************************************************************************************
 
			(c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2017] ++
			
			(c) Copyright Nexus Developers 2014 - 2017
			
			http://www.opensource.org/licenses/mit-license.php
  
*******************************************************************************************/

#ifndef NEXUS_LLP_INCLUDE_UNIFIEDTIME_H
#define NEXUS_LLP_INCLUDE_UNIFIEDTIME_H

#include <vector>
#include <time.h>

#include "../Include/protocol.h"
#include "../wallet/db.h"

#include "server.h"
#include <inttypes.h>

/* TODO: Put in the LLP Namespace. */

/** Carried on from uint1024.h **/
typedef long long  int64;


/** Unified Time Flags **/
extern bool fTimeUnified;
extern bool fIsTimeSeed;
extern bool fIsSeedNode;


/* Offset to be added to your Local Time. This counteracts the effects of changing your clock by will or accident. */
extern int UNIFIED_LOCAL_OFFSET;


/* Offset calculated from average Unified Offset collected over time. */
extern int UNIFIED_AVERAGE_OFFSET;


/* Vector to Contain list of Unified Time Offset from Time Seeds, Seed Nodes, and Peers. */
extern std::vector<int> UNIFIED_TIME_DATA;

extern std::vector<LLP::CAddress> SEED_NODES;
extern std::vector<LLP::CAddress> TRUSTED_NODES;



/* The Maximum Seconds that a Clock can be Off. This is set to account
    for Network Propogation Times and Normal Hardware level clock drifting. */
extern int MAX_UNIFIED_DRIFT;


/** Initializes the Unifed Time System. 
    A] Checks Database for Offset Average List
	B] Gets Periodic Average of 10 Seeds if first Unified Time **/
void InitializeUnifiedTime();


/** Gets the Current Unified Time Average. The More Data in Time Average, the less
    a pesky node with a manipulated time seed can influence. Keep in mind too that the
	Unified Time will only be updated upon your outgoing connection... otherwise anyone flooding
	network with false time seed will just be ignored. The average is a moving one, so that iter_swap
	will allow clocks that operate under different intervals to always stay synchronized with the network. **/
int GetUnifiedAverage();



/** Unified Time Clock Regulator. Periodically gets Offset from Time Seeds to build a strong Average.
    Checks current time against itself, if there is too much drift, your local offset adjusts to Unified Average. **/
void ThreadTimeRegulator(void* parg);
void ThreadUnifiedSamples(void* parg);


namespace LLP
{

	class CoreOutbound : public Outbound
	{
		Service_t IO_SERVICE;
		
	public:
		CoreOutbound(std::string ip, std::string port) : Outbound(ip, port){}
		
		enum
		{
			/** DATA PACKETS **/
			TIME_DATA     = 0,
			ADDRESS_DATA  = 1,
			TIME_OFFSET   = 2,
			
			/** DATA REQUESTS **/
			GET_OFFSET    = 64,
			
			
			/** REQUEST PACKETS **/
			GET_TIME      = 129,
			GET_ADDRESS   = 130,
					

			/** GENERIC **/
			PING          = 253,
			CLOSE         = 254
		};
		
		inline Packet NewPacket() { return this->INCOMING; }
		
		inline Packet GetPacket(unsigned char HEADER)
		{
			Packet PACKET;
			PACKET.HEADER = HEADER;
			return PACKET;
		}
		
		inline void GetOffset(unsigned int nTimestamp)
		{
			Packet REQUEST = GetPacket(GET_OFFSET);
			REQUEST.LENGTH = 4;
			REQUEST.DATA   = uint2bytes(nTimestamp);
			
			this->WritePacket(REQUEST);
		}
		
		inline void GetTime()
		{
			Packet REQUEST = GetPacket(GET_TIME);
			this->WritePacket(REQUEST);
		}
		
		void Close()
		{
			Packet RESPONSE = GetPacket(CLOSE);
			this->WritePacket(RESPONSE);
			this->Disconnect();
		}
		
		inline void GetAddress()
		{
			Packet REQUEST = GetPacket(GET_ADDRESS);
			this->WritePacket(REQUEST);
		}
	};
	
	class CoreLLP : public Connection<>
	{	
		std::vector<unsigned char> ADDRESS;
		
		enum
		{
			/** DATA PACKETS **/
			TIME_DATA     = 0,
			ADDRESS_DATA  = 1,
			TIME_OFFSET   = 2,
			
			/** DATA REQUESTS **/
			GET_OFFSET    = 64,
			
			
			/** REQUEST PACKETS **/
			GET_TIME      = 129,
			GET_ADDRESS   = 130,
					

			/** GENERIC **/
			PING          = 253,
			CLOSE         = 254
		};
	
	public:
		CoreLLP() : Connection(){ }
		CoreLLP( Socket_t SOCKET_IN, DDOS_Filter* DDOS_IN, bool isDDOS = false ) : Connection( SOCKET_IN, DDOS_IN ) 
		{ ADDRESS = parse_ip(SOCKET_IN->remote_endpoint().address().to_string()); }
		
		/** Handle Event Inheritance. **/
		void Event(unsigned char EVENT, unsigned int LENGTH = 0)
		{
			/** Handle any DDOS Packet Filters. **/
			if(EVENT == EVENT_HEADER)
			{
				if(fDDOS)
				{
					Packet PACKET   = this->INCOMING;
					
					if(PACKET.HEADER == TIME_DATA)
						DDOS->Ban();
					
					if(PACKET.HEADER == ADDRESS_DATA)
						DDOS->Ban();
					
					if(PACKET.HEADER == TIME_OFFSET)
						DDOS->Ban();
					
					if(PACKET.HEADER == GET_OFFSET && PACKET.LENGTH > 4)
						DDOS->Ban();
					
					if(DDOS->Banned())
						return;
					
				}
			}
			
			
			/** Handle for a Packet Data Read. **/
			if(EVENT == EVENT_PACKET)
				return;
			
			
			/** On Generic Event, Broadcast new block if flagged. **/
			if(EVENT == EVENT_GENERIC)
				return;
			
			/** On Connect Event, Assign the Proper Daemon Handle. **/
			if(EVENT == EVENT_CONNECT)
				return;
			
			/** On Disconnect Event, Reduce the Connection Count for Daemon **/
			if(EVENT == EVENT_DISCONNECT)
				return;
		
		}
		
		/** This function is necessary for a template LLP server. It handles your 
			custom messaging system, and how to interpret it from raw packets. **/
		bool ProcessPacket()
		{
			Packet PACKET   = this->INCOMING;
			
			//TODO: Calculate the latency of the request and put that into the time equation
			if(PACKET.HEADER == GET_OFFSET)
			{
				unsigned int nTimestamp = bytes2uint(PACKET.DATA);
				int   nOffset    = (int)(GetUnifiedTimestamp() - nTimestamp);
				
				Packet RESPONSE;
				RESPONSE.HEADER = TIME_OFFSET;
				RESPONSE.LENGTH = 4;
				RESPONSE.DATA   = int2bytes(nOffset);
				
				if(GetArg("-verbose", 0) >= 3)
					printf("***** Core LLP: Sent Offset %i | %u.%u.%u.%u | Unified %"PRId64"\n", nOffset, ADDRESS[0], ADDRESS[1], ADDRESS[2], ADDRESS[3], GetUnifiedTimestamp());
				
				this->WritePacket(RESPONSE);
				return true;
			}

			if(PACKET.HEADER == GET_TIME)
			{
				Packet RESPONSE;
				RESPONSE.HEADER = TIME_DATA;
				RESPONSE.LENGTH = 4;
				RESPONSE.DATA = uint2bytes((unsigned int)GetUnifiedTimestamp());
				
				if(GetArg("-verbose", 0) >= 3)
					printf("***** Core LLP: Sent Time Sample %"PRId64" to %u.%u.%u.%u\n", GetUnifiedTimestamp(), ADDRESS[0], ADDRESS[1], ADDRESS[2], ADDRESS[3]);
				
				this->WritePacket(RESPONSE);
				return true;
			}
			
			if(PACKET.HEADER == GET_ADDRESS)
			{
				Packet RESPONSE;
				RESPONSE.HEADER = ADDRESS_DATA;
				RESPONSE.LENGTH = 4;
				RESPONSE.DATA   = ADDRESS;
				
				this->WritePacket(RESPONSE);
				return true;
			}
			
			return false;
		}
	};
}


#endif