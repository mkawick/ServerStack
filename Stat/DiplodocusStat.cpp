#include "DiplodocusStat.h"

#include "DiplodocusStat.h"

#include "../NetworkCommon/Packets/ServerToServerPacket.h"
#include "../NetworkCommon/Packets/PacketFactory.h"
#include "../NetworkCommon/Packets/StatPacket.h"

#include "../NetworkCommon/Database/StringLookup.h"

#include <iostream>
#include <time.h>

using namespace std;

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

DiplodocusStat::DiplodocusStat( const string& serverName, U32 serverId ): Diplodocus< KhaanStat >( serverName, serverId, 0,  ServerType_Stat )
                                         
{
    //time( &m_lastTimeStamp );
   SetSleepTime( 100 );

  /* 
   string exchangeQuery = "SELECT pe.id, pe.begin_date, pe.end_date, pe.enchange_uuid, pe.title_string, pe.description_string, pe.custom_uuid, ";
   exchangeQuery += " p1.product_id AS source_id, p1.uuid source_uuid, p1.name_string source_name, pe.source_count, p1.icon_lookup source_icon, p1.product_type source_type,";
   exchangeQuery += " p2.product_id AS dest_id, p2.uuid dest_uuid, p2.name_string dest_name, pe.dest_count, p2.icon_lookup dest_icon, p1.product_type dest_type ";       

   exchangeQuery += " FROM playdek.product_exchange_rate AS pe";
   exchangeQuery += " INNER JOIN product p1 on pe.product_source_id=p1.product_id";
   exchangeQuery += " INNER JOIN product p2 on pe.product_dest_id=p2.product_id ";
   
   const int FiveMinutes = 60 * 5;
   m_salesManager = new SalesManager( QueryType_ExchangeRateLookup, this, exchangeQuery, true );
   m_salesManager->SetPeriodicty( FiveMinutes ); //

   vector< string > stringCategories;
   stringCategories.push_back( string( "product" ) );
   stringCategories.push_back( string( "sale" ) );
   m_stringLookup = new StringLookup( QueryType_ProductLookup, this, stringCategories );*/
}

DiplodocusStat :: ~DiplodocusStat()
{
   //delete m_staticAssets;
   //delete m_dynamicAssets;
}
//---------------------------------------------------------------

void     DiplodocusStat::ServerWasIdentified( IChainedInterface* khaan )
{
   BasePacket* packet = NULL;
   PackageForServerIdentification( m_serverName, m_localIpAddress, m_serverId, m_listeningPort, m_gameProductId, m_isGame, m_isControllerApp, true, m_isGateway, &packet );
   //khaan->AddOutputChainData( packet, 0 );
   //m_serversNeedingUpdate.push_back( static_cast<InputChainType*>( khaan )->GetServerId() );
   ChainedType* localKhaan = static_cast< ChainedType* >( khaan );
   localKhaan->AddOutputChainData( packet, 0 );
   m_clientsNeedingUpdate.push_back( localKhaan->GetServerId() );
}

//---------------------------------------------------------------

bool     DiplodocusStat::AddInputChainData( BasePacket* packet, U32 connectionId )
{
   if( packet->packetType == PacketType_GatewayInformation )
   {
      PacketCleaner cleaner( packet );
      HandleCommandFromGateway( packet, connectionId );
      return true;
   }

   if( packet->packetType == PacketType_ServerJobWrapper )
   {
      PacketCleaner cleaner( packet );
      HandlePacketFromOtherServer( packet, connectionId );
      return true;
   }

   if( packet->packetType == PacketType_GatewayWrapper ) // this should not happen on the stat server
   {
     /* PacketCleaner cleaner( packet );
      PacketGatewayWrapper* wrapper = static_cast< PacketGatewayWrapper* >( packet );
      BasePacket* unwrappedPacket = wrapper->pPacket;
      U32 connectionIdToUse = wrapper->connectionId;
      
      if( unwrappedPacket->packetType == PacketType_Purchase )
      {
         bool found = false;
         UAADMapIterator it = m_userTickets.begin();
         while( it != m_userTickets.end() )
         {
            if( it->second.GetUserTicket().connectionId == connectionIdToUse )
            {
               found = true;
               break;
            }
         }
         if( found == false )
         {
            cout << "packet from unknown connection" << connectionIdToUse << endl;
            PacketFactory factory;
            return true;
         }
         UserAccountPurchase& uap = it->second;
         PacketPurchase* pp = reinterpret_cast< PacketPurchase* >( unwrappedPacket );
         return uap.HandleRequestFromClient( pp );
      }
      else
      {
         assert( 0 );
      }*/
      assert( 0 );
      // we handle all packets from the gateway here.
      return false;
   }
   
   return false;
}

//---------------------------------------------------------------

bool  DiplodocusStat::HandlePacketFromOtherServer( BasePacket* packet, U32 connectionId )// not thread safe
{
   if( packet->packetType != PacketType_ServerJobWrapper )
   {
      return false;
   }

   PacketServerJobWrapper* wrapper = static_cast< PacketServerJobWrapper* >( packet );
   BasePacket* unwrappedPacket = wrapper->pPacket;
   U32  serverIdLookup = wrapper->serverId;
   serverIdLookup = serverIdLookup;

   U8 packetType = unwrappedPacket->packetType;
   
   if( packetType == PacketType_Stat )
   {
      PacketCleaner cleaner( unwrappedPacket );

      PacketStat* statPacket = static_cast< PacketStat* >( unwrappedPacket );
      cout << "***********************************************" << endl;
      cout << " stat packet contents " << endl;
      cout << "   statName    :" << statPacket->statName << endl;
      cout << "serverReporting:" << statPacket->serverReporting << endl;
      cout << "   category    :" << statPacket->category << endl;
      cout << "   subCategory :" << statPacket->subCategory << endl;
      cout << "   value       :" << statPacket->value << endl;
      cout << "   subValue    :" << statPacket->subValue << endl;
      cout << "   minValue    :" << statPacket->minValue << endl;
      cout << "   maxValue    :" << statPacket->maxValue << endl;
      cout << "   numValues   :" << statPacket->numValues << endl;
      cout << "   stdDev      :" << statPacket->stdDev << endl;
      cout << "   beginTime   :" << statPacket->beginTime << endl;
      cout << "   endTime     :" << statPacket->endTime << endl;


      cout << "***********************************************" << endl;

      return true;
   }

   return false;
}

//---------------------------------------------------------------
//---------------------------------------------------------------

bool     DiplodocusStat::AddQueryToOutput( PacketDbQuery* packet )
{
   ChainLinkIteratorType itOutputs = m_listOfOutputs.begin();
   while( itOutputs != m_listOfOutputs.end() )// only one output currently supported.
   {
      ChainType* outputPtr = static_cast< ChainType*> ( (*itOutputs).m_interface );
      if( outputPtr->AddInputChainData( packet, m_chainId ) == true )
      {
         return true;
      }
      itOutputs++;
   }

   delete packet;/// normally, we'd leave this up to the invoker to cleanup. 
   return false;
}

//---------------------------------------------------------------

// make sure to follow the model of the account server regarding queries. Look at CreateAccount
// 
bool     DiplodocusStat::AddOutputChainData( BasePacket* packet, U32 connectionId )
{
  /* if( packet->packetType == PacketType_GatewayWrapper )
   {
      if( m_connectionIdGateway == 0 )
         return false;

      Threading::MutexLock locker( m_mutex );

      ChainLinkIteratorType itInputs = m_listOfInputs.begin();
      while( itInputs != m_listOfInputs.end() )
      {
         ChainLink& chainedInput = *itInputs++;
         IChainedInterface* interfacePtr = chainedInput.m_interface;
         KhaanPurchase* khaan = static_cast< KhaanPurchase* >( interfacePtr );
         if( khaan->GetServerId() == m_connectionIdGateway )
         {
            khaan->AddOutputChainData( packet );
            //khaan->Update();// the gateway may not have a proper connection id.

            AddServerNeedingUpdate( khaan->GetServerId() );
            return true;
         }
      }
      return false;
   }
   if( packet->packetType == PacketType_ServerJobWrapper )
   {
      return HandlePacketToOtherServer( packet, connectionId );
   }
   if( packet->packetType == PacketType_DbQuery )
   {
      if( packet->packetSubType == BasePacketDbQuery::QueryType_Result )
      {
         bool wasHandled = false;
         PacketDbQueryResult* dbResult = static_cast<PacketDbQueryResult*>( packet );
         if( m_salesManager->HandleResult( dbResult ) == true )
         {
            wasHandled = true;
         }
         else if( m_stringLookup->HandleResult( dbResult ) == true )
         {
            wasHandled = true;
         }

         if( wasHandled == true )
         {
            PacketFactory factory;
            factory.CleanupPacket( packet );
         }
         return wasHandled;
      }
   }
   if( m_userTickets.size() )
   {
      Threading::MutexLock locker( m_mutex );
      UAADMapIterator it = m_userTickets.begin();
      while( it != m_userTickets.end() )
      {
         UAADMapIterator currentIt = it++;
         if( currentIt->second.LogoutExpired() )
         {
            //delete it->second;// bad idea
            m_userTickets.erase( currentIt );
         }
      }
   }*/

   return false;
}

//---------------------------------------------------------------
/*
void     DiplodocusStat::AddServerNeedingUpdate( U32 serverId )
{
   deque< U32 >::iterator it = m_serversNeedingUpdate.begin();
   while( it != m_serversNeedingUpdate.end() )
   {
      if( *it == serverId )
         return;
      it++;
   }
   m_serversNeedingUpdate.push_back( serverId );

}*/

//---------------------------------------------------------------

int      DiplodocusStat::CallbackFunction()
{
   UpdateAllConnections();

   time_t currentTime;
   time( &currentTime );


   UpdateConsoleWindow( m_timeOfLastTitleUpdate, m_uptime, m_numTotalConnections, m_connectedClients.size(), m_listeningPort, m_serverName );

   return 1;
}


//---------------------------------------------------------------
//---------------------------------------------------------------