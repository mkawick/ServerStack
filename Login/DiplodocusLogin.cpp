// DiplodocusLogin.cpp

#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
using boost::format;

#include "../NetworkCommon/Utils/TableWrapper.h"
#include "../NetworkCommon/Utils/Utils.h"
#include "../NetworkCommon/Utils/StringUtils.h"

#include "../NetworkCommon/Database/StringLookup.h"

#include "../NetworkCommon/ServerConstants.h"

#if PLATFORM == PLATFORM_WINDOWS
#pragma warning( disable:4996 )
#endif

#include "../NetworkCommon/Packets/ServerToServerPacket.h"
#include "../NetworkCommon/Packets/GamePacket.h"
#include "../NetworkCommon/Packets/DbPacket.h"
#include "../NetworkCommon/Packets/CheatPacket.h"
#include "../NetworkCommon/Packets/LoginPacket.h"
#include "../NetworkCommon/Packets/PacketFactory.h"
#include "../NetworkCommon/Utils/CommandLineParser.h"

#include "../NetworkCommon/Database/Deltadromeus.h"

#include "../NetworkCommon/Logging/server_log.h"
#include "../NetworkCommon/NetworkIn/DiplodocusTools.h"

#include <boost/lexical_cast.hpp>


#include "DiplodocusLogin.h"
#include "FruitadensLogin.h"

#include "ScheduledOutageDBReader.h"

//#define _DEMO_13_Aug_2013
const int normalLogoutExpireTime = 18; // seconds


//////////////////////////////////////////////////////////

DiplodocusLogin:: DiplodocusLogin( const string& serverName, U32 serverId )  : 
                  Queryer(),
                  Diplodocus< KhaanLogin >( serverName, serverId, 0, ServerType_Login ), 
                  StatTrackingConnections(),
                  m_isInitialized( false ), 
                  m_isInitializing( true ),
                  m_autoAddProductFromWhichUsersLogin( true ),
                  m_stringLookup( NULL ),
                  m_numRelogins( 0 ),
                  m_numFailedLogins( 0 ),
                  m_numSuccessfulLogins( 0 ),
                  m_totalUserLoginSeconds( 0 ),
                  m_totalNumLogouts( 0 ),
                  m_printPacketTypes( false ),
                  m_printFunctionNames( false ),
                  m_outageReader( NULL )
{
   SetSleepTime( 19 );
   LogOpen();
   LogMessage( LOG_PRIO_INFO, "Login::Login server created" );

   time_t currentTime;
   time( &currentTime );
   m_initializingProductListTimeStamp = currentTime;
   m_timestampHourlyStatServerStatisics = currentTime;
}

void  DiplodocusLogin:: Init()
{
   vector< string > stringCategories;
   stringCategories.push_back( string( "product" ) );
   stringCategories.push_back( string( "sale" ) );
   m_stringLookup = new StringLookup( QueryType_ProductStringLookup, this, stringCategories );

   m_timestampDailyStatServerStatisics = m_timestampHourlyStatServerStatisics;

   m_timestampHourlyStatServerStatisics = ZeroOutMinutes( m_timestampHourlyStatServerStatisics );
   m_timestampDailyStatServerStatisics = ZeroOutHours( m_timestampDailyStatServerStatisics );

   m_outageReader = new ScheduledOutageDBReader;
   m_outageReader->SetMainLoop( this );
}

void           DiplodocusLogin:: PrintPacketTypes( bool printingOn )
{
   m_printPacketTypes = printingOn;
   if( printingOn == true )
   {
      cout << "Not functioning" << endl;
      assert(0);
   }
}

void          DiplodocusLogin::  PrintFunctionNames( bool printingOn )
{
   m_printFunctionNames = printingOn;
}
//---------------------------------------------------------------

void     DiplodocusLogin:: ServerWasIdentified( IChainedInterface* khaan )
{
   if( m_printFunctionNames )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   BasePacket* packet = NULL;
   PackageForServerIdentification( m_serverName, m_localIpAddress, m_externalIpAddress, m_serverId, m_serverType, m_listeningPort, m_gameProductId, m_isGame, m_isControllerApp, true, m_gatewayType, &packet );
   Khaan* localKhaan = static_cast< Khaan* >( khaan );
   localKhaan->AddOutputChainDataNoLock( packet );
   // this is not thread safe, but will be invoked from within the same thread.
   m_clientsNeedingUpdate.push_back( localKhaan->GetChainedId() );
}

/////////////////////////////////////////////////////////////////////////////////

void     DiplodocusLogin::InputConnected( IChainedInterface* chainedInput )
{
   if( m_printFunctionNames )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   LogMessage( LOG_PRIO_INFO, "DiplodocusLogin::InputConnected" );
   KhaanLogin* khaan = static_cast< KhaanLogin* >( chainedInput );
   string currentTime = GetDateInUTC();

   string printer = "Accepted connection at time:" + currentTime + " from " + inet_ntoa( khaan->GetIPAddress().sin_addr );
   LogMessage( LOG_PRIO_INFO, printer.c_str() );
   if( m_printFunctionNames )
   {
      LogMessage( LOG_PRIO_INFO, printer.c_str() );
   }
   PrintDebugText( "** InputConnected" , 1 );

   int outputBufferSize = 128 * 1024;
   LogMessage( LOG_PRIO_INFO, "DiplodocusLogin::SetOutputBufferSize( %d )", outputBufferSize );
   khaan->SetOutboudBufferSize( outputBufferSize );
}

/////////////////////////////////////////////////////////////////////////////////

void     DiplodocusLogin::InputRemovalInProgress( IChainedInterface * chainedInput )
{
   KhaanLogin* khaan = static_cast< KhaanLogin* >( chainedInput );

   SetupClientWaitingToBeRemoved( khaan );

   string currentTime = GetDateInUTC();
   string printer = "DiplodocusLogin::Client disconnection at time:" + currentTime + " from " + inet_ntoa( khaan->GetIPAddress().sin_addr );
   LogMessage( LOG_PRIO_INFO, printer.c_str() );

   LogMessage( LOG_PRIO_ERR, "** InputRemovalInProgress" );
}


//---------------------------------------------------------------

bool     DiplodocusLogin:: AddInputChainData( BasePacket* packet, U32 gatewayId )
{
   if( m_printFunctionNames )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   // all packets coming in should be from the gateway only through s2s connections.
   // this is already multi threaded, so putting threading protections here is duplicative.

   if( packet->packetType != PacketType_GatewayWrapper )
   {
      string text = "Login server: received junk packets. Type: ";
      text += packet->packetType;
      LogMessage( LOG_PRIO_ERR, text.c_str() );
      return false;
   }

   m_mutex.lock();
   m_inputPacketsToBeProcessed.push_back( PacketStorage( packet, gatewayId ) );
   m_mutex.unlock();
   return true;
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: ProcessPacket( PacketStorage& storage )
{
   BasePacket* packet = storage.packet;
   U32 gatewayId = storage.gatewayId;

   PacketGatewayWrapper* wrapper = static_cast< PacketGatewayWrapper * >( packet );
   U32   userConnectionId = wrapper->connectionId;
   BasePacket* actualPacket = wrapper->pPacket;

   switch( actualPacket->packetType )
   {
      case PacketType_Login:
      {
         PacketFactory factory;
         switch( actualPacket->packetSubType )
         {
         case PacketLogin::LoginType_Login:
            {
               //PacketLogin* login = static_cast<PacketLogin*>( actualPacket );
               //LogUserIn( login->userName, login->password, login->loginKey, login->gameProductId, userConnectionId, login->gatewayId );
               //LogUserIn( login, userConnectionId );
               cout << "Bad packet: NO longer supported." << endl;
            }
            break;
         case PacketLogin::LoginType_LoginFromGateway:
            {
               PacketLoginFromGateway* login = static_cast<PacketLoginFromGateway*>( actualPacket );
               LogUserIn( login, userConnectionId );
            }
            break;
         case PacketLogin::LoginType_Logout:
            {
               PacketLogout* logout = static_cast<PacketLogout*>( actualPacket );
               //UpdateLastLoggedOutTime( userConnectionId );
               LogUserOut( userConnectionId, logout->wasDisconnectedByError );
            }
            break;
         case PacketLogin::LoginType_CreateAccount:
            {
               PacketCreateAccount* createAccount = static_cast<PacketCreateAccount*>( actualPacket );
               CreateUserAccount( userConnectionId, gatewayId, createAccount->userEmail, createAccount->password, createAccount->userName, createAccount->deviceAccountId, createAccount->deviceId, createAccount->languageId, createAccount->gameProductId );
            }
            break;
         case PacketLogin::LoginType_ListOfAggregatePurchases:
            {
               PacketListOfUserAggregatePurchases* purchases = static_cast<PacketListOfUserAggregatePurchases*>( actualPacket );
               StoreUserPurchases( userConnectionId, purchases );
            }
            break;
         case PacketLogin::LoginType_RequestListOfPurchases:
            {
               PacketListOfUserPurchasesRequest* purchaseRequest = static_cast<PacketListOfUserPurchasesRequest*>( actualPacket );
               RequestListOfPurchases( userConnectionId, purchaseRequest );
            }
            break;
         case PacketLogin::LoginType_AddPurchaseEntry:
            {
               PacketAddPurchaseEntry* addPurchase = static_cast<PacketAddPurchaseEntry*>( actualPacket );
               AddPurchase( userConnectionId, addPurchase );
            }
            break;
         case PacketLogin::LoginType_RequestUserProfile:
            {
               PacketRequestUserProfile* profileRequest = static_cast<PacketRequestUserProfile*>( actualPacket );
               RequestProfile( userConnectionId, profileRequest );
            }
            break;
         case PacketLogin::LoginType_UpdateUserProfile:
            {
               PacketUpdateUserProfile* updateProfileRequest = static_cast<PacketUpdateUserProfile*>( actualPacket );
               UpdateProfile( userConnectionId, updateProfileRequest );
            }
            break;
         case PacketLogin::LoginType_RequestListOfProducts:
            {
               PacketRequestListOfProducts* purchaseRequest = static_cast<PacketRequestListOfProducts*>( actualPacket );
               HandleRequestListOfProducts( userConnectionId, purchaseRequest );
            }
            break;

         case PacketLogin::LoginType_RequestOtherUserProfile:
            {
               PacketRequestOtherUserProfile* profileRequest = static_cast<PacketRequestOtherUserProfile*>( actualPacket );
               RequestOthersProfile( userConnectionId, profileRequest );
            }
            break;
         case PacketLogin::LoginType_UpdateSelfProfile:
            {
               PacketUpdateSelfProfile* updateProfileRequest = static_cast<PacketUpdateSelfProfile*>( actualPacket );
               UpdateProfile( userConnectionId, updateProfileRequest );
            }
            break;
         case PacketLogin::LoginType_DebugThrottleUsersConnection:
            {
               PacketLoginDebugThrottleUserPackets* throttleRequest = static_cast<PacketLoginDebugThrottleUserPackets*>( actualPacket );
               ThrottleUser( userConnectionId, throttleRequest );
            }
            break;
         case PacketLogin::LoginType_EchoToServer:
            {
               EchoHandler( userConnectionId );
            }
            break;

         case PacketLogin::LoginType_UserListOfPurchasesWasUpdated:
            {
               PacketListOfUserPurchasesUpdated* userInventory = static_cast<PacketListOfUserPurchasesUpdated*>( actualPacket );
               RequestListOfPurchasesUpdate( userInventory );
            }
            break;
         case PacketLogin::LoginType_LogoutAllUsers:
            {
               PacketLogin_LogoutAllUsers* usersLogout = static_cast< PacketLogin_LogoutAllUsers* >( actualPacket );
               LogoutListOfUsers( usersLogout, gatewayId );
            }
            break;
         
         factory.CleanupPacket( packet );
         /*delete wrapper;
         delete actualPacket;*/
         }
      }
      return true;
      case PacketType_Cheat:
      {
         PacketFactory factory;
         PacketCheat* cheat = static_cast<PacketCheat*>( actualPacket );
         HandleCheats( userConnectionId, cheat );
         factory.CleanupPacket( packet );
      }
      return true;
      
   }
   return false;
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: AddQueryToOutput( PacketDbQuery* dbQuery )
{
   if( m_printFunctionNames )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

  /* ChainLinkIteratorType itOutputs = m_listOfOutputs.begin();
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
   return false;*/

   PacketFactory factory;
   m_outputChainListMutex.lock();
   BaseOutputContainer tempOutputContainer = m_listOfOutputs;
   m_outputChainListMutex.unlock();

   ChainLinkIteratorType itOutputs = tempOutputContainer.begin();
   while( itOutputs != tempOutputContainer.end() )// only one output currently supported.
   {
      ChainType* outputPtr = static_cast< ChainType*> ( (*itOutputs).m_interface );
      if( outputPtr->GetChainedType() == ChainedType_DatabaseConnector )
      {
         bool isValidConnection = false;
         Database::Deltadromeus* delta = static_cast< Database::Deltadromeus* >( outputPtr );
         if( dbQuery->dbConnectionType != 0 )
         {
            if( delta->WillYouTakeThisQuery( dbQuery->dbConnectionType ) )
            {
               isValidConnection = true;
            }
         }
         else // if this query is not set, default to true
         {
            isValidConnection = true;
         }
         if( isValidConnection == true )
         {
            if( outputPtr->AddInputChainData( dbQuery, m_chainId ) == true )
            {
               return true;
            }
         }
      }
      itOutputs++;
   }

   BasePacket* deleteMe = static_cast< BasePacket*>( dbQuery );

   factory.CleanupPacket( deleteMe );
   return false;
}

//---------------------------------------------------------------

bool  DiplodocusLogin:: IsUserInLoginHistory( const string& userName, const string& password, const string& loginKey, U8 gameProductId, U32 connectionId, U32 gatewayId )
{
   U32 oldConnectionId = FindUserAlreadyInGame( userName, gameProductId );
   if( oldConnectionId != 0 )
   {
      ConnectionToUser* oldConnection = GetUserConnection( oldConnectionId );
      string uuid;
      if( oldConnection )
      {
         uuid = oldConnection->m_userUuid;

         bool thisUserLoggedInSuccessfully = false;
         if( oldConnection->GetUuid().size() > 0 )
            thisUserLoggedInSuccessfully = true;

         LogMessage( LOG_PRIO_INFO, "User was already in list of users" );
         if( oldConnection->GetPasswordHash() == password )
         {
            LogMessage( LOG_PRIO_INFO, "User name and password match" );
            if( thisUserLoggedInSuccessfully == false )// it was an invalid login again with the same bad password
               return false;// do the normal login and leave the bad login alone... it will clean itself up.
            
            // if we have the same password but it failed last time...
            // the following happens below.
            //ForceUserLogoutAndBlock( oldConnectionId, oldConnection->GetGatewayId() );// force disconnect on the new id
            //return true;// do not load
         }
         else
         {
            if( thisUserLoggedInSuccessfully == false )// he had an invalid login last time, but we are trying a different password
               return false;// do the normal login and leave the bad login alone... it will clean itself up.

            // this means that our passwords do not math 
            LogMessage( LOG_PRIO_ERR, "ERROR: Hacker alert: User name and password do not match on a relog" );
            LogMessage( LOG_PRIO_ERR, "ERROR: Could also be a user just correcting his/her password" );
            ForceUserLogoutAndBlock( connectionId, gatewayId );// force disconnect on the old id
            //return true;
         }
         
         // clear old connection and establish new one
         ForceUserLogoutAndBlock( oldConnectionId, oldConnection->GetGatewayId() );// force disconnect
      }

      // should we boot this user for hacking? Or is it bad code?
      LogMessage( LOG_PRIO_INFO, "Second login from the same product attempt was made" );
      LogMessage( LOG_PRIO_INFO, "    user: %s", userName.c_str() );
      LogMessage( LOG_PRIO_INFO, "    uuid: %s", uuid.c_str() );
      LogMessage( LOG_PRIO_INFO, "    pass: %s", password.c_str() );
      LogMessage( LOG_PRIO_INFO, " product: %u", gameProductId );

      LogMessage( LOG_PRIO_INFO, "Reconnect user" );
      ReinsertUserConnection( oldConnectionId, connectionId );// and we remove the old connection

      ConnectionToUser* connection = GetUserConnection( connectionId );
      if( connection )
      {
         connection->SetGatewayId( gatewayId );
         if( connection->IsLoggingOut() == false )// already failed. Screw this hacker
         {
            // user may attempt hacking.. only 3 attempts allowed
            if( connection->GetLoginAttemptCount () > 3 )
            {
               ForceUserLogoutAndBlock( connectionId, gatewayId );// force disconnect
               return true;
            }
            return SetupQueryForLogin( userName, password, gameProductId, connectionId );
         }

         connection->ClearLoggingOutStatus();
         if( connection->CanContinueLogginIn() == true )
         {
            const bool isLoggedIn = true; 
            const bool wasDisconnectedByError = false;

            BroadcastLoginStatus( connectionId, 
                                gameProductId, 
                                isLoggedIn, 
                                wasDisconnectedByError,
                                gatewayId );
            m_numRelogins ++;
            return true;
            
         }
         else
         {
            connection->UpdateConnectionId( connectionId );
            m_numFailedLogins ++;
          /*  ForceUserLogoutAndBlock( connectionId );*/
            return LoadUserAccount( userName, password, loginKey, gameProductId, connectionId, gatewayId );
         }
      }
      else
      {
         LogMessage( LOG_PRIO_ERR, "Error: could not find user by connectionId after verifying that s/he exists" );
      }
   }
   return false;
}

bool  DiplodocusLogin:: BroadcastLoginStatus( U32 connectionId, 
                                               U8 gameProductId, 
                                               bool isLoggedIn, 
                                               bool wasDisconnectedByError,
                                               U32 gatewayId )
{
   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      SendLoginStatusTo_Non_GameServers( connection->m_userName, 
                                          connection->m_userUuid, 
                                          connectionId, 
                                          gameProductId, 
                                          connection->m_lastLoginTime, 
                                          connection->m_isActive, 
                                          connection->m_email, 
                                          connection->m_passwordHash, 
                                          connection->m_id, 
                                          connection->m_loginKey, 
                                          connection->m_languageId, 
                                          isLoggedIn, 
                                          wasDisconnectedByError,
                                          gatewayId );

      LogMessage( LOG_PRIO_INFO, "--------------------------------------------" );
      LogMessage( LOG_PRIO_INFO, "User login:      %s", connection->m_userName.c_str() );
      LogMessage( LOG_PRIO_INFO, "User uuid:       %s", connection->m_userUuid.c_str() );
      LogMessage( LOG_PRIO_INFO, "lastLoginTime =  %s", connection->m_lastLoginTime.c_str() );
      LogMessage( LOG_PRIO_INFO, "--------------------------------------------" );
      if( connection->SuccessfulLoginFinished( connectionId, false ) == true )
      {
         LogMessage( LOG_PRIO_INFO, "User connection success" );
         m_uniqueUsers.insert( connection->m_userUuid );

         UpdateLastLoggedInTime( connectionId ); // update the user logged in time
         SendLoginStatusTo_GameServers( connection->m_userName, 
                                          connection->m_userUuid, 
                                          connectionId, 
                                          gameProductId, 
                                          connection->m_lastLoginTime, 
                                          connection->m_isActive, 
                                          connection->m_email, 
                                          connection->m_passwordHash, 
                                          connection->m_id, 
                                          connection->m_loginKey, 
                                          connection->m_languageId, 
                                          isLoggedIn, 
                                          wasDisconnectedByError,
                                          gatewayId );
         m_numSuccessfulLogins++;
         //m_numRelogins ++;
      }
      return true;
   }
   return false;
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: LogUserIn( const PacketLoginFromGateway* loginPacket, U32 connectionId )
{
   const string& userName = Trim( loginPacket->userName ); 
   const string& password = Trim( loginPacket->password );
   const string& loginKey = Trim( loginPacket->loginKey );
   U8 gameProductId = loginPacket->gameProductId;
   U32 gatewayId = loginPacket->gatewayId;

   if( m_printFunctionNames )
   {
      cout << "fn: " << __FUNCTION__ << endl;

      LogMessage( LOG_PRIO_INFO, "***********************" );
      LogMessage( LOG_PRIO_INFO, "attempt to login user: %s, pwHash:%s for game id=%d and conn: %d", userName.c_str(), password.c_str(), (int) gameProductId, connectionId );
      LogMessage( LOG_PRIO_INFO, "***********************" );
   }

   LogMessage( LOG_PRIO_INFO, "User login " );
   LogMessage( LOG_PRIO_INFO, "    User: %s", userName.c_str() );
   //LogMessage( LOG_PRIO_INFO, "    uuid: %s", loginPacket->uuid.c_str() );
   LogMessage( LOG_PRIO_INFO, "    pass: %s", password.c_str() );
   
   LogMessage( LOG_PRIO_INFO, "--------------------------------------" );
   if( gameProductId == 0 )
   {
      LogMessage( LOG_PRIO_ERR, "User logging in without a product id" );
   }
   if( loginPacket->gatewayId == 0 )
   {
      LogMessage( LOG_PRIO_ERR, "User logging in without a gateway id" );
   }
   
   if( IsUserConnectionValid( connectionId ) )
   {
      // should we boot this user for hacking? Or is it bad code?
      LogMessage( LOG_PRIO_INFO, "Second attempt was made at login from the same connection ID" );
      //InformUserOfSuccessfulLogout();// someone is hacking our server
      ForceUserLogoutAndBlock( connectionId, gatewayId );
   }

   if( userName.size() == 0 )
   {
      // should we boot this user for hacking? Or is it bad code?
      LogMessage( LOG_PRIO_ERR, "invalid attempt at login: userName was empty" );
      return false;
   }

   if( password.size() == 0 )
   {
      // should we boot this user for hacking? Or is it bad code?
      LogMessage( LOG_PRIO_ERR, "invalid attempt at login: password was empty" );
      return false;
   }

   // Before we add the user, let's verify that s/he isn't already logged in with a different connectionId. 
   if( IsUserInLoginHistory( userName, password, loginKey, gameProductId, connectionId, gatewayId ) )
   {
      return true;
   }
   else
   {
      return LoadUserAccount( userName, password, loginKey, gameProductId, connectionId, gatewayId );
   }

   return false;
}

//---------------------------------------------------------------

bool  DiplodocusLogin:: LoadUserAccount( const string& userName, const string& password, const string& loginKey, U8 gameProductId, U32 connectionId, U32 gatewayId )
{
   if( m_printFunctionNames )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   ConnectionToUser conn( userName, password, loginKey );
   conn.m_connectionDetails.gameProductId = gameProductId;
   conn.m_connectionDetails.connectionId = connectionId;
   conn.SetGatewayId( gatewayId );
   AddUserConnection( UserConnectionPair( connectionId, conn ) );

   return SetupQueryForLogin( userName, password, gameProductId, connectionId );
}

//---------------------------------------------------------------

bool  DiplodocusLogin:: SetupQueryForLogin( const string& userName, const string& password, U8 gameProductId, U32 connectionId )
{
   if( m_printFunctionNames )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   //*********************************************************************************
   // perhaps some validation here is in order like is this user valid based on the key
   //*********************************************************************************

   PacketDbQuery* dbQuery = new PacketDbQuery;
   dbQuery->id =           connectionId;
   dbQuery->lookup =       QueryType_UserLoginInfo;
   dbQuery->meta =         userName;
   dbQuery->serverLookup = gameProductId;

   dbQuery->query = "SELECT * FROM users JOIN user_profile ON users.user_id=user_profile.user_id WHERE users.user_email='%s' AND users.user_pw_hash='%s'";
   dbQuery->escapedStrings.insert( userName );
   dbQuery->escapedStrings.insert( password );

   return AddQueryToOutput( dbQuery );
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: HandleUserLoginResult( U32 connectionId, const PacketDbQueryResult* dbResult )
{
   if( m_printFunctionNames )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      connection->IncreaseLoginAttemptCount();
      if( dbResult->successfulQuery == false || dbResult->GetBucket().size() == 0 )
      {
         connection->ClearLoggingOutStatus();
         LogMessage( LOG_PRIO_ERR, "Connect user: Cannot continue logging in" );
         LogMessage( LOG_PRIO_ERR, "User record not found" );
         LogMessage( LOG_PRIO_ERR, "Name :%s ", connection->GetName().c_str() );
         LogMessage( LOG_PRIO_ERR, "Email :%s ", connection->GetEmail().c_str() );
         LogMessage( LOG_PRIO_ERR, "Uuid :%s ", connection->GetUuid().c_str() );
         LogMessage( LOG_PRIO_ERR, "PwdHash :%s ", connection->GetPasswordHash().c_str() );
         LogMessage( LOG_PRIO_ERR, "Id :%s ", connection->GetId().c_str() );
         return false;
      }
      // ---------------------------------
      // lots happening here
      connection->LoginResult( dbResult );
      // ---------------------------------

      if( connection->CanContinueLogginIn() == false )
      {
         connection->ClearLoggingOutStatus();
         LogMessage( LOG_PRIO_ERR, "Connect user: Cannot continue logging in" );
         return false;
      }

      if( dbResult->successfulQuery == true && dbResult->GetBucket().size() > 0 )
      {
         const bool isLoggedIn = true; 
         const bool wasDisconnectedByError = false;

         return BroadcastLoginStatus( connectionId, 
                                connection->m_connectionDetails.gameProductId, 
                                isLoggedIn, 
                                wasDisconnectedByError,
                                connection->GetGatewayId() );
         
         connection->ClearLoginAttemptCount();
      }
      else
      {
         LogMessage( LOG_PRIO_INFO, "User connection failure %s", connection->m_userName.c_str() );
      }
   }

   return false;
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: HandleAdminRequestUserProfile( U32 connectionId, const PacketDbQueryResult* dbResult )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      return connection->HandleAdminRequestUserProfile( dbResult );
   }

   return true;// i doesn't matter
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: LogUserOut( U32 connectionId, bool wasDisconnectedByError )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      return connection->BeginLogout( wasDisconnectedByError );
   }
   else
   {
      LogMessage( LOG_PRIO_INFO, "Attempt to log user out failed: user record not found: %d", connectionId );
      return false;
   }
   return true;
}

//---------------------------------------------------------------

void     DiplodocusLogin:: FinalizeLogout( U32 connectionId, bool wasDisconnectedByError )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      const bool isLoggedIn = false; 
      bool result = connection->FinalizeLogout();
      result = result;
      time_t currentTime;
      time( &currentTime );

      m_totalUserLoginSeconds += static_cast<int>( difftime( currentTime, connection->GetLoginTime() ) );
      m_totalNumLogouts ++; 

      SendLoginStatusToOtherServers( connection->m_userName, 
                                    connection->m_userUuid, 
                                    connectionId, 
                                    connection->m_connectionDetails.gameProductId, 
                                    connection->m_lastLoginTime, 
                                    connection->m_isActive, 
                                    connection->m_email, 
                                    connection->m_passwordHash, 
                                    connection->m_id, 
                                    connection->m_loginKey,
                                    connection->m_languageId, 
                                    isLoggedIn, 
                                    wasDisconnectedByError,
                                    connection->GetGatewayId() );
   }
}

//---------------------------------------------------------------
//---------------------------------------------------------------

bool  DiplodocusLogin:: IsUserConnectionValid( U32 connectionId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   //Threading::MutexLock locker( m_inputChainListMutex );
   UserConnectionMapConstIterator it = m_userConnectionMap.find( connectionId );
   if( it != m_userConnectionMap.end() )
      return true;
   return false;
}

ConnectionToUser*     DiplodocusLogin:: GetUserConnection( U32 connectionId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   //Threading::MutexLock locker( m_inputChainListMutex );
   UserConnectionMapIterator it = m_userConnectionMap.find( connectionId );
   if( it != m_userConnectionMap.end() )
   {
      return &( it->second );
   }
   return NULL;
}

void    DiplodocusLogin:: ReinsertUserConnection( int oldIndex, int newIndex )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   //Threading::MutexLock locker( m_inputChainListMutex );
    UserConnectionMapIterator it = m_userConnectionMap.find( oldIndex );
    if( it != m_userConnectionMap.end() )
    {
        m_userConnectionMap.insert( DiplodocusLogin:: UserConnectionPair( newIndex, it->second ) );
        m_userConnectionMap.erase( it );
    }
}

bool     DiplodocusLogin:: AddUserConnection( DiplodocusLogin:: UserConnectionPair pair )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   //Threading::MutexLock locker( m_inputChainListMutex );
   pair.second.SetManager( this );
   m_userConnectionMap.insert( pair );
   return true;
}

bool     DiplodocusLogin:: RemoveUserConnection( U32 connectionId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   //Threading::MutexLock locker( m_inputChainListMutex );
   UserConnectionMapIterator it = m_userConnectionMap.find( connectionId );
   if( it != m_userConnectionMap.end() )
   {
      m_userConnectionMap.erase( it );
      return true;
   }
   return false;
}

//---------------------------------------------------------------
//---------------------------------------------------------------

void     DiplodocusLogin:: RemoveOldConnections()
{
   Threading::MutexLock    locker( m_inputChainListMutex );
   UserConnectionMapIterator it = m_userConnectionMap.begin();
   time_t testTimer;
   time( &testTimer );

   while( it != m_userConnectionMap.end() )
   {
      UserConnectionMapIterator temp = it++;
      const ConnectionToUser& connection = temp->second;
      if( connection.IsReadyToBeCleanedUp() == true && // do not remove in the middle of logging out
         connection.m_loggedOutTime != 0 )
      {
      /*   if( m_printFunctionNames == true )
         {
            cout << "fn: " << __FUNCTION__ << endl;
         }*/
         
         if( difftime( testTimer, connection.m_loggedOutTime ) >= normalLogoutExpireTime )
         {
            FinalizeLogout( temp->first, false );
            m_userConnectionMap.erase( temp );
         }
      }
   }
}

//---------------------------------------------------------------

U32     DiplodocusLogin:: FindUserAlreadyInGame( const string& userName, U8 gameProductId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   //Threading::MutexLock locker( m_inputChainListMutex );

   UserConnectionMapIterator it = m_userConnectionMap.begin();
   while( it != m_userConnectionMap.end() )
   {
      UserConnectionPair pairObj = *it++;
      ConnectionToUser& conn = pairObj.second;
      if( conn.m_connectionDetails.gameProductId == gameProductId && // optimized for simplest test first
         
         ( conn.m_email == userName || conn.m_userName == userName ) )// we use these interchangably right now.
      {
         return pairObj.first;
      }
   }
   return 0;
}


//---------------------------------------------------------------
//---------------------------------------------------------------

void     DiplodocusLogin:: TellUserThatAccountAlreadyMatched( const CreateAccountResultsAggregator* aggregator )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   LogMessage( LOG_PRIO_ERR, "-----------------------------------" );
   LogMessage( LOG_PRIO_ERR, "Account create: Error... user eamil already in use" );
   LogMessage( LOG_PRIO_ERR, "Username: %s", aggregator->m_username.c_str() );
   LogMessage( LOG_PRIO_ERR, "Email:    %s", aggregator->m_useremail.c_str() );
   LogMessage( LOG_PRIO_ERR, "GK Hash:  %s", aggregator->m_gamekitHashId.c_str() );
   LogMessage( LOG_PRIO_ERR, "-----------------------------------" );

   if( aggregator->GetMatchingRecordType( CreateAccountResultsAggregator::MatchingRecord_Name ) )
   {
      SendErrorToClient( aggregator->GetConnectionId(), aggregator->GetGatewayId(), PacketErrorReport::ErrorType_CreateFailed_DuplicateUsername );  // E_NETWORK_DUPLICATE_USERNAME
      return;
   }
   if( aggregator->GetMatchingRecordType( CreateAccountResultsAggregator::MatchingRecord_Email ) )
   {
      SendErrorToClient( aggregator->GetConnectionId(), aggregator->GetGatewayId(), PacketErrorReport::ErrorType_CreateFailed_DuplicateEmail );  // E_NETWORK_DUPLICATE_USERNAME
      return;
   }
}

//---------------------------------------------------------------

void DiplodocusLogin:: UpdateUserAccount( const CreateAccountResultsAggregator* aggregator )
{
   return;
   
   /// unused function.. I kept it for reference and 

   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   U32 user_id = 0;
   if( aggregator->m_userRecordMatchingGKHash != 0 )
   {
      user_id = aggregator->m_userRecordMatchingGKHash;
   }
   else if( aggregator->m_userRecordMatchingEmail != 0 )
   {
      user_id = aggregator->m_userRecordMatchingEmail;
   }
   else
   {
      assert( 0 );
   }

   // Rule #1 
   // if the GK Hash matches but the email does not, then this may be two users sharing the same device. Zero out the
   // GK hash, and then create the new account.

   // If the gk hash matches but the username or email do not match, then zero out the storage of the gk
   // hash and create the new pending account
   if( aggregator->IsMatching_GKHashRecord_DifferentFrom_UserEmail( aggregator->m_useremail ) )
   {
     /* bool  wasUpdated = false;
      if( aggregator->m_userRecordMatchingGKHash != 0 )
      {
         string query = "UPDATE users SET user_gamekit_hash='0', active='1' WHERE user_id='";
         query += boost::lexical_cast< string> ( user_id );
         query += "'";
         PacketDbQuery* dbQuery = new PacketDbQuery;
         dbQuery->id =           aggregator->m_connectionId;
         dbQuery->lookup =       QueryType_UpdateUseraccount;
         dbQuery->meta =         aggregator->m_username;
         dbQuery->serverLookup = aggregator->m_gameProductId;
         dbQuery->isFireAndForget = true;
         dbQuery->query = query;
         AddQueryToOutput( dbQuery );
         wasUpdated = true;
      }


      // now use all of this info to create a new user record and reset the game_kit_hash
      if( aggregator->m_numPendingUserRecordsMatching == 0 )//  aggregator->ShouldUpdatePendingUserRecord() == false ) // we may already have this user update pending...
      {
         bool setGkHashToNull = true;
         CreateNewPendingUserAccount( aggregator, setGkHashToNull );
      }
      else if( wasUpdated )
      {
         SendErrorToClient( aggregator->m_connectionId, PacketErrorReport::ErrorType_CreateAccount_AccountUpdated );
      }
      return;*/

      //aggregator->m_gamekitHashId.clear();
      bool setGkHashToNull = true;
      CreateNewPendingUserAccount( aggregator, setGkHashToNull );
   }

/*
   // Rule #2
   // user is probably updating his user name.
   
   string query = "UPDATE users SET user_name='%s', user_name_match='%s', user_pw_hash='%s', user_email='%s', user_gamekit_hash='%s', active='1' WHERE user_id=";

   query += boost::lexical_cast< string> ( user_id );

   PacketDbQuery* dbQuery = new PacketDbQuery;
   dbQuery->id =           aggregator->m_connectionId;
   dbQuery->lookup =       QueryType_UpdateUseraccount;
   dbQuery->meta =         aggregator->m_username;
   dbQuery->serverLookup = aggregator->m_gameProductId;
   dbQuery->isFireAndForget = true;

   std::string lowercase_username = aggregator->m_username; 
   std::transform( lowercase_username.begin(), lowercase_username.end(), lowercase_username.begin(), ::tolower );

   dbQuery->query = query;
   dbQuery->escapedStrings.insert( aggregator->m_username );
   dbQuery->escapedStrings.insert( lowercase_username );
   dbQuery->escapedStrings.insert( aggregator->m_password );
   dbQuery->escapedStrings.insert( aggregator->m_useremail );
   dbQuery->escapedStrings.insert( aggregator->m_gamekitHashId );
   AddQueryToOutput( dbQuery );

   SendErrorToClient( aggregator->m_connectionId, PacketErrorReport::ErrorType_CreateAccount_AccountUpdated );
   */
}

//---------------------------------------------------------------

void     DiplodocusLogin:: CreateNewPendingUserAccount( const CreateAccountResultsAggregator* aggregator, bool setGameKitHashToNUll )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   LogMessage( LOG_PRIO_INFO, "-----------------------------------" );
   LogMessage( LOG_PRIO_INFO, "Account create: Create pending user account" );
   LogMessage( LOG_PRIO_INFO, "Username: ", aggregator->m_username.c_str() );
   LogMessage( LOG_PRIO_INFO, "Email:    ", aggregator->m_useremail.c_str() );
   LogMessage( LOG_PRIO_INFO, "GK Hash:  ", aggregator->m_gamekitHashId.c_str() );
   LogMessage( LOG_PRIO_INFO, "-----------------------------------" );

   string query = "INSERT INTO user_temp_new_user (user_name, user_name_match, user_pw_hash, user_email, user_gamekit_hash, game_id, language_id) "
                                 "VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s')";

   PacketDbQuery* dbQuery = new PacketDbQuery;
   dbQuery->id =           aggregator->m_connectionId;
   dbQuery->lookup =       QueryType_CreateUseraccount;
   dbQuery->meta =         aggregator->m_username;
   dbQuery->serverLookup = aggregator->m_gameProductId;
   dbQuery->isFireAndForget = true;

   std::string lowercase_username = aggregator->m_username; 
   std::transform( lowercase_username.begin(), lowercase_username.end(), lowercase_username.begin(), ::tolower );

   dbQuery->query = query;
   dbQuery->escapedStrings.insert( aggregator->m_username );
   dbQuery->escapedStrings.insert( lowercase_username );
   dbQuery->escapedStrings.insert( aggregator->m_password );
   dbQuery->escapedStrings.insert( aggregator->m_useremail );
   string gameKitHash( "0" );
   if( setGameKitHashToNUll == false )
   {
      gameKitHash  = aggregator->m_gamekitHashId;
   }

   dbQuery->escapedStrings.insert( gameKitHash );
   dbQuery->escapedStrings.insert( boost::lexical_cast< string >( static_cast< int > ( aggregator->m_gameProductId ) ) );
   dbQuery->escapedStrings.insert( boost::lexical_cast< string >( static_cast< int > ( aggregator->m_languageId ) ) );
   AddQueryToOutput( dbQuery );

   SendErrorToClient( aggregator->m_connectionId, aggregator->GetGatewayId(), PacketErrorReport::ErrorType_CreateAccount_Success );
}

//---------------------------------------------------------------

void     DiplodocusLogin:: UpdatePendingUserRecord( const CreateAccountResultsAggregator* aggregator )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   LogMessage( LOG_PRIO_INFO, "-----------------------------------" );
   LogMessage( LOG_PRIO_INFO, "Account create: Update pending user account" );
   LogMessage( LOG_PRIO_INFO, "Username: ", aggregator->m_username.c_str() );
   LogMessage( LOG_PRIO_INFO, "Email:    ", aggregator->m_useremail.c_str() );
   LogMessage( LOG_PRIO_INFO, "GK Hash:  ", aggregator->m_gamekitHashId.c_str() );
   LogMessage( LOG_PRIO_INFO, "-----------------------------------" );

   string query = "UPDATE user_temp_new_user SET user_name='%s', user_name_match='%s', "
         "user_pw_hash='%s', user_email='%s', user_gamekit_hash='%s', game_id='%s', "
         "language_id='%s', was_email_sent='0', lookup_key=NULL, flagged_as_invalid='0' WHERE id='";
   if( aggregator->m_pendingUserRecordMatchingGKHash != 0 )
      query += boost::lexical_cast< U32 >( aggregator->m_pendingUserRecordMatchingGKHash );
   else if( aggregator->m_pendingUserRecordMatchingEmail != 0 )
      query += boost::lexical_cast< U32 >( aggregator->m_pendingUserRecordMatchingEmail );
   else if( aggregator->m_pendingUserRecordMatchingName !=0 )
      query += boost::lexical_cast< U32 >( aggregator->m_pendingUserRecordMatchingName );
   else 
   {
      LogMessage( LOG_PRIO_ERR, "Bad update user record in user account creation" );
      assert( 0 );
   }
   query += "'";

   PacketDbQuery* dbQuery = new PacketDbQuery;
   dbQuery->id =           aggregator->m_connectionId;
   dbQuery->lookup =       QueryType_CreateUseraccount;
   dbQuery->meta =         aggregator->m_username;
   dbQuery->serverLookup = aggregator->m_gameProductId;
   dbQuery->isFireAndForget = true;

   std::string lowercase_username = aggregator->m_username; 
   std::transform( lowercase_username.begin(), lowercase_username.end(), lowercase_username.begin(), ::tolower );

   dbQuery->query = query;

   dbQuery->escapedStrings.insert( aggregator->m_username );
   dbQuery->escapedStrings.insert( lowercase_username );
   dbQuery->escapedStrings.insert( aggregator->m_password );
   dbQuery->escapedStrings.insert( aggregator->m_useremail );
   dbQuery->escapedStrings.insert( aggregator->m_gamekitHashId );
   dbQuery->escapedStrings.insert( boost::lexical_cast< string >( static_cast< int > ( aggregator->m_gameProductId ) ) );
   dbQuery->escapedStrings.insert( boost::lexical_cast< string >( static_cast< int > ( aggregator->m_languageId ) ) );

   AddQueryToOutput( dbQuery );

   SendErrorToClient( aggregator->m_connectionId, aggregator->GetGatewayId(), PacketErrorReport::ErrorType_CreateAccount_AccountUpdated );
}

//---------------------------------------------------------------

void     DiplodocusLogin:: CreateNewUserAccount( const CreateAccountResultsAggregator* aggregator, bool setGkHashTo0 )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   U32 hash = static_cast<U32>( GenerateUniqueHash( boost::lexical_cast< string >( aggregator->GetConnectionId() ) + aggregator->m_useremail ) );

   string newUuid = GenerateUUID( GetCurrentMilliseconds() + hash );

   string query = "INSERT INTO users (user_name, user_name_match, user_pw_hash, user_email, user_gamekit_hash, active, language_id, uuid ) "
                                 "VALUES ('%s', '%s', '%s', '%s', '%s', '1', '%s','";
   query += newUuid;
   query += "')";

   PacketDbQuery* dbQuery = new PacketDbQuery;
   dbQuery->id =           aggregator->m_connectionId;
   dbQuery->lookup =       QueryType_CreateUseraccount;
   dbQuery->meta =         aggregator->m_username;
   dbQuery->serverLookup = aggregator->m_gameProductId;
   dbQuery->isFireAndForget = true;

   std::string lowercase_username = aggregator->m_username; 
   std::transform( lowercase_username.begin(), lowercase_username.end(), lowercase_username.begin(), ::tolower );

   dbQuery->query = query;
   dbQuery->escapedStrings.insert( aggregator->m_username );
   dbQuery->escapedStrings.insert( lowercase_username );
   dbQuery->escapedStrings.insert( aggregator->m_password );
   dbQuery->escapedStrings.insert( aggregator->m_useremail );

   string gkHash( "0" );
   if( setGkHashTo0 == false )
   {
      gkHash = aggregator->m_gamekitHashId;
   }
   dbQuery->escapedStrings.insert( gkHash );
   dbQuery->escapedStrings.insert( boost::lexical_cast< string >( static_cast< int > ( aggregator->m_languageId ) ) );
   dbQuery->escapedStrings.insert( boost::lexical_cast< string >( static_cast< int > ( aggregator->m_gameProductId ) ) );

   AddQueryToOutput( dbQuery );

   SendErrorToClient( aggregator->m_connectionId, aggregator->GetGatewayId(), PacketErrorReport::ErrorType_CreateAccount_Success );
}

//---------------------------------------------------------------

bool        DiplodocusLogin:: CreateUserAccount( U32 connectionId, U32 gatewayId, const string& email, const string& password, const string& userName, const string& deviceAccountId, const string& deviceId, U8 languageId, U8 gameProductId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   if( IsUserConnectionValid( connectionId ) )
   {
      string str = "ERROR: Login server, user attempted to create a second account while logged in, userName: ";
      str += userName;
      str += ", email: ";
      str += email;
      LogMessage( LOG_PRIO_ERR, str.c_str() );

      LogMessage( LOG_PRIO_ERR, "--- shutting down user" );

      ForceUserLogoutAndBlock( connectionId, gatewayId );
      return false;
   }

   string trimmedPassword = Trim( password );
   std::string trimmedUsername = Trim( userName );
   std::string lowercase_username = Trim( userName );
   std::string lowercase_useremail = Trim( email );

   if (trimmedPassword.size() < 6)
   {
      SendErrorToClient( connectionId, gatewayId, PacketErrorReport::ErrorType_CreateFailed_BadPassword );
      return false;
   }
   if (lowercase_useremail.size() < 3) // currently user name can be null
   {
      SendErrorToClient( connectionId, gatewayId, PacketErrorReport::ErrorType_CreateFailed_DisallowedUsername );
      return false;
   }

   //bool  isExpectingGkHash = false;
   U64 gameKitHash = 0;
   if( deviceAccountId.size() != 0 )
   {
      //isExpectingGkHash = true;
      ConvertFromString( deviceAccountId, gameKitHash );
   }
   LogMessage(LOG_PRIO_INFO, "        email=%s, GC ID=%llu\n", email.c_str(), gameKitHash );

   std::transform( lowercase_username.begin(), lowercase_username.end(), lowercase_username.begin(), ::tolower ); 
   std::transform( lowercase_useremail.begin(), lowercase_useremail.end(), lowercase_useremail.begin(), ::tolower );   

   std::size_t found = lowercase_username.find( "playdek" );
   if (found!=std::string::npos)
   {
      SendErrorToClient( connectionId, gatewayId, PacketErrorReport::ErrorType_CreateFailed_DisallowedUsername );
      return false;
   }

   CreateAccountResultsAggregator* aggregator = new CreateAccountResultsAggregator(connectionId, gatewayId, lowercase_useremail, trimmedPassword, trimmedUsername, deviceAccountId, deviceId, languageId, gameProductId);
   //aggregator->isExpectingGkHash = isExpectingGkHash;

   LockMutex();
   m_userAccountCreationMap.insert( UserCreateAccountPair( connectionId, aggregator ) );
   UnlockMutex();

   U64 passwordHash = 0;
   ConvertFromString( trimmedPassword, passwordHash);
   
 //  string gkHashLookup =         "SELECT id FROM users WHERE user_gamekit_hash='%s'";
   string queryInvalidUserName = "SELECT id FROM invalid_username WHERE user_name_match='%s'";
   string queryUsers =           "SELECT * FROM users WHERE user_name='%s' OR user_email='%s'";
   if( deviceAccountId.size() > 0 ) queryUsers  += " OR user_gamekit_hash='%s'";
   string queryTempUsers =       "SELECT * FROM user_temp_new_user WHERE user_name='%s' OR user_email='%s'";
   if( deviceAccountId.size() > 0 ) queryTempUsers += " OR user_gamekit_hash='%s'";

   PacketDbQuery* dbQuery = new PacketDbQuery;
   dbQuery->id =           connectionId;
   dbQuery->lookup =       QueryType_LookupUserNameForInvalidName;
   dbQuery->meta =         userName;
   dbQuery->serverLookup = gameProductId;

   dbQuery->query = queryInvalidUserName;
   dbQuery->escapedStrings.insert( userName );
   AddQueryToOutput( dbQuery );

   dbQuery = new PacketDbQuery;
   dbQuery->id =           connectionId;
   dbQuery->lookup =       QueryType_LookupUserByUsernameOrEmail;
   dbQuery->meta =         userName;
   dbQuery->serverLookup = gameProductId;

   dbQuery->query = queryUsers;
   dbQuery->escapedStrings.insert( userName );
   dbQuery->escapedStrings.insert( email );
   dbQuery->escapedStrings.insert( deviceAccountId );
   AddQueryToOutput( dbQuery );

   dbQuery = new PacketDbQuery;
   dbQuery->id =           connectionId;
   dbQuery->lookup =       QueryType_LookupTempUserByUsernameOrEmail;
   dbQuery->meta =         userName;
   dbQuery->serverLookup = gameProductId;

   dbQuery->query = queryTempUsers;
   dbQuery->escapedStrings.insert( userName );
   dbQuery->escapedStrings.insert( email );
   dbQuery->escapedStrings.insert( deviceAccountId );
   AddQueryToOutput( dbQuery );

 /*  if( isExpectingGkHash )
   {
      dbQuery = new PacketDbQuery;
      dbQuery->id =           connectionId;
      dbQuery->lookup =       QueryType_LookupUserByGkHash;
      dbQuery->meta =         userName;
      dbQuery->serverLookup = gameProductId;

      dbQuery->query = gkHashLookup;
      dbQuery->escapedStrings.insert( deviceAccountId );
      AddQueryToOutput( dbQuery );   
   }*/
   
   return false;

}

//---------------------------------------------------------------

int         DiplodocusLogin:: FindProductByName( const string& name )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   std::string lowercase_productUUID = name; 
   std::transform( lowercase_productUUID.begin(), lowercase_productUUID.end(), lowercase_productUUID.begin(), ::tolower );

   ProductList::iterator it = m_productList.begin();
   while( it != m_productList.end() )
   {
      if( it->vendorUuid == name )   // NOTE: these names may vary
      {
	      int index = static_cast< int >( it - m_productList.begin() );
         return index;
      }
      it++;
   }
   return DiplodocusLogin::ProductNotFound;
}


bool        DiplodocusLogin:: FindProductByLookupName( const string& lookupName, ProductInfo& productDefn )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ProductList::iterator it = m_productList.begin();
   while( it != m_productList.end() )
   {
      if( it->lookupName == lookupName )   // NOTE: these names may vary
      {
	      productDefn = *it;
         return true;
      }
      it++;
   }
   return false;
}

bool        DiplodocusLogin:: FindProductByUuid( const string& uuid, ProductInfo& returnPi  )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   vector< ProductInfo >::iterator it = m_productList.begin();
   while( it != m_productList.end() )
   {
      if( it->uuid == uuid )
      {
         returnPi = *it;
         return true;
      }
      it++;
   }

   return false;
}

int        DiplodocusLogin:: FindProductByVendorUuid( const string& vendorUuid )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   std::string lowercase_productUUID = vendorUuid; 
   std::transform( lowercase_productUUID.begin(), lowercase_productUUID.end(), lowercase_productUUID.begin(), ::tolower );

   vector< ProductInfo >::iterator it = m_productList.begin();
   while( it != m_productList.end() )
   {
      if( it->vendorUuid == lowercase_productUUID )
      {
         int index = static_cast< int >( it - m_productList.begin() );
         return index;
      }
      it++;
   }

   return DiplodocusLogin::ProductNotFound;
}

bool        DiplodocusLogin:: GetProductByIndex( int index, ProductInfo& returnPi )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   if( index > (int) m_productList.size() )
   {
      return false;
   }
   returnPi = m_productList[ index] ;
   return true;
}

bool        DiplodocusLogin:: GetProductByProductId( int productId, ProductInfo& returnPi  )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   vector< ProductInfo >::iterator it = m_productList.begin();
   while( it != m_productList.end() )
   {
      if( it->productId == productId )
      {
         returnPi = *it;
         return true;
      }
      it++;
   }

   return false;
}

//---------------------------------------------------------------

// how can we trust these values?
bool        DiplodocusLogin:: StoreUserPurchases( U32 connectionId, const PacketListOfUserAggregatePurchases* deviceReportedPurchases )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   //Threading::MutexLock locker( m_inputChainListMutex );
   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL )
   {
      LogMessage( LOG_PRIO_ERR, "Error: could not find user by connection id: %d", connectionId );
      return false;
   }

   if( connection->StoreUserPurchases( deviceReportedPurchases ) == false )
      return false;

   if( connection->productsWaitingForInsertionToDb.size() == 0 )
   {
      SendListOfUserProductsToAssetServer( connectionId );
      // SendListOfUserProductsToOtherServers
   }
   return false;
}

//---------------------------------------------------------------

bool   DiplodocusLogin:: RequestListOfPurchases( U32 connectionId, const PacketListOfUserPurchasesRequest* purchase )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      LogMessage( LOG_PRIO_ERR, "Login server.RequestListOfPurchases: major problem logged in user" );
      return false;
   }

   return connection->HandleRequestForListOfPurchases( purchase );

}

//---------------------------------------------------------------

bool   DiplodocusLogin:: RequestListOfPurchasesUpdate( const PacketListOfUserPurchasesUpdated* userInventory )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( userInventory->userConnectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      LogMessage( LOG_PRIO_ERR, "Login server.RequestListOfPurchases: major problem logged in user" );
      return false;
   }

   return connection->RequestListOfPurchases( userInventory->userUuid );
}

//---------------------------------------------------------------

bool  DiplodocusLogin:: LogoutListOfUsers( const PacketLogin_LogoutAllUsers* usersLogout, U32 gatewayId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   int num = usersLogout->connectionIds.size();
   for( int i=0; i<num; i++ )
   {
      U32 connectionId = usersLogout->connectionIds[i];
      ForceUserLogoutAndBlock( connectionId, gatewayId );
   }
   return true;
}

//---------------------------------------------------------------

bool   DiplodocusLogin:: AddPurchase( U32 connectionId, const PacketAddPurchaseEntry* purchase )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      LogMessage( LOG_PRIO_ERR, "Login server.AddPurchase: major problem logged in user" );
      return false;
   }

   return connection->AddPurchase( purchase );

}

//---------------------------------------------------------------

bool     DiplodocusLogin:: RequestProfile( U32 connectionId, const PacketRequestUserProfile* profileRequest )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      LogMessage( LOG_PRIO_ERR, "Login server.RequestProfile: major problem logged in user" );
      return false;
   }

   return connection->RequestProfile( profileRequest );
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: UpdateProfile( U32 connectionId, const PacketUpdateUserProfile* profileRequest )
{   
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      LogMessage( LOG_PRIO_ERR, "Login server.UpdateProfile: major problem logged in user" );
      return false;
   }

   return connection->UpdateProfile( profileRequest );
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: UpdateProfile( U32 connectionId, const PacketUpdateSelfProfile* profileRequest )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      LogMessage( LOG_PRIO_ERR, "Login server.UpdateProfile: major problem logged in user" );
      return false;
   }

   return connection->UpdateProfile( profileRequest );
}

//---------------------------------------------------------------

void  DiplodocusLogin:: PackageProductToSendToClient( const ProductInfo& pi, ProductBriefPacketed& brief, U32 languageId )
{
   string name = pi.name;
   if( pi.lookupName.size() != 0 )
   {
      name = GetStringLookup()->GetString( pi.lookupName, languageId );
   }
   brief.uuid =            pi.uuid;
   brief.vendorUuid =      pi.vendorUuid;
   brief.localizedName =   name;

   /* if( pi.productType != GameProductType_Game && 
   pi.productType != GameProductType_Dlc && 
   pi.productType != GameProductType_Consumable )
   continue;*/

   if( pi.parentId )
   {
      ProductInfo parent;
      GetProductByProductId( pi.parentId, parent );// reuse this local variable
      brief.parentUuid = parent.uuid;
   }
   brief.iconName = pi.iconName;
   brief.productType = pi.productType;
}

int      DiplodocusLogin:: CountNumOfAvailableProducts()
{
   int count = 0;
   ProductList::iterator it = m_productList.begin();
   while( it != m_productList.end() )
   {
      const ProductInfo& pi = *it ++;
      if( pi.isHidden == true )
         continue;

      count ++;
   }
   return count;
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: HandleRequestListOfProducts( U32 connectionId, PacketRequestListOfProducts* purchaseRequest )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      LogMessage( LOG_PRIO_ERR, "Login server.HandleRequestListOfProducts: major problem logged in user" );
      return false;
   }

   U32 languageId = connection->m_languageId;

   int numProductsPerPacket = 15;
   PacketRequestListOfProductsResponse* response = new PacketRequestListOfProductsResponse();
   response->platformId = purchaseRequest->platformId;
   
   int totalCount = CountNumOfAvailableProducts();

   response->products.SetIndexParams( 0, totalCount );
   int   numPacketsSent = 0;

   ProductList::iterator it = m_productList.begin();
   while( it != m_productList.end() )
   {
      const ProductInfo& pi = *it ++;
      if( pi.isHidden == true )
         continue;

      ProductBriefPacketed    brief;
      PackageProductToSendToClient( pi, brief, languageId );

      response->products.push_back( brief );
      if( response->products.size() == numProductsPerPacket )
      {
         numPacketsSent++;
         SendPacketToGateway( response, connectionId, connection->GetGatewayId() );
         if( numPacketsSent* numProductsPerPacket < totalCount )
         {
            response = new PacketRequestListOfProductsResponse();
            response->platformId = purchaseRequest->platformId;
            response->products.SetIndexParams( numPacketsSent* numProductsPerPacket, totalCount );
         }
         else
         {
            response = NULL;
            break;
         }
      }
   }

   if( response != NULL )
   {
      SendPacketToGateway( response, connectionId, connection->GetGatewayId() );
   }
   return true;
}

//---------------------------------------------------------------

bool  DiplodocusLogin:: RequestOthersProfile( U32 connectionId, const PacketRequestOtherUserProfile* profileRequest )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
     LogMessage( LOG_PRIO_ERR, "Login server.RequestProfile: major problem logged in user" );
      return false;
   }

   return connection->RequestOthersProfile( profileRequest );
}

//---------------------------------------------------------------

bool  DiplodocusLogin::ThrottleUser( U32 userConnectionId, const PacketLoginDebugThrottleUserPackets* throttleRequest )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( userConnectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      LogMessage( LOG_PRIO_ERR, "Login server.ThrottleUser: major problem logged in user" );
      return false;
   }

   int delay = throttleRequest->level;
   if( delay > 20 )
      delay = 20;// arbitrary value equal to 10 seconds.
   delay *= 500; // milliseconds;
   PacketLoginThrottlePackets* throttler = new PacketLoginThrottlePackets;
   throttler->delayBetweenPacketsMs = delay;
   return SendPacketToGateway( throttler, userConnectionId, connection->GetGatewayId() );
}

//---------------------------------------------------------------

bool     DiplodocusLogin::EchoHandler( U32 userConnectionId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( userConnectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      //Log( "Login server.RequestProfile: major problem logged in user", 4 );
      LogMessage( LOG_PRIO_ERR, "echo failed to find user" );
      return false;
   }

   return connection->EchoHandler();
}


//---------------------------------------------------------------

ConnectionToUser*     DiplodocusLogin:: GetLoadedUserConnectionByUuid( const string & uuid )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   //Threading::MutexLock locker( m_inputChainListMutex );
   UserConnectionMapIterator it = m_userConnectionMap.begin();
   while( it != m_userConnectionMap.end() )
   {
      if( it->second.m_userUuid == uuid )
         return &it->second;
      it++;
   }

   return NULL;
}; 

//---------------------------------------------------------------

bool   DiplodocusLogin:: HandleCheats( U32 connectionId, const PacketCheat* cheat )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL || connection->status != ConnectionToUser::LoginStatus_LoggedIn )
   {
      LogMessage( LOG_PRIO_ERR, "Login server: major problem with cheats... user not set properly" );
      SendErrorToClient( connectionId, connection->GetGatewayId(), PacketErrorReport::ErrorType_Cheat_BadPermissions );
      return false;
   }
   
   // 
   return connection->HandleCheats( cheat );
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: UpdateProductFilterName( int index, string newVendorUuid )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   std::transform( newVendorUuid.begin(), newVendorUuid.end(), newVendorUuid.begin(), ::tolower );

   m_productList[ index ].vendorUuid = newVendorUuid;

   PacketDbQuery* dbQuery = new PacketDbQuery;
   dbQuery->id =              0 ;
   dbQuery->lookup =          QueryType_UpdateProductFileInfo;
   dbQuery->isFireAndForget = true;// no result is needed

   string queryString = "UPDATE product SET filter_name='%s' WHERE uuid='%s'";
   dbQuery->escapedStrings.insert( newVendorUuid );
   dbQuery->escapedStrings.insert( m_productList[ index ].uuid );
   dbQuery->query =           queryString;

   return AddQueryToOutput( dbQuery );
}

//---------------------------------------------------------------

bool  DiplodocusLogin:: ForceUserLogoutAndBlock( U32 connectionId, U32 gatewayId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }
   
   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      SendErrorToClient( connectionId, gatewayId, PacketErrorReport::ErrorType_UserBadLogin );
   }
   string                     userName;
   string                     uuid;
   string                     lastLoginTime;
   string                     email;
   bool                       active = false;
   string                     passwordHash = "0";
   string                     userId = "0";
   string                     loginKey = "";
   int                        languageId = 1;
   U8 gameProductId = 0;

   if( connection )
   {
      userName =              connection->m_userName;
      uuid =                  connection->m_userUuid;
      lastLoginTime =         connection->m_lastLoginTime;
      connection->status =    ConnectionToUser::LoginStatus_Invalid;
      active =                connection->m_isActive;
      passwordHash =          connection->m_passwordHash;
      userId =                connection->m_id;
      email =                 connection->m_email;
      gameProductId =         connection->m_connectionDetails.gameProductId;
      loginKey =              connection->m_loginKey;
      languageId =            connection->m_languageId;
   }

   // now disconnect him/her
   PacketLoginToGateway* loginStatus = new PacketLoginToGateway();
   loginStatus->userName = userName;
   loginStatus->userEmail = email;
   loginStatus->uuid = uuid;
   loginStatus->lastLogoutTime = GetDateInUTC();
   loginStatus->loginKey = loginKey;
   loginStatus->languageId = languageId;

   loginStatus->wasLoginSuccessful = false;
   loginStatus->adminLevel = 0;
   loginStatus->gameProductId = gameProductId;
   
   SendPacketToGateway( loginStatus, connectionId, gatewayId );
   const bool isLoggedIn = false; 
   const bool wasDisconnectedByError = false;

   
   if( connection )
   {
      time_t currentTime;
      time( &currentTime );
      connection->SetLoggedOutTime( currentTime );
   }

   SendLoginStatusToOtherServers( userName, uuid, connectionId, gameProductId, lastLoginTime, active, email, passwordHash, userId, loginKey, languageId, isLoggedIn, wasDisconnectedByError,
                                             gatewayId );

   return true;
}

//------------------------------------------------------------------------------------------------

bool  DiplodocusLogin:: SendListOfOwnedGamesToGameServers( U32 connectionId, const KeyValueVector& kvArray )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   if( IsUserConnectionValid( connectionId ) ) // user may have disconnected waiting for the db.
   {
      BaseOutputContainer::iterator itOutputs = m_listOfOutputs.begin();
      while( itOutputs != m_listOfOutputs.end() )
      {
         ChainType* outputPtr = static_cast< ChainType*> ( (*itOutputs).m_interface );

         PacketListOfGames* packetToSend = new PacketListOfGames;

         KeyValueVector::const_iterator it = kvArray.begin();
         while( it != kvArray.end() )
         {
            packetToSend->games.insert( it->key, it->value );
            it++;
         }
         //packetToSend->games = kvArray;// potentially costly.
         packetToSend->connectionId = connectionId;

         if( outputPtr->AddOutputChainData( packetToSend, m_chainId ) == false )
         {
            delete packetToSend;
         }
         itOutputs++;
      }
   }

   return false;
}

//------------------------------------------------------------------------------------------------

bool  DiplodocusLogin:: UpdateLastLoggedInTime( U32 connectionId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      return connection->UpdateLastLoggedInTime();
   }
   return false;
}


//------------------------------------------------------------------------------------------------

bool  DiplodocusLogin:: UpdateLastLoggedOutTime( U32 connectionId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      return connection->UpdateLastLoggedOutTime();
   }

   return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
/*
bool     DiplodocusLogin:: HandleUserProfileFromDb( U32 connectionId, PacketDbQueryResult* dbResult )
{
   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection == NULL )
   {
      Log( "Login server: major problem with successful login", 4 );
      return false;
   }

   if( connection->HandleUserProfileFromDb( dbResult ) == true )
   {
      string   userName =        connection->userName;
      string   userUuid =        connection->userUuid;
      string   email =           connection->email;
      string   lastLoginTime =   connection->lastLoginTime;
      bool     active =          connection->isActive;
      string   passwordHash =    connection->passwordHash;
      string   userId =          connection->id;
      U8       gameProductId =   connection->gameProductId;
      string   loginKey =        connection->loginKey;

      return SendLoginStatusToOtherServers( userName, userUuid, connectionId, gameProductId, lastLoginTime, active, email, passwordHash, userId, loginKey, true, false );
   }

   else return false;
}*/

//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
bool  DiplodocusLogin:: SendLoginStatus(  ChainType*  destinationServerPtr,
                                          const string& userName, 
                                          const string& userUuid, 
                                          U32 connectionId, 
                                          U8 gameProductId, 
                                          const string& lastLoginTime, 
                                          bool isActive, 
                                          const string& email, 
                                          const string& passwordHash, 
                                          const string& userId, 
                                          const string& loginKey,
                                          int languageId, 
                                          bool isLoggedIn, 
                                          bool wasDisconnectedByError,
                                          U32 gatewayId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   if( destinationServerPtr == NULL )
      return false;

   BasePacket* packetToSend = NULL;
   if( isLoggedIn )
   {
      PacketPrepareForUserLogin* prepareForUser = new PacketPrepareForUserLogin;
      prepareForUser->connectionId = connectionId;
      prepareForUser->userName = userName;
      prepareForUser->uuid = userUuid;
      prepareForUser->lastLoginTime = lastLoginTime;
      prepareForUser->gameProductId = gameProductId;

      prepareForUser->active = isActive;
      prepareForUser->email= email;
      string tempUserId = userId;
      if( userId.size() == 0 )
         tempUserId = "0";
      prepareForUser->userId = boost::lexical_cast<U32>( tempUserId );
      prepareForUser->password = passwordHash;
      prepareForUser->loginKey = loginKey;
      prepareForUser->languageId = languageId;
      prepareForUser->gatewayId = gatewayId;

      packetToSend = prepareForUser;
      
   }
   else
   {
      PacketPrepareForUserLogout* logout = new PacketPrepareForUserLogout;
      logout->uuid = userUuid;
      logout->connectionId = connectionId;
      logout->wasDisconnectedByError = wasDisconnectedByError;

      packetToSend = logout;
   }

   if( destinationServerPtr->AddOutputChainData( packetToSend, m_chainId ) == false )
   {
      delete packetToSend;
   }

   return true;
}

//---------------------------------------------------------------

bool  DiplodocusLogin:: SendLoginStatusToOtherServers( const string& userName, 
                                                     const string& userUuid, 
                                                     U32 connectionId, 
                                                     U8 gameProductId, 
                                                     const string& lastLoginTime, 
                                                     bool isActive, 
                                                     const string& email, 
                                                     const string& passwordHash, 
                                                     const string& userId, 
                                                     const string& loginKey,
                                                     int languageId, 
                                                     bool isLoggedIn, 
                                                     bool wasDisconnectedByError,
                                                     U32 gatewayId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   // send this to every other listening server
   BaseOutputContainer::iterator itOutputs = m_listOfOutputs.begin();
   //cout << "SendLoginStatusToOtherServers" << endl;
   while( itOutputs != m_listOfOutputs.end() )
   {
      ChainType*  outputPtr = static_cast< ChainType*> ( (*itOutputs).m_interface );
      itOutputs++;

      if( outputPtr->GetChainedType() != ChainedType_OutboundSocketConnector )
      {
         continue;
      }
      SendLoginStatus( outputPtr, 
                        userName, 
                       userUuid, 
                       connectionId, 
                       gameProductId, 
                       lastLoginTime, 
                       isActive, 
                       email, 
                       passwordHash, 
                       userId, 
                       loginKey,
                       languageId, 
                       isLoggedIn, 
                       wasDisconnectedByError,
                       gatewayId );
   }

   if( isLoggedIn )
   {
      m_numSuccessfulLogins++;
   }

   return true;
}

//---------------------------------------------------------------

bool  DiplodocusLogin:: SendLoginStatusTo_Non_GameServers( const string& userName, 
                                                     const string& userUuid, 
                                                     U32 connectionId, 
                                                     U8 gameProductId, 
                                                     const string& lastLoginTime, 
                                                     bool isActive, 
                                                     const string& email, 
                                                     const string& passwordHash, 
                                                     const string& userId, 
                                                     const string& loginKey,
                                                     int languageId, 
                                                     bool isLoggedIn, 
                                                     bool wasDisconnectedByError,
                                                     U32 gatewayId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   // send this to every other listening server
   BaseOutputContainer::iterator itOutputs = m_listOfOutputs.begin();
   LogMessage( LOG_PRIO_ERR, "SendLoginStatusTo_Non_GameServers for user: %s", userName.c_str() );
   while( itOutputs != m_listOfOutputs.end() )
   {
      ChainType*  outputPtr = static_cast< ChainType*> ( (*itOutputs).m_interface );
      itOutputs++;

      //cout << outputPtr->GetClassName() << endl;
      if( outputPtr->GetChainedType() != ChainedType_OutboundSocketConnector )
      {
         continue;
      }
      FruitadensLogin* login = static_cast< FruitadensLogin* >( outputPtr );
      if( login->IsGameServer() == true )
      {
         continue;
      }

      SendLoginStatus( outputPtr, 
                        userName, 
                       userUuid, 
                       connectionId, 
                       gameProductId, 
                       lastLoginTime, 
                       isActive, 
                       email, 
                       passwordHash, 
                       userId, 
                       loginKey,
                       languageId, 
                       isLoggedIn, 
                       wasDisconnectedByError,
                       gatewayId );
   }

   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << " exit " << endl;
   }

   return true;
}

bool  DiplodocusLogin:: SendLoginStatusTo_GameServers( const string& userName, 
                                                     const string& userUuid, 
                                                     U32 connectionId, 
                                                     U8 gameProductId, 
                                                     const string& lastLoginTime, 
                                                     bool isActive, 
                                                     const string& email, 
                                                     const string& passwordHash, 
                                                     const string& userId, 
                                                     const string& loginKey,
                                                     int languageId, 
                                                     bool isLoggedIn, 
                                                     bool wasDisconnectedByError,
                                                     U32 gatewayId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   // send this to every other listening server
   BaseOutputContainer::iterator itOutputs = m_listOfOutputs.begin();
   LogMessage( LOG_PRIO_ERR, "SendLoginStatusTo_GameServers for user: %s", userName.c_str() );
   while( itOutputs != m_listOfOutputs.end() )
   {
      ChainType*  outputPtr = static_cast< ChainType*> ( (*itOutputs).m_interface );
      itOutputs++;

      if( outputPtr->GetChainedType() != ChainedType_OutboundSocketConnector )
      {
         continue;
      }
      FruitadensLogin* login = static_cast< FruitadensLogin* >( outputPtr );      
      if( login->IsGameServer() == false )
      {
         continue;
      }
      SendLoginStatus( outputPtr, 
                        userName, 
                       userUuid, 
                       connectionId, 
                       gameProductId, 
                       lastLoginTime, 
                       isActive, 
                       email, 
                       passwordHash, 
                       userId, 
                       loginKey,
                       languageId, 
                       isLoggedIn, 
                       wasDisconnectedByError,
                       gatewayId );
   }

   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << " exit " << endl;
   }
   return true;
}


//---------------------------------------------------------------
/*
bool     DiplodocusLogin:: SendListOfUserProductsToOtherServers( const string& userUuid, U32 connectionId, const vector< string >& productNames )
{
   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      ////--------------
      fix me
//------------------------
      PacketListOfUserProductsS2S* packet = new PacketListOfUserProductsS2S;

      vector< string >::const_iterator it = productNames.begin();
      while( it != productNames.end() )
      {
         packet->products.insert( *it++ ) ;
      }
      packet->uuid = userUuid;

      BaseOutputContainer::iterator itOutputs = m_listOfOutputs.begin();
      while( itOutputs != m_listOfOutputs.end() )
      {
         ChainType*  outputPtr = static_cast< ChainType*> ( (*itOutputs).m_interface );

         if( outputPtr->AddOutputChainData( packet, m_chainId ) == true )
         {
            return true;
         }
         itOutputs++;
      }

      delete packet;   
   }

   return false;
}*/


//---------------------------------------------------------------

bool     DiplodocusLogin:: SendPacketToOtherServer( BasePacket* packet, U32 connectionId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      BaseOutputContainer::iterator itOutputs = m_listOfOutputs.begin();
      while( itOutputs != m_listOfOutputs.end() )
      {
         ChainType*  outputPtr = static_cast< ChainType*> ( (*itOutputs).m_interface );

         if( outputPtr->AddOutputChainData( packet, m_chainId ) == true )
         {
            // this will be cleaned up much later
            //PacketFactory factory;
            //factory.CleanupPacket( packet );  
            return true;
         }
         itOutputs++;
      }
   }

   return false;
}


//---------------------------------------------------------------

void     DiplodocusLogin:: UpdateUserRecord( CreateAccountResultsAggregator* aggregator )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   // any duplicates should simply report back to the user that this account email or user id is already taken
   if( aggregator->IsDuplicateUsernameOrEmailRecord() )
   {
      TellUserThatAccountAlreadyMatched( aggregator );
   }
   else if( aggregator->DoesGKPendingMatch_ButUsernameOrEmailAreNotSameRecord() )
   {
      UpdatePendingUserRecord( aggregator );
   }
   else if( aggregator->ShouldUpdatePendingUserRecord() ) // checks gkhash kit and does exact match, reset count back to 0
   {
      UpdatePendingUserRecord( aggregator );
   }
   else if( aggregator->ShouldInsertNewUserRecord() )
   {
      //
#ifdef _DEMO_13_Aug_2013
      CreateNewUserAccount( aggregator, true );
#else
      bool setGameKitHashToZero = aggregator->HasFoundMatchingGKHashInUserTable();
      CreateNewPendingUserAccount( aggregator, setGameKitHashToZero );
#endif
   }
   else
   {
      LogMessage( LOG_PRIO_ERR, "-----------------------------------" );
      LogMessage( LOG_PRIO_ERR, "Account create: error creating pending user account" );
      LogMessage( LOG_PRIO_ERR, "Username: ", aggregator->m_username.c_str() );
      LogMessage( LOG_PRIO_ERR, "Email:    ", aggregator->m_useremail.c_str() );
      LogMessage( LOG_PRIO_ERR, "GK Hash:  ", aggregator->m_gamekitHashId.c_str() );
      LogMessage( LOG_PRIO_ERR, "-----------------------------------" );

      SendErrorToClient( aggregator->GetConnectionId(), aggregator->GetGatewayId(), PacketErrorReport::ErrorType_CreateFailed_UserCreateAccountPending );
   }
}

//---------------------------------------------------------------

bool  DiplodocusLogin::HandlePacketFromOtherServer( BasePacket* packet, U32 connectionId )// not thread safe
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   U8 packetType = packet->packetType;
   U8 subType = packet->packetSubType;

   switch( packetType )
   {
      case PacketType_Login:
      {
         switch( subType )
         {
         case PacketLogin::LoginType_UserListOfPurchasesWasUpdated:
            {
               PacketListOfUserPurchasesUpdated* userInventory = static_cast<PacketListOfUserPurchasesUpdated*>( packet );
               RequestListOfPurchasesUpdate( userInventory );
            }
            break;
         }
      }
      break;
   }
   return false;
}

//---------------------------------------------------------------

// data going out can go only a few directions
// coming from the DB, we can have a result or possibly a different packet meant for a single chat UserConnection
// otherwise, coming from a UserConnection, to go out, it will already be packaged as a Gateway Wrapper and then 
// we simply send it on.
bool     DiplodocusLogin::AddOutputChainData( BasePacket* packet, U32 connectionId ) 
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   if( packet->packetType == PacketType_ServerJobWrapper )
   {
      return HandlePacketToOtherServer( packet, connectionId );
   }

   if( packet->packetType == PacketType_ServerToServerWrapper )
   {
      PacketServerToServerWrapper* wrapper = static_cast< PacketServerToServerWrapper* >( packet );

      U32 connectedServerId = wrapper->serverId;
      connectedServerId = connectedServerId;
      if( connectionId == m_serverId || connectionId == 0 )// outgoing... we are send it
      {
         return HandlePacketToOtherServer( wrapper, connectionId );
      }
      else
      {
         HandlePacketFromOtherServer( wrapper->pPacket, connectionId );
         PacketFactory factory;
         factory.CleanupPacket( packet );
         return true;
      }
   }

   if( packet->packetType == PacketType_DbQuery )// coming from the db
   {
      Threading::MutexLock locker( m_mutex );
      if( packet->packetSubType == BasePacketDbQuery::QueryType_Result )
      {
         PacketDbQueryResult* result = static_cast<PacketDbQueryResult*>( packet );
         m_dbQueries.push_back( result );
      /*   if( result->customData != NULL )
            cout << "AddOutputChainData: Non-null custom data " << endl;*/
      }
      return true;
      //assert(0);/// should not happen
   }
   
   return false;
}


//---------------------------------------------------------------

void     DiplodocusLogin::UpdateDbResults()
{
   PacketFactory factory;

   m_mutex.lock();
      deque< PacketDbQueryResult* > tempQueue = m_dbQueries;
      m_dbQueries.clear();
   m_mutex.unlock();

   if( tempQueue.size() )
   {
      if( m_printFunctionNames == true )
      {
         cout << "fn: " << __FUNCTION__ << endl;
      }
   }

   deque< PacketDbQueryResult* >::iterator it = tempQueue.begin();
   while( it != tempQueue.end() )
   {
      PacketDbQueryResult* result = *it++;
    /*  if( result->customData != NULL )
            cout << "UpdateDbResults: Non-null custom data " << endl;*/
      BasePacket* packet = static_cast<BasePacket*>( result );

      HandleDbResult( result );
      factory.CleanupPacket( packet );
   }
}

//---------------------------------------------------------------

bool     DiplodocusLogin:: HandleDbResult( PacketDbQueryResult* dbResult )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   U32 connectionId = dbResult->id;

   // new user accounts are not going to be part of the normal login.
   UserCreateAccountIterator createIt = m_userAccountCreationMap.find( connectionId );
   if( createIt != m_userAccountCreationMap.end () )
   {
      CreateAccountResultsAggregator* aggregator = createIt->second;
      aggregator->HandleResult( dbResult );
      if( aggregator->IsComplete() )
      {
         UpdateUserRecord( aggregator );
         delete aggregator;
         //Threading::MutexLock locker( m_inputChainListMutex );
         m_userAccountCreationMap.erase( createIt );
      }
      return true;
   }

   ConnectionToUser* connection = NULL; 
   if( connectionId != 0 )
   {
      connection = GetUserConnection( connectionId );
      if( connection == NULL )
      {
         string str = "Login server: Something seriously wrong where the db query came back from the server but no record.. ";
         LogMessage( LOG_PRIO_ERR, str.c_str() );
         str = "was apparently requested or at least it was not stored properly: userName was :";
         str += dbResult->meta;
         LogMessage( LOG_PRIO_ERR, str.c_str() );
         return false;
      }
   }
   bool  wasHandled = false;
   if( m_stringLookup && m_stringLookup->HandleResult( dbResult ) == true )
   {
      wasHandled = true;
   }
   else if( m_outageReader && m_outageReader->HandleResult( dbResult ) == true )
   {
      wasHandled = true;
   }
   else
   {
      switch( dbResult->lookup )
      {
         cout << "Db query type:"<< dbResult->lookup << ", success=" << dbResult->successfulQuery << endl;

         case QueryType_UserLoginInfo:
            {
               if( HandleUserLoginResult( connectionId, dbResult ) == false )
               {
                  SendErrorToClient( connectionId, connection->GetGatewayId(), PacketErrorReport::ErrorType_UserBadLogin );  
                  string str = "User not valid and db query failed, userName: ";
                  str += connection->m_userName;
                  str += ", password: ";
                  str += connection->m_passwordHash;
                  str += ", connectionId: ";
                  str += boost::lexical_cast< string> ( connectionId );
                  str += ", gatewayId: ";
                  str += boost::lexical_cast< string> ( connection->m_connectionDetails.gatewayId );
                  
                  cout << "Error:" << str << endl;
                  cout << connection->m_passwordHash << endl;

                  LogMessage( LOG_PRIO_ERR, str.c_str() );
                  connection->ForceCleanupState();
                  time_t currentTime;
                  time( &currentTime );
                  currentTime += normalLogoutExpireTime - 3;// give us 3 seconds to cleanup
                  connection->SetLoggedOutTime( currentTime );
                  ForceUserLogoutAndBlock( connectionId, connection->GetGatewayId() );
                  wasHandled = false;
               }
               else
               {
                  wasHandled = true;
               }
            }
            break;
         case QueryType_AdminRequestUserProfile:
            {
               // in some weird circustance, we could end up in an infinite loop here.
              /* if( dbResult->successfulQuery == false || dbResult->GetBucket().size() == 0 )// no records found
               {
                  connection->AddBlankUserProfile();
               }
               else
               {
                  HandleUserProfileFromDb( connectionId, dbResult );
               }*/
               HandleAdminRequestUserProfile( connectionId, dbResult );
               wasHandled = true;
            }
            break;
         case QueryType_UserListOfGame:
            {
               if( dbResult->successfulQuery == false || dbResult->GetBucket().size() == 0 )// no records found
               {
                  string str = "List of games not valid db query failed, userName: ";
                  str += connection->m_userName;
                  str += ", uuid: ";
                  str += connection->m_userUuid;
                  LogMessage( LOG_PRIO_ERR, str.c_str() );
                  ForceUserLogoutAndBlock( connectionId, connection->GetGatewayId() );
                  wasHandled = false;
               }
               else
               {
                  wasHandled = true;
               
                  KeyValueVector             key_value_array;

                  SimpleGameTable            enigma( dbResult->bucket );
                  SimpleGameTable::iterator it = enigma.begin();
                  
                  while( it != enigma.end() )
                  {
                     SimpleGameTable::row       row = *it++;
                     string name =              row[ SimpleGame::Column_name ];
                     string uuid =              row[ SimpleGame::Column_uuid ];

                     key_value_array.push_back( KeyValueString ( uuid, name ) );
                  }

                  SendListOfOwnedGamesToGameServers( connectionId, key_value_array );
               }
            }
            break;
         case QueryType_UserListOfUserProducts:
            {
               if( dbResult->successfulQuery == false )//|| dbResult->GetBucket().size() == 0 )
               {
                  string str = "Query failed looking up a user products ";
                  str += connection->m_userName;
                  str += ", uuid: ";
                  str += connection->m_userUuid;
                  LogMessage( LOG_PRIO_ERR, str.c_str() );
                  ForceUserLogoutAndBlock( connectionId, connection->GetGatewayId() );
                  wasHandled = false;
               }
               else
               {
                  StoreListOfUsersProductsFromDB( connectionId, dbResult );
                  wasHandled = true;
               }
            }
            break;
         case QueryType_LookupUserByUsernameOrEmail:// these should never happen since these are handled elsewhere
         case QueryType_LookupTempUserByUsernameOrEmail:
         case QueryType_LookupUserNameForInvalidName:
         case QueryType_LookupUserByGkHash:
            {
               if( dbResult->successfulQuery == false || dbResult->GetBucket().size() == 0 )
               {
                  string str = "Query failed looking up a user ";
                  str += connection->m_userName;
                  str += ", uuid: ";
                  str += connection->m_userUuid;
                  LogMessage( LOG_PRIO_ERR, str.c_str() );
                  ForceUserLogoutAndBlock( connectionId, connection->GetGatewayId() );
                  wasHandled = false;
               }
               else
               {
                  wasHandled = true;
               }
            }
            break;
         case QueryType_LoadProductInfo:
            {
               if( dbResult->successfulQuery == false || dbResult->GetBucket().size() == 0 )
               {
                  string str = "Initialization failed: table does not exist ";
                  wasHandled = false;
               }
               else
               {
                  StoreAllProducts( dbResult );
                  wasHandled = true;
               }
            }
            break;
         case QueryType_GetSingleProductInfo:
            {
               if( dbResult->successfulQuery == false ||
                  dbResult->GetBucket().size() == 0 )
               {
                  string str = "Product not found ";
                  str += dbResult->meta;
                  LogMessage( LOG_PRIO_ERR, str.c_str() );
                  wasHandled = false;
               }
               else
               {
                  StoreSingleProduct( dbResult );
                  wasHandled = true;
               }
            }
            break;
         case QueryType_GetProductListForUser:
            {
               SendListOfPurchasesToUser( connectionId, dbResult );
               wasHandled = true;
            }
            break;
         }
      }
      return wasHandled;
}

//---------------------------------------------------------------

void     DiplodocusLogin:: StoreAllProducts( const PacketDbQueryResult* dbResult )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ProductTable            enigma( dbResult->bucket );

   ProductTable::iterator  it = enigma.begin();
   //if( m_printPacketTypes == true )
   {
      LogMessage( LOG_PRIO_INFO, "products loaded: enigma size=%d", enigma.size() );
   }
  // int numProducts = static_cast< int >( dbResult->bucket.size() );          
   while( it != enigma.end() )
   {
      ProductTable::row       row = *it++;

      ProductInfo productDefn;
      productDefn.uuid =                  row[ TableProduct::Column_uuid ];
      productDefn.name =                  row[ TableProduct::Column_name ];
      int id =                            boost::lexical_cast< int >( row[ TableProduct::Column_id ] );
      productDefn.vendorUuid =            row[ TableProduct::Column_vendor_uuid ];
      if( row[ TableProduct::Column_is_hidden ] == "1" )
         productDefn.isHidden =           true;
      else 
         productDefn.isHidden =           false;

      string productId =                  row[ TableProduct::Column_product_id ];
      if( productId == "NULL" || productId.size() == 0 || productId == "0" )
      {
         if( productDefn.vendorUuid.size() == 0 )
         {
            LogMessage( LOG_PRIO_ERR, "Invalid product: %d, name='%s', uuid='%s', filter='%s'", id, productDefn.name.c_str(), productDefn.uuid.c_str(), productDefn.vendorUuid.c_str() );
            continue;// invalid product
         }
         // the filter is still useful for preventing further additions
         LogMessage( LOG_PRIO_ERR, "Invalid product: %d, name='%s', uuid='%s', filter='%s'", id, productDefn.name.c_str(), productDefn.uuid.c_str(), productDefn.vendorUuid.c_str() );
      }

      if( FindProductByVendorUuid( productDefn.vendorUuid ) != DiplodocusLogin::ProductNotFound )
      {
         LogMessage( LOG_PRIO_ERR, "Duplicate product found by vendor uuid" );
         LogMessage( LOG_PRIO_ERR, "Invalid product: %d, name='%s', uuid='%s', filter='%s'", id, productDefn.name.c_str(), productDefn.uuid.c_str(), productDefn.vendorUuid.c_str() );
      }

      std::string lowercase_productUUID = productDefn.vendorUuid; 
      std::transform( lowercase_productUUID.begin(), lowercase_productUUID.end(), lowercase_productUUID.begin(), ::tolower );
      productDefn.vendorUuid = lowercase_productUUID;

      productDefn.productId  =            boost::lexical_cast< int >( productId );
      
      
      productDefn.Begindate =             row[ TableProduct::Column_begin_date ];
      productDefn.lookupName =            row[ TableProduct::Column_name_string ];
      string temp = row[ TableProduct::Column_product_type ];
      if( temp == "" )
         temp = "0";

      productDefn.parentId =              boost::lexical_cast< int >( row[ TableProduct::Column_parent_id ] );

      productDefn.productType  =          boost::lexical_cast< int >( temp );
      
      productDefn.iconName =              row[ TableProduct::Column_icon_lookup ];
      productDefn.convertsToProductId =   boost::lexical_cast< int >( row[ TableProduct::Column_converts_to_product_id ] );
      productDefn.convertsToQuantity =    boost::lexical_cast< int >( row[ TableProduct::Column_converts_to_quantity ] );

      m_productList.push_back( productDefn );
   }

   //if( m_printPacketTypes == true )
   {
      LogMessage( LOG_PRIO_ERR, "products loaded: size=%d", m_productList.size() );
   }

   m_isInitialized = true;
}

//---------------------------------------------------------------

void     DiplodocusLogin:: StoreSingleProduct( const PacketDbQueryResult* dbResult )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ProductTable            enigma( dbResult->bucket );

   string vendorUuid;
   string filterUuid;

   
   ProductTable::row row = *enigma.begin();
   //if( it != enigma.end() )
   {
      ProductInfo productDefn;
      productDefn.productId  =            boost::lexical_cast< int >( row[ TableProduct::Column_product_id ] );
      productDefn.uuid =                  row[ TableProduct::Column_uuid ];
      productDefn.name =                  row[ TableProduct::Column_name ];
      productDefn.vendorUuid =            row[ TableProduct::Column_vendor_uuid ];

      if( row[ TableProduct::Column_is_hidden ] == "1" )
         productDefn.isHidden =           true;
      else 
         productDefn.isHidden =           false;

      std::string lowercase_productUUID = productDefn.vendorUuid; 
      std::transform( lowercase_productUUID.begin(), lowercase_productUUID.end(), lowercase_productUUID.begin(), ::tolower );
      productDefn.vendorUuid = lowercase_productUUID;

      productDefn.Begindate =             row[ TableProduct::Column_begin_date ];
      productDefn.lookupName =            row[ TableProduct::Column_name_string ];

      string temp =                       row[ TableProduct::Column_product_type ];
      if( temp == "" )
         temp = "0";
      productDefn.productType  =          boost::lexical_cast< int >( temp );
      
      vendorUuid = productDefn.vendorUuid;
      filterUuid = productDefn.uuid;

      if( FindProductByVendorUuid( vendorUuid ) != DiplodocusLogin::ProductNotFound )
      {
         LogMessage( LOG_PRIO_ERR, "StoreSingleProduct failed" );
         LogMessage( LOG_PRIO_ERR, "New product already stored" );
         return;
      }

      m_productList.push_back( productDefn );
   }

   // now that we've added the new product, see which users needed it.
   //Threading::MutexLock locker( m_inputChainListMutex );
   UserConnectionMapIterator userIt = m_userConnectionMap.begin();
   while( userIt != m_userConnectionMap.end() )
   {
      vector< ProductInfo >& userProducts = userIt->second.productsWaitingForInsertionToDb;
      vector< ProductInfo >::iterator productIt = userProducts.begin();
      while( productIt != userProducts.end() )
      {
         if( productIt->vendorUuid == vendorUuid )
         {
            double price = productIt->price;
            userIt->second.WriteProductToUserRecord( filterUuid, price );
            userProducts.erase( productIt );

            if( userProducts.size() == 0 )
            {
               SendListOfUserProductsToAssetServer( userIt->first );
            }
            break;
         }
         productIt ++;
      }


      userIt ++;
   }
   
}

//---------------------------------------------------------------

void     DiplodocusLogin:: SendListOfPurchasesToUser( U32 connectionId, PacketDbQueryResult* dbResult )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      connection->StoreProductInfo( dbResult );
   }
}

//---------------------------------------------------------------

void     DiplodocusLogin:: StoreListOfUsersProductsFromDB( U32 connectionId, PacketDbQueryResult* dbResult )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      connection->StoreListOfUsersProductsFromDB( dbResult, m_autoAddProductFromWhichUsersLogin );
   }
}

//---------------------------------------------------------------

void     DiplodocusLogin:: AddNewProductToDb( const PurchaseEntryExtended& product )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   std::string lowercase_productUuidname = product.productUuid; 
   std::transform( lowercase_productUuidname.begin(), lowercase_productUuidname.end(), lowercase_productUuidname.begin(), ::tolower );
   

   if( FindProductByVendorUuid( lowercase_productUuidname ) != DiplodocusLogin::ProductNotFound )
   {
      LogMessage( LOG_PRIO_ERR, "Duplicate product found by vendor uuid" );
      LogMessage( LOG_PRIO_ERR, "Invalid product: %d, name='%s', uuid='%s', filter='%s'", product.name.c_str(), lowercase_productUuidname.c_str(), lowercase_productUuidname.c_str() );
      return;
   }

   PacketDbQuery* dbQuery = new PacketDbQuery;
   dbQuery->id =           0;
   dbQuery->lookup =       QueryType_AddProductInfo;
   dbQuery->meta =         product.name.c_str();
   dbQuery->serverLookup = 0;
   dbQuery->isFireAndForget = true;

   U32 hash = static_cast<U32>( GenerateUniqueHash( product.name ) );
   string newUuid = GenerateUUID( hash );   

   LogMessage( LOG_PRIO_INFO, "New product entry: " );
   LogMessage( LOG_PRIO_INFO, " name: %s", product.name.c_str() );
   LogMessage( LOG_PRIO_INFO, " UUID: %s", newUuid.c_str() );

   dbQuery->query = "INSERT INTO product (product_id, uuid, name, filter_name, first_available) ";
   dbQuery->query += "VALUES( 0, '";// new products haven an id of 0
   dbQuery->query += newUuid;
   dbQuery->query += "', '%s', '%s', UTC_TIMESTAMP())";

   dbQuery->escapedStrings.insert( product.name );
   dbQuery->escapedStrings.insert( lowercase_productUuidname );

   AddQueryToOutput( dbQuery );

   //--------------------------------------------------

   // pull back the results so that we have the index.
   dbQuery = new PacketDbQuery;
   dbQuery->id =           0;
   dbQuery->lookup =       QueryType_GetSingleProductInfo;
   dbQuery->meta =         product.name.c_str();
   dbQuery->serverLookup = 0;

   dbQuery->query = "SELECT * FROM product WHERE uuid = '%s'";
   dbQuery->escapedStrings.insert( newUuid );

   AddQueryToOutput( dbQuery );
}

//---------------------------------------------------------------

void     DiplodocusLogin:: SendListOfUserProductsToAssetServer( U32 connectionId )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   ConnectionToUser* connection = GetUserConnection( connectionId );
   if( connection )
   {
      PacketListOfUserProductsS2S* packet = new PacketListOfUserProductsS2S;
      packet->uuid = connection->m_userUuid;
      vector< string >::iterator it =  connection->productVendorUuids.begin();
      while( it != connection->productVendorUuids.end() )
      {
         ProductInfo productDefn;
         if( FindProductByLookupName( *it++, productDefn ) == true )
         {
            packet->products.insert( productDefn.vendorUuid );
         }
      }


      BaseOutputContainer::iterator itOutputs = m_listOfOutputs.begin();
      while( itOutputs != m_listOfOutputs.end() )
      {
         ChainType*  outputPtr = static_cast< ChainType*> ( (*itOutputs).m_interface );

         if( outputPtr->AddOutputChainData( packet, m_chainId ) == true )
         {
            return;
         }
         itOutputs++;
      }

      delete packet;
   }
}

//---------------------------------------------------------------

void     DiplodocusLogin:: LoadInitializationData()
{
   if( m_productList.size() == 0 )
   {
      time_t currentTime;
      time(& currentTime );
      // try every 15 seconds
      if( difftime( currentTime, m_initializingProductListTimeStamp ) > 15 )
      {
         m_initializingProductListTimeStamp = currentTime;
         if( m_printFunctionNames == true )
         {
            cout << "fn: " << __FUNCTION__ << endl;
         }

         PacketDbQuery* dbQuery = new PacketDbQuery;
         dbQuery->id =           0;
         dbQuery->lookup =       QueryType_LoadProductInfo;
         dbQuery->meta =         "";
         dbQuery->serverLookup = 0;

         // add in 0's last
         dbQuery->query = "SELECT * FROM product ORDER BY product_id DESC"; // WHERE product_id > 0 

         AddQueryToOutput( dbQuery );
      }
   }
}


//-----------------------------------------------------------------------------------------

void     DiplodocusLogin::TrackCountStats( StatTracking stat, float value, int sub_category )
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   StatTrackingConnections::TrackCountStats( m_serverName, m_serverId, stat, value, sub_category );
}

//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------

void     DiplodocusLogin::RunHourlyStats()
{
   /*if( m_userConnectionMap.size() == 0 )
      return;*/

   time_t currentTime;
   time( &currentTime );

   if( difftime( currentTime, m_timestampHourlyStatServerStatisics ) >= timeoutHourlyStatisics ) 
   {
      if( m_printFunctionNames == true )
      {
         cout << "fn: " << __FUNCTION__ << endl;
      }
      m_timestampHourlyStatServerStatisics = ZeroOutMinutes( currentTime );

      //--------------------------------
      float numConnections = static_cast<float>( m_userConnectionMap.size() );
      float totalNumSeconds = 0;

      UserConnectionMapIterator nextIt = m_userConnectionMap.begin();
      while( nextIt != m_userConnectionMap.end() )
      {
         const ConnectionToUser& user = nextIt->second;
         nextIt++;
         time_t loginTime = user.GetLoginTime();
         if( loginTime == 0 )
            loginTime = currentTime;

         totalNumSeconds += static_cast<float>( difftime( currentTime, loginTime ) );
      }

      totalNumSeconds += m_totalUserLoginSeconds;
      numConnections += m_totalNumLogouts;

      float averageNumSeconds = totalNumSeconds / numConnections;
      TrackCountStats( StatTracking_UserAverageTimeOnline, averageNumSeconds, 0 );
      TrackCountStats( StatTracking_UserTotalTimeOnline, totalNumSeconds, 0 );
      TrackCountStats( StatTracking_NumUsersOnline, numConnections, 0 );

      // if we decide to track these
      m_numFailedLogins = 0;
      m_numRelogins = 0;
      m_numSuccessfulLogins = 0;
      m_totalUserLoginSeconds = 0;
      m_totalNumLogouts = 0;
   }
}

//---------------------------------------------------------------

void     DiplodocusLogin::RunDailyStats()
{
   time_t currentTime;
   time( &currentTime );
   if( difftime( currentTime, m_timestampDailyStatServerStatisics ) >= timeoutDailyStatisics ) 
   {
      if( m_printFunctionNames == true )
      {
         cout << "fn: " << __FUNCTION__ << endl;
      }
      m_timestampDailyStatServerStatisics = ZeroOutHours( currentTime );

      float numUniqueUsers = static_cast< float >( m_uniqueUsers.size() );
      if( numUniqueUsers == 0 )
         return;

      ClearOutUniqueUsersNotLoggedIn();

      TrackCountStats( StatTracking_UniquesUsersPerDay, numUniqueUsers, 0 );
   }
}

//---------------------------------------------------------------

void     DiplodocusLogin::ClearOutUniqueUsersNotLoggedIn()
{
   if( m_printFunctionNames == true )
   {
      cout << "fn: " << __FUNCTION__ << endl;
   }

   // clear out
   set< string >::iterator it = m_uniqueUsers.begin();
   while( it != m_uniqueUsers.end() )
   {
      set< string >::iterator currentIt = it++;
      UserConnectionMapIterator userIt = m_userConnectionMap.begin();
      bool found = false;
      while( userIt != m_userConnectionMap.end() )
      {
         const ConnectionToUser& user = userIt->second;
         userIt++;
         if( m_uniqueUsers.find( user.m_email ) != m_uniqueUsers.end() )
         {
            found = true;
            break;
         }
      }
      if( found == false )
      {
         m_uniqueUsers.erase( currentIt );
      }
   }
}
//---------------------------------------------------------------

int      DiplodocusLogin:: CallbackFunction()
{
   /*// too chatty to be useful
   if( m_printFunctionNames == true )
   {
      cout << "fn: ClearOutUniqueUsersNotLoggedIn" << endl;
   }

   */
   CommonUpdate();

   if( m_isInitializing == true )
   {
      if( m_productList.size() > 0 )
         m_isInitializing = false; 

      if( m_isInitialized == false )
      {
         LoadInitializationData();
      }
   }

   time_t currentTime;
   time( &currentTime );
   if( m_stringLookup )
      m_stringLookup->Update( currentTime );

   CleanupOldClientConnections( "KhaanLogin" );
   UpdateAllConnections( "KhaanLogin" );

   UpdateInputPacketToBeProcessed();

   RemoveOldConnections();

   UpdatePendingGatewayPackets();

   if( m_outageReader )
      m_outageReader->Update();

   RunHourlyStats();
   RunDailyStats();

   UpdateDbResults();

   StatTrackingConnections::SendStatsToStatServer( m_listOfOutputs, m_serverName, m_serverId, m_serverType );

   return 1;
}

//---------------------------------------------------------------
//////////////////////////////////////////////////////////
