/*
 * Author: Wenjie Li
 * Date: 07/15/2016
 * Reward Tracer for cache multisection project
 */

#include "ndn-reward-tracer.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/config.h"
#include "ns3/names.h"
#include "ns3/callback.h"

#include "ns3/ndn-app.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/ndn-content-store.h"
#include "ns3/simulator.h"
#include "ns3/node-list.h"
#include "ns3/ndn-bitrate.h"
#include "ns3/log.h"

#include <boost/lexical_cast.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <exception>
#include <iomanip>

NS_LOG_COMPONENT_DEFINE ("ndn.RewardTracer");

using namespace std;

namespace ns3 {
namespace ndn {

sql::Driver* RewardTracer::driver = nullptr;
sql::Connection* RewardTracer::con = nullptr;
sql::Statement* RewardTracer::stmt = nullptr;

std::string RewardTracer::RewardTable;
int RewardTracer::TableId;
int RewardTracer::RewardTableItem;

std::string RewardTracer::PartitionTable;
int RewardTracer::PartitionTableItem;

std::string RewardTracer::DatabaseName;

bool RewardTracer::usedfordestruction = false;

std::list< boost::tuple< boost::shared_ptr<std::ostringstream>,
						 boost::shared_ptr<std::ostringstream>,
						 std::list<Ptr<RewardTracer> > > > RewardTracer::g_tracers_reward;

template<class T>
static inline void
NullDeleter (T *ptr)
{
}

void
RewardTracer::Destroy ()
{
  g_tracers_reward.clear();
}


void
RewardTracer::InstallAllSql (sql::Driver* d, const std::string& ip, const std::string& database,
							 const std::string& rtable,
							 const std::string& ptable,
		  	  	  	  	  	 const std::string& user,
							 const std::string& password,
							 const int expidx,
							 const std::string BitRates[],
							 uint32_t TotalBitrate)
{
	using namespace boost;
	using namespace std;

	try{
		DatabaseName = database;
		RewardTracer::usedfordestruction = true;
		driver = d;
		con = driver->connect("tcp://"+ip+":3306", user, password);
		con->setSchema(DatabaseName);
		stmt = con->createStatement();

		RewardTable = rtable;
		PartitionTable = ptable;

		TableId = expidx;

		// The RequestReward in Table(Reward) is the reward per request
		/*stmt->execute("CREATE TABLE IF NOT EXISTS " + RewardTable + " (Node SMALLINT, Transition SMALLINT, " +
				"`BitRate(Kbps)` VARCHAR(6), RequestReward DOUBLE, ExpID SMALLINT);"); */
		std::ostringstream partitionss;
		// The CacheReward in Table(Partition) is the accumulated reward on router (and its upstream routers)
		partitionss << "CREATE TABLE IF NOT EXISTS " + PartitionTable + " (Node SMALLINT, Transition INT, " ;
		for(uint32_t i = 0; i < TotalBitrate; i++)
			partitionss << "`" << BitRates[i] << "` DECIMAL(4,3), ";
		partitionss << "CacheReward DOUBLE, ExpID SMALLINT);";
		stmt->execute(partitionss.str());
		RewardTableItem = 0;
		PartitionTableItem = 0;

	}
	catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
		     << __LINE__ << endl;
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}

	std::list<Ptr<RewardTracer> > tracers;
	boost::shared_ptr<std::ostringstream> OS_reward (new std::ostringstream());
	boost::shared_ptr<std::ostringstream> OS_partition (new std::ostringstream());

	for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
	{
		Ptr<RewardTracer> trace = InstallSql(*node, OS_reward, OS_partition);
		tracers.push_back (trace);
	}
	g_tracers_reward.push_back (boost::make_tuple (OS_reward, OS_partition , tracers));
}


Ptr<RewardTracer>
RewardTracer::InstallSql (Ptr<Node> node,
                   boost::shared_ptr<std::ostringstream> OS_r,
				   boost::shared_ptr<std::ostringstream> OS_p)
{
  NS_LOG_DEBUG ("Node: " << node->GetId ());
  Ptr<RewardTracer> trace = Create<RewardTracer> (OS_r, OS_p, node);

  return trace;
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

RewardTracer::RewardTracer (boost::shared_ptr<std::ostringstream> os_r,
							boost::shared_ptr<std::ostringstream> os_p,
							Ptr<Node> node)
	: m_nodePtr (node)
	, m_rewardStream (os_r)
	, m_partitionStream(os_p)
{
  m_transition = 0;
  m_node = boost::lexical_cast<string> (m_nodePtr->GetId ());
  m_BRinfo = m_nodePtr->GetObject<NDNBitRate>();
  Connect ();
  string name = Names::FindName (node);
  if (!name.empty ())
    {
      m_node = name;
    }
}

RewardTracer::~RewardTracer ()
{
	ClearRewardTable();
	ClearPartitionTable();

	if(RewardTracer::usedfordestruction)
	{
		RewardTracer::usedfordestruction = false;
		delete RewardTracer::stmt;
		delete RewardTracer::con;

		stmt = nullptr;
		con = nullptr;
	}
}

void
RewardTracer::ClearRewardTable()
{
	if(m_rewardStream != nullptr && RewardTableItem != 0)
	{
		RewardTableItem = 0;
		try{
			*m_rewardStream << ";";
			if(!con->isValid())
			{
				delete RewardTracer::stmt;
				stmt = nullptr;
				con->reconnect();
				con->setSchema(DatabaseName);
				stmt = con->createStatement();
			}
			stmt->execute(m_rewardStream->str());
		} catch (sql::SQLException &e) {
			std::cout << "# ERR: SQLException in " << __FILE__;
			std::cout << "(" << __FUNCTION__ << ") on line "
			     << __LINE__ << endl;
			std::cout << "# ERR: " << e.what();
			std::cout << " (MySQL error code: " << e.getErrorCode();
			std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		}
		m_rewardStream->clear();
		m_rewardStream->str("");
	}
}

void
RewardTracer::ClearPartitionTable()
{
	if(m_partitionStream != nullptr && PartitionTableItem != 0)
	{
		PartitionTableItem = 0;
		try{
			*m_partitionStream << ";";

			if(!con->isValid())
			{
				delete RewardTracer::stmt;
				stmt = nullptr;
				con->reconnect();
				con->setSchema(DatabaseName);
				stmt = con->createStatement();
			}
			stmt->execute(m_partitionStream->str());
		} catch (sql::SQLException &e) {
			std::cout << "# ERR: SQLException in " << __FILE__;
			std::cout << "(" << __FUNCTION__ << ") on line "
			     << __LINE__ << endl;
			std::cout << "# ERR: " << e.what();
			std::cout << " (MySQL error code: " << e.getErrorCode();
			std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		}
		m_partitionStream->clear();
		m_partitionStream->str("");
	}
}
void
RewardTracer::Connect ()
{
  Ptr<ContentStore> cs = m_nodePtr->GetObject<ContentStore> ();
  //cs->TraceConnectWithoutContext ("CacheReward", MakeCallback(&RewardTracer::CacheReward, this));
  for(uint32_t i = 1; i <= m_BRinfo->GetTableSize(); i++)
	  m_stats.insert(std::make_pair(m_BRinfo->GetBRFromRank(i), 0));

  cs->TraceConnectWithoutContext("CachePartition", MakeCallback (&RewardTracer::CachePartition, this));
}


void
RewardTracer::Reset ()
{
	for(auto iter = m_stats.begin(); iter != m_stats.end(); iter++)
		iter->second = 0;
}

void
RewardTracer::PrintReward() const
{
  std::string tail = "kbps";
  for(auto iter = m_stats.begin(); iter != m_stats.end(); iter++)
  {
	  if(static_cast<int>(iter->second) == 0)
		  continue;
	  if(m_rewardStream != nullptr)
	  {
		  std::size_t found = iter->first.find(tail);
		  if(RewardTableItem == 0)
			{
				*m_rewardStream << "INSERT INTO " << RewardTable
						<<" VALUES("
				        << m_nodePtr->GetId() << ","
						<< m_transition << ","
				        << std::stoi(iter->first.substr(0, found)) << ","
						<< fixed << setprecision(5) << iter->second << ","
						<< RewardTracer::TableId << ")";
			}
		  else
			{
				*m_rewardStream << ",("
				        << m_nodePtr->GetId() << ","
						<< m_transition << ","
				        << std::stoi(iter->first.substr(0, found)) << ","
						<< fixed << setprecision(5) << iter->second << ","
						<< RewardTracer::TableId << ")";
			}
		  RewardTableItem++;
	  }
	  else
		  NS_LOG_WARN("Log error in CS Tracer !");
  }
  if(m_rewardStream != nullptr && RewardTableItem >= 3000)
  {
	  RewardTableItem = 0;
	  try{
		  *m_rewardStream << ";";
			if(!con->isValid())
			{
				delete RewardTracer::stmt;
				stmt = nullptr;
				con->reconnect();
				con->setSchema(DatabaseName);
				stmt = con->createStatement();
			}
		  stmt->execute(m_rewardStream->str());
	  } catch (sql::SQLException &e) {
			  std::cout << "# ERR: SQLException in " << __FILE__;
			  std::cout << "(" << __FUNCTION__ << ") on line "
				     << __LINE__ << endl;
			  std::cout << "# ERR: " << e.what();
			  std::cout << " (MySQL error code: " << e.getErrorCode();
			  std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		  }
		  m_rewardStream->clear();
		  m_rewardStream->str("");
	}
}

void
RewardTracer::PrintPartition(uint32_t episode, double aggreward, std::shared_ptr<double> arrpar)
{
	//The sequence in partition array (arrpar) must be consistent with the table names in schema.
	uint32_t limit = m_BRinfo->GetTableSize();
	if(m_partitionStream != nullptr)
	{
		  if(PartitionTableItem == 0)
			{
				*m_partitionStream << "INSERT INTO " << PartitionTable
						<<" VALUES("
				        << m_nodePtr->GetId() << ","
						<< episode << ",";
				for(uint32_t i = 0; i < limit; i++)
					*m_partitionStream << arrpar.get()[i] << ",";
				*m_partitionStream << fixed << setprecision(6) << aggreward << ","
						<< RewardTracer::TableId << ")";
			}
		  else
			{
				*m_partitionStream << ",("
				        << m_nodePtr->GetId() << ","
						<< episode << ",";
				for(uint32_t i = 0; i < limit; i++)
					*m_partitionStream << arrpar.get()[i] << ",";
				*m_partitionStream << fixed << setprecision(6) << aggreward << ","
						<< RewardTracer::TableId << ")";
			}
		  PartitionTableItem++;
	  }
	else
		NS_LOG_WARN("Log error in CS Tracer !");

	  if(m_partitionStream != nullptr && PartitionTableItem >= 1)
	  {
		  PartitionTableItem = 0;
		  try{
			  *m_partitionStream << ";";
				if(!con->isValid())
				{
					delete RewardTracer::stmt;
					stmt = nullptr;
					con->reconnect();
					con->setSchema(DatabaseName);
					stmt = con->createStatement();
				}
			  stmt->execute(m_partitionStream->str());
		  } catch (sql::SQLException &e) {
				  std::cout << "# ERR: SQLException in " << __FILE__;
				  std::cout << "(" << __FUNCTION__ << ") on line "
					     << __LINE__ << endl;
				  std::cout << "# ERR: " << e.what();
				  std::cout << " (MySQL error code: " << e.getErrorCode();
				  std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
			  }
		  m_partitionStream->clear();
		  m_partitionStream->str("");
		}


}

void
RewardTracer::CacheReward (uint32_t episode, const std::string& BR, double r)
{
	if(episode > m_transition)
	{
		PrintReward();
		Reset ();
		m_transition = episode;
	}
	m_stats[BR] += r;
}

void
RewardTracer::CachePartition(uint32_t episode, double aggreward, std::shared_ptr<double> partition_arr)
{
	PrintPartition(episode, aggreward, partition_arr);

}



} // namespace ndn
} // namespace ns3
