/*
 * Author: Wenjie Li
 * Date: 07/15/2016
 * Tracer to record the reward on each router
 */

#ifndef NDN_REWARD_TRACER_H
#define NDN_REWARD_TRACER_H

#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"
#include "ns3/ndn-videocontent.h"
#include <ns3/nstime.h>
#include <ns3/event-id.h>
#include <ns3/node-container.h>
#include "ns3/ndn-bitrate.h"

#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>
#include <map>
#include <list>
#include <sstream>
#include <memory>

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>


namespace ns3 {

class Node;
class Packet;

namespace ndn {

class Interest;
class Data;

namespace cs {

}

class RewardTracer : public SimpleRefCount<RewardTracer>
{
public:

  /*
   * Record traces for all nodes on MySQL database;
   */
  static void
  InstallAllSql (sql::Driver* d, const std::string& ip, const std::string& database, const std::string& rtable, const std::string& ptable,
		  	  	  const std::string& user, const std::string& password, const int expidx,
				  const std::string BitRates[], uint32_t TotalBitrate);

  static Ptr<RewardTracer>
  InstallSql (Ptr<Node>, boost::shared_ptr<std::ostringstream>, boost::shared_ptr<std::ostringstream>);

  static void
  Destroy ();


  RewardTracer (boost::shared_ptr<std::ostringstream>,boost::shared_ptr<std::ostringstream>, Ptr<Node> node);

  /**
   * @brief Destructor
   */
  ~RewardTracer ();


  /**
   * @brief Print current trace data
   *
   * @param os reference to output stream
   */
  void
  PrintReward () const;

  void
  PrintPartition (uint32_t, double, std::shared_ptr<double>);

  static sql::Statement 	*stmt;
  static sql::Driver 		*driver;
  static sql::Connection 	*con;

private:
  void
  Connect ();

private:

  void
  CacheReward(uint32_t, const std::string&, double);

  void
  CachePartition(uint32_t, double, std::shared_ptr<double>);

  void
  Reset ();

  void
  ClearRewardTable();

  void
  ClearPartitionTable();

private:
  std::string 	m_node;
  Ptr<Node> 	m_nodePtr;
  Ptr<NDNBitRate> m_BRinfo;

  boost::shared_ptr<std::ostringstream> m_rewardStream;
  boost::shared_ptr<std::ostringstream> m_partitionStream;

  std::map<std::string, double> m_stats;
  uint32_t 		m_transition;

  static std::list< boost::tuple< boost::shared_ptr<std::ostringstream>,
  	  	  	  	  	  	  	  	  boost::shared_ptr<std::ostringstream>,
  	  	  	  	  	  	  	  	  std::list<Ptr<RewardTracer> > > > 	g_tracers_reward;

  static std::string		RewardTable;
  static int				RewardTableItem;

  static std::string		PartitionTable;
  static int				PartitionTableItem;

  static std::string		DatabaseName;

  static int				TableId;
  static bool				usedfordestruction;

};

} // namespace ndn
} // namespace ns3

#endif
