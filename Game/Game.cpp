// Game.cpp
//

#include <iostream>
#include <list>
#include <vector>
#pragma warning (disable:4996) 
using namespace std;

#include <conio.h>
#include <assert.h>

#include <boost/lexical_cast.hpp>

#include "DiplodocusGame.h"
#include "FruitadensServerToServer.h"

#include "../NetworkCommon/Packets/ServerToServerPacket.h"
#include "../NetworkCommon/NetworkIn/DiplodocusServerToServer.h"


//-----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
   CommandLineParser    parser( argc, argv );

   string listenPort = "23995";
   string listenAddress = "localhost";

   string chatServerPortForGames = "9602";
   string chatServerAddressForGames = "localhost";

   string listenForS2SPort = "23996";
   string listenForS2SAddress = "localHost";

   //---------------------------------------
   parser.FindValue( "listen.port", listenPort );
   parser.FindValue( "listen.address", listenAddress );

   parser.FindValue( "s2s.port", listenForS2SPort );
   parser.FindValue( "s2s.address", listenForS2SAddress );

   string dbPort = "16384";
   string dbIpAddress = "localhost";
   string dbUsername = "root";
   string dbPassword = "password";
   string dbSchema = "pleiades";

   parser.FindValue( "db.address", dbIpAddress );
   parser.FindValue( "db.port", dbPort );
   parser.FindValue( "db.username", dbUsername );
   parser.FindValue( "db.password", dbPassword );
   parser.FindValue( "db.schema", dbSchema );

   int listenPortAddress = 23995, 
      dbPortAddress = 16384, 
      chatServerPort = 9602, 
      listenS2SPort = 23996;
   try 
   {
      listenS2SPort = boost::lexical_cast<int>( listenForS2SPort );
      listenPortAddress = boost::lexical_cast<int>( listenPort );
      dbPortAddress = boost::lexical_cast<int>( dbPort );
      chatServerPort = boost::lexical_cast<int>( chatServerPortForGames );
   } 
   catch( boost::bad_lexical_cast const& ) 
   {
      std::cout << "Error: input string was not valid" << std::endl;
   }

   Deltadromeus* delta = new Deltadromeus;
   delta->SetConnectionInfo( dbIpAddress, dbPortAddress, dbUsername, dbPassword, dbSchema );
   if( delta->IsConnected() == false )
   {
      cout << "Error: Database connection is invalid." << endl;
      getch();
      return 1;
   }

   //---------------------------------------------------------------------
   // the guts

   
   
   
   string serverName = "Agricola Game Server";
   U64 serverUniqueHashValue = GenerateUniqueHash( serverName );

   string version = "0.04";
   cout << serverName << ":" << endl;
   cout << "Version " << version << endl;
   cout << "ServerId " << (U32)serverUniqueHashValue << endl;
   cout << "------------------------------------------------------------------" << endl << endl << endl;

   //----------------------------------------------------------------
   DiplodocusGame*    middleware = new DiplodocusGame();
   middleware->AddOutputChain( delta );

   middleware->SetupListening( listenPortAddress );

   FruitadensServerToServer chatControl( "fruity to chat" );
   chatControl.SetConnectedServerType( ServerType_Chat );
   chatControl.SetServerId( static_cast< U32>( serverUniqueHashValue ) );
   
   chatControl.Connect( chatServerAddressForGames.c_str(), chatServerPort );
   chatControl.Resume();
   chatControl.NotifyEndpointOfIdentification( serverName, static_cast< U32>( serverUniqueHashValue ), true, false, false );

   DiplodocusServerToServer* s2s = new DiplodocusServerToServer();
   s2s->SetupListening( listenS2SPort );

   //----------------------------------------------------------------
   s2s->AddOutputChain( middleware );
   middleware->AddOutputChain( &chatControl );

   middleware->Run();

   getch();

	return 0;
}
