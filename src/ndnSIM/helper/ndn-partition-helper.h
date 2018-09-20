/*
 * Author: Wenjie Li
 * Date: 2017-02-23
 * Global Optimization for Cache Capacity Partition (Using Gurobi)
 */

// Change GCC Version to 5.2
// sudo update-alternatives --config gcc
#ifndef NDN_PARTITION_HELPER_H
#define NDN_PARTITION_HELPER_H

#include "ns3/ptr.h"
#include "ns3/nstime.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "gurobi_c++.h"

#include "ns3/ndn-videocontent.h"
#include "ns3/ndn-videostat.h"

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>

#include <tuple>
#include <unordered_map>
#include <map>
#include <deque>
#include <utility>
#include <fstream>

namespace ns3{
namespace ndn{

class VarIndex; // X_j(f, k, b) Variable Index
class PathVarIndex; // \alpha_j(L(m), f, k, b) Path Variable Index
class DelayTableKey;

class PartitionHelper
{
	using NodesOnPathList = std::map<uint32_t, std::deque<Ptr<Node> > >;
	// Tuple: Bitrate(String), Hop(uint32_t), EdgeNodeID(uint32_t)
	using DelayTable = std::unordered_map<DelayTableKey, double>;
	using PopularityTable = std::map<std::string, std::vector<std::pair<VideoIndex,uint64_t> > >;
public:
	PartitionHelper(uint32_t NumF, uint32_t NumC, uint32_t NumB, uint32_t NumNodes,
					const std::string ArrB[],
					uint32_t EdgeSize, uint32_t IntmSize,
					const std::string& prefix,
					const std::string& roundtime,
					double alpha, uint32_t id, uint32_t designchoice,
					uint32_t iterations,
					const std::string& logpath,
					sql::Driver* driver,
					const std::string& DatabaseName,
					const std::string& user,
					const std::string& password,
					const std::string& ip,
					int port);

	~PartitionHelper();

	void Solver();
	void IterativeRun();

	void
	SetTopologicalOrder(Ptr<Node> ServerNode, NodeContainer edges);

private:
	int SolveIntegerPro();
	/* Related to Gruobi Optimization */

	void GRBModelInit();
	void AddOptVar();
	// Each Edge router should aggregate class VideoStatistics
	void UpdateRequestStat();
	void SetOptimizationObj();
	void SetOptimizationConstr();

private:
	/* Related to processing topology */
	void
	DepthFirstSearch(Ptr<Node>, std::deque<Ptr<Node> >&);

	uint64_t
	SetBRBoundary(Ptr<Node> edge);

	uint64_t
	SetBRBoundary(Ptr<Node> edge, const std::string& reqbr);

	double
	CalReward(uint64_t& bd, uint32_t reqBR, Ptr<Node> edge);

	void
	OrderPopTable(Ptr<VideoStatistics>,  PopularityTable&);

	/* For Debugging: Result Verification */
	void
	VerifyOptResult(const std::unordered_map<uint32_t, std::vector<Name> >&);

	/* Only used to verify the design of MARL learning */
	void
	VerifyMARLDesign();

	bool
	CheckIterationCondition();

	void OutputCacheStatus(uint32_t);

private:
	GRBEnv* env;
	GRBModel* model;

	uint32_t 		m_NumOfChunks;
	uint32_t		m_NumOfFiles;
	uint32_t		m_NumofBitRates;
	uint32_t		m_NumofNodes;
	std::string*	m_BRnames;

	uint32_t		m_edgesize;
	uint32_t 		m_intmsize;

	uint64_t		m_totalreq;
	std::string		m_prefix;
	std::string		m_roundtime;

	std::unordered_map<VarIndex, GRBVar> 		m_varDict;
	std::unordered_map<PathVarIndex, GRBVar>	m_pathvarDict;
	DelayTable									m_delaytable;

	NodesOnPathList m_forwardingpath;		//The Array of NodeID on the forwarding path starting from a certain edge router
											//This Array is in reverse order (including the edge itself)
	NodeContainer	m_edgenodes; 			//Edge routers

	uint32_t		m_longestRoutingHop = 5;//Assume We know the longest routing path and its corresponding hopcount
	uint32_t		m_iterationTimes;
	uint32_t		m_itercounter;

	double			m_alpha;
	uint32_t		m_design;
	uint32_t		m_expid;

	std::ofstream* 	m_log;
	std::list<double> m_condition;

	sql::Connection*				m_con;
	sql::Statement*					m_stmt;
};

bool cmp_by_value_video(const std::pair<ns3::ndn::VideoIndex, uint64_t>& lhs,
		const std::pair<ns3::ndn::VideoIndex, uint64_t>& rhs);
}
}

#endif




