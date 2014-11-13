CREATE DATABASE  IF NOT EXISTS `playdek` /*!40100 DEFAULT CHARACTER SET utf8 */;
USE `playdek`;
-- MySQL dump 10.13  Distrib 5.6.13, for Win32 (x86)
--
-- Host: 10.16.4.44    Database: playdek
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
-- Table structure for table `stats_summonwar_faction_losses`
--

DROP TABLE IF EXISTS `stats_summonwar_faction_losses`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `stats_summonwar_faction_losses` (
  `user_id` int(11) unsigned NOT NULL,
  `faction_id` int(11) unsigned NOT NULL,
  `vs_faction_0` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_1` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_2` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_3` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_4` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_5` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_6` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_7` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_8` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_9` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_10` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_11` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_12` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_13` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_14` int(11) unsigned NOT NULL DEFAULT '0',
  `vs_faction_15` int(11) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`user_id`,`faction_id`),
  CONSTRAINT `stats_summonwar_faction_losses_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `users` (`user_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2014-11-13 15:07:52