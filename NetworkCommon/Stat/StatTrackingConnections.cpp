// StatTrackingConnections.cpp


#include "StatTrackingConnections.h"

#include "../Packets/PacketFactory.h"
#include "../Packets/ServerToServerPacket.h"
#include "../Utils/Utils.h"

#include "../NetworkOut/Fruitadens.h"

//////////////////////////////////////////////////////////

StatTrackingConnections :: StatTrackingConnections() : m_timeoutSendStatServerStats ( 0 )
{
}

void  StatTrackingConnections :: GetStatString( U16 statId, PacketStat* packet )
{
   packet->statType = PacketStat::StatType_Accumulator;
   switch( statId )
   {
   case StatTracking_BadPacketVersion:
      packet->statName = "packet.version";
      break;

   case StatTracking_UserBlocked:
      packet->statName = "user.blocked";
      break;
   case StatTracking_ForcedDisconnect:
      packet->statName = "user.force_disconnect";
      break;
   case StatTracking_UserLoginSuccess:
      packet->statName = "user.login_success";
      break;
   case StatTracking_UserLogoutSuccess:
      packet->statName = "user.logout_success";
      break;

   case StatTracking_UserAverageTimeOnline:
      packet->statName = "user.aggregate.timeonline";
      packet->statType = PacketStat::StatType_SimpleValue;
      break;
   case StatTracking_UserTotalTimeOnline:
      packet->statName = "user.aggregate.timeonline";
      packet->statType = PacketStat::StatType_SimpleValue;
      break;

   case StatTracking_GamePacketsSentToGame:
      packet->statName = "game_packet.count_to_server";
      break;
   case StatTracking_GamePacketsSentToClient:
      packet->statName = "game_packet.count_to_client";
      break;
   case StatTracking_NumUsersOnline:
      packet->statName = "gateway.num_users_current";
      packet->statType = PacketStat::StatType_SimpleValue;
      break;
   case StatTracking_UserTotalCount:
      packet->statName = "gateway.num_users_total";
      packet->statType = PacketStat::StatType_SimpleValue;
      break;

   case StatTracking_SuccessfulLogins:
      packet->statName = "login.users_logged_in";
      packet->statType = PacketStat::StatType_SimpleValue;
      break;
   case StatTracking_PurchaseMade:
      packet->statName = "purchase.purchases_made";
      break;
   case StatTracking_UserRelogin:
      packet->statName = "login.user_relogged";
      break;
   case StatTracking_UsersPerGame:
      packet->statName = "game.numusers";
      break;
   case StatTracking_UsersAverageTimePerGame:
      packet->statName = "game.average_login_time";
      packet->statType = PacketStat::StatType_SimpleValue;
      break;
   case StatTracking_UsersPlayedMultipleGames:
      packet->statName = "login.user_played_multiple_games";
      break;
   case StatTracking_UsersLostConnection:
      packet->statName = "gateway.user_connection_lost";
      break;
   case StatTracking_UniquesUsersPerDay:
      packet->statName = "gateway.unique_users_per_day";
      packet->statType = PacketStat::StatType_SimpleValue;
      break;
   }

}
//-----------------------------------------------------------------------------------------
/*
void     StatTrackingConnections::TrackStats( StatTracking stat, float value )
{
   PacketStat* packet = new PacketStat;
   packet->serverReporting = "gateway";
   GetStatString( stat, packet );
   packet->value = value;
   packet->timestamp = GetDateInUTC();

   m_stats.push_back( packet );
}*/

//-----------------------------------------------------------------------------------------

void  StatTrackingConnections::TrackCountStats( const string& serverName, int ServerId, StatTracking stat, float value, int sub_category )
{
   PacketStat* packet = new PacketStat;
   packet->serverReporting = serverName;
   packet->category = ServerId;
   packet->subCategory = sub_category;

   GetStatString( stat, packet );  
   packet->value = value;
   packet->timestamp = GetDateInUTC();

   m_stats.push_back( packet );
}

//-----------------------------------------------------------------------------------------

void     StatTrackingConnections::TrackStats( const string& serverName, int ServerId, const string& statName, U16 stat, float value, PacketStat::StatType type )
{
   PacketStat* packet = new PacketStat;
   packet->serverReporting = serverName;
   packet->statName = statName;
   packet->category = ServerId;
   packet->subCategory = stat;

   packet->value = value;
   packet->timestamp = GetDateInUTC();
   packet->statType = type;

   m_stats.push_back( packet );
}


//-----------------------------------------------------------------------------------------

void     StatTrackingConnections::SendStatsToStatServer( Fruitadens* fruity, const string& serverName, U32 serverId, ServerType serverType )
{
   PacketFactory factory;
   if( fruity != NULL )
   {
      while( m_stats.size() )
      {
         PacketStat* packet = static_cast< PacketStat* >( m_stats.front() );
         m_stats.pop_front();

         packet->serverReporting = serverName;
         packet->category = serverType;
         packet->subCategory = serverId;

         //Gateway
         PacketServerToServerWrapper* wrapper = new PacketServerToServerWrapper;
         wrapper->gameInstanceId = serverId;
         wrapper->gameProductId = 0;
         wrapper->serverId = serverId;
         wrapper->pPacket = packet;
         
         U32 unusedParam = -1;
         if( fruity->AddOutputChainData( wrapper, unusedParam ) == false )
         {
            PrintDebugText( "SendStatsToStatServer had a problem" ); 
            BasePacket* deletedPacket = static_cast< BasePacket* >( packet );
            factory.CleanupPacket( deletedPacket );
            return;
         }
      }

   }
   else// simply cleanup
   {
      while( m_stats.size() )
      {
         BasePacket* packet = static_cast< BasePacket* >( m_stats.front() );
         m_stats.pop_front();
         factory.CleanupPacket( packet );
      }
   }
}

//////////////////////////////////////////////////////////