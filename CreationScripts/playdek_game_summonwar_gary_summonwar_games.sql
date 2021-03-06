CREATE DATABASE  IF NOT EXISTS `playdek_game_summonwar_gary` /*!40100 DEFAULT CHARACTER SET latin1 */;
USE `playdek_game_summonwar_gary`;
-- MySQL dump 10.13  Distrib 5.6.13, for Win32 (x86)
--
-- Host: 10.16.4.44    Database: playdek_game_summonwar_gary
-- ------------------------------------------------------
-- Server version	5.1.49-3-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `summonwar_games`
--

DROP TABLE IF EXISTS `summonwar_games`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `summonwar_games` (
  `game_id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `game_version` int(11) unsigned NOT NULL DEFAULT '1',
  `max_players` int(11) unsigned NOT NULL DEFAULT '2',
  `random_seed` int(11) unsigned NOT NULL,
  `game_state` int(11) unsigned NOT NULL DEFAULT '0',
  `game_params` tinyblob,
  `rematch_id` int(11) NOT NULL DEFAULT '0',
  `player_timer` int(11) NOT NULL DEFAULT '0',
  `event_id` int(10) unsigned DEFAULT NULL,
  `event_matchup_index` int(10) unsigned DEFAULT NULL,
  `creation_time` datetime NOT NULL,
  `decision_time` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `start_time` datetime DEFAULT NULL,
  `completion_time` datetime DEFAULT NULL,
  PRIMARY KEY (`game_id`)
) ENGINE=InnoDB AUTO_INCREMENT=636 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2014-11-13 15:08:03
