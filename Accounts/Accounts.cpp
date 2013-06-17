// Accounts.cpp : Defines the entry point for the console application.
//

// Login.cpp
//

#include <iostream>
#include <list>
#include <vector>

using namespace std;

//#include <conio.h>
#include <assert.h>

#include <boost/lexical_cast.hpp>

#include <cstdio>
#include <memory.h>

#include "../NetworkCommon/Utils/Utils.h"

#include "../NetworkCommon/Utils/CommandLineParser.h"

#include "../NetworkCommon/Database/Deltadromeus.h"
#include "../NetworkCommon/Packets/ServerToServerPacket.h"

#include "../NetworkCommon/Daemon/Daemonizer.h"

#include "StatusUpdate.h"


#if PLATFORM == PLATFORM_WINDOWS
#include <conio.h>
#pragma warning (disable:4996)
#endif

// db.address=10.16.4.44 db.port=3306 db.username=admin db.password=Pz5328!@ db.schema=pleiades
////////////////////////////////////////////////////////////////////////

//FruitadensLogin* PrepFruitadensLogin( const string& ipaddress, U16 port, U32 serverId, DiplodocusLogin* loginServer );

////////////////////////////////////////////////////////////////////////

int main( int argc, const char* argv[] )
{
   daemonize( "account_serverd" );

   CommandLineParser    parser( argc, argv );


   string chatPortString = "9602";
   string chatIpAddressString = "localhost";

   string agricolaPortString = "23996";
   string agricolaIpAddressString = "localhost";

   //---------------------------------------

   parser.FindValue( "chat.port", chatPortString );
   parser.FindValue( "chat.address", chatIpAddressString );

   parser.FindValue( "agricola.address", agricolaPortString );
   parser.FindValue( "agricola.port", agricolaIpAddressString );


   string dbPortString = "16384";
   string dbIpAddress = "localhost";
   string dbUsername = "root";
   string dbPassword = "password";
   string dbSchema = "playdek";

   parser.FindValue( "db.address", dbIpAddress );
   parser.FindValue( "db.port", dbPortString );
   parser.FindValue( "db.username", dbUsername );
   parser.FindValue( "db.password", dbPassword );
   parser.FindValue( "db.schema", dbSchema );


   int chatPort = 9602, dbPortAddress = 3306, agricolaPort = 23996;
   try 
   {
       //listenPort = boost::lexical_cast<int>( listenPortString );
       chatPort = boost::lexical_cast<int>( chatPortString );
       agricolaPort = boost::lexical_cast<int>( agricolaPortString );
       dbPortAddress = boost::lexical_cast<int>( dbPortString );
   } 
   catch( boost::bad_lexical_cast const& ) 
   {
       std::cout << "Error: input string was not valid" << std::endl;
   }

   //--------------------------------------------------------------

   Deltadromeus* delta = new Deltadromeus;
   delta->SetConnectionInfo( dbIpAddress, dbPortAddress, dbUsername, dbPassword, dbSchema );
   if( delta->IsConnected() == false )
   {
      cout << "Error: Database connection is invalid." << endl;
      getch();
      return 1;
   }

   //----------------------------------------------------------------


   string serverName = "Account server";
   U64 serverUniqueHashValue = GenerateUniqueHash( serverName );
   U32 serverId = (U32)serverUniqueHashValue;

   string version = "0.01";
   cout << serverName << endl;
   cout << "Version " << version << endl;
   cout << "ServerId " << serverId << endl;
   cout << "------------------------------------------------------------------" << endl << endl << endl;

   StatusUpdate* server = new StatusUpdate( serverName, serverId );
   server->AddOutputChain( delta );
   
   server->Resume();

   getch();
   
	return 0;
}


////////////////////////////////////////////////////////////////////////
