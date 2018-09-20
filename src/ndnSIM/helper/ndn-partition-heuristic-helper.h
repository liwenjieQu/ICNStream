/*
 * ndn-partition-heuristic-helper.h
 * A simple heuristic design for cache partitioning (not MARL project)
 * To shut up reviewers
 *  Created on: 2018-03-11
 *      Author: wenjie
 */

#ifndef NDN_PARTITION_HEURISTIC_PARTITION_H_
#define NDN_PARTITION_HEURISTIC_PARTITION_H_

#include "ns3/ptr.h"
#include "ns3/nstime.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ndn-net-device-face.h"

#include "ns3/ndn-videocontent.h"
#include "ns3/ndn-videostat.h"
#include "ns3/ndn-bitrate.h"

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>

#include <tuple>
#include <unordered_map>
#include <map>
#include <list>
#include <deque>
#include <set>
#include <utility>
#include <fstream>

#include <boost/lexical_cast.hpp>

namespace ns3{
namespace ndn{


class HeuPartitionHelper
{
	using DependencySetDict = std::map<uint32_t, std::deque<Ptr<Node> > >;
	using CandidateTable = std::map<uint32_t, std::set<VideoIndex> >;
	using DelayTable = std::unordered_map<DelayTableKey, double>;
public:
	HeuPartitionHelper(const std::string& prefix,
					   const std::string& roundtime,
					   double rewardparam,
	   	   	   	   	   uint32_t B,
					   uint32_t iterationTimes,
					   uint32_t mode,
					   sql::Driver* driver,
					   const std::string& DatabaseName,
					   const std::string& user,
					   const std::string& password,
					   const std::string& ip,
					   int port,
					   uint32_t expid);

	~HeuPartitionHelper();

	void
	SetTopologicalOrder(Ptr<Node> ServerNode);

	void
	SetTopologicalOrder(NodeContainer EdgeNodes);

	void
	IterativeRun();


private:
	// Topology Search AND Discover Dependency Set (Coordination Graph) for Each Node
	void BreadthFirstSearch(std::deque<Ptr<Node> >&);
	void DepthFirstSearch(Ptr<Node>, std::deque<Ptr<Node> >&);

private:
	/*
	 * Push Content into Cache Stack when there is available capacity
	 */
	void AssignCacheStack(Ptr<Node>,
						  uint32_t,
						  std::list<std::pair<VideoIndex, double> >&);

	void AdjustCacheStack(Ptr<Node>,
					  	  uint32_t,
						  std::list<std::pair<VideoIndex, double> >&,
						  std::map<uint32_t, std::list<std::pair<VideoIndex, double> > >&);

	bool AllowCacheMore(Ptr<NDNBitRate>,
						uint32_t,
						uint32_t,
						uint32_t,
			  	  	  	const std::map<uint32_t, std::list<std::pair<VideoIndex, double> > >&);

	// Maintain the Cache Stack
	void DeriveCacheStackByPath();

private:
	void MergeCacheStack(Ptr<Node>,
						 std::unordered_map<VideoIndex,double>&,
						 CandidateTable&);

	bool AdjustStackSize(Ptr<Node>,
						 std::unordered_map<VideoIndex,double>&,
						 CandidateTable&);

	void
	Solver();

	bool
	CheckIterationCondition();

	void
	Initialization();

	void OutputCacheStatus(uint32_t);

private:
	void PlacementByPop();
	void FindInitCachePartition(Ptr<Node>,
								std::unordered_map<VideoIndex,double>&,
								std::set<VideoIndex>&);
	void CalPopScore(Ptr<Node>,
					 uint32_t,
					 const std::set<VideoIndex>&,
					 std::unordered_map<VideoIndex,double>&);

	void UpdateRequestStat();

	uint64_t SetBRBoundary(Ptr<Node> edge, const std::string& reqbr);

	double	RippleRatio(uint32_t, uint32_t, uint64_t);

private:
	void PathCacheByPop();

private:
	Ptr<Node>						m_producer;
	NodeContainer					m_Producers;
	std::list<Ptr<Node> > 			m_topo_order_ptr; 	// Topology Order (For Input and Output)
	std::list<Ptr<Node> >			m_edgenode_ptr;	  	// Edge Routers;
	DependencySetDict				m_UpstreamSet;		// Upstream Dependent Nodes
	DependencySetDict 				m_DownstreamSet;	// Downstream Dependent Nodes

private:
	std::string						m_prefix;
	std::string						m_roundtime;
	uint32_t						NumB;
	uint32_t						m_expid;

	uint32_t						m_iterationTimes;
	uint32_t						m_itercounter;

	double							m_alpha;
	uint32_t						m_design;

	sql::Connection*				m_con;
	sql::Statement*					m_stmt;

	//Key: each edge router; Value: cache stack viewed from each routing path
	std::unordered_map<uint32_t, std::list<std::pair<VideoIndex, double> > > 	m_cachestack;
	//Key: each edge router; Value: available cache capacity which can be utilized by routing path
	std::unordered_map<uint32_t, std::vector<uint32_t> >	m_availcapacity;
	//Key: each router; Value: cache placement on each router
	std::unordered_map<uint32_t, std::set<VideoIndex> >	m_decision;

	DelayTable											m_delaytable;

	//std::unordered_map<uint32_t, std::map<std::string, double> >	m_partition;
	//std::unordered_map<uint32_t, std::map<std::string, double> >	m_lastpartition;
	std::list<double> m_condition;

};

}
}

#endif
