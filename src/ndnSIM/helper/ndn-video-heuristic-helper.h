/*
 * Author by Wenjie Li
 * Data: 2015-10-08
 *
 */

#ifndef NDN_VIDEO_HEURISTIC_HELPER_H
#define NDN_VIDEO_HEURISTIC_HELPER_H

#include "ns3/ptr.h"
#include "ns3/nstime.h"
#include "ns3/ndn-name.h"
#include "ns3/node-container.h"
#include "ns3/ndn-l3-protocol.h"
#include "ns3/node.h"

#include <vector>
#include <string>
#include <fstream>

namespace ns3{

namespace ndn{

class DASHeuristicHelper
{
public:
	DASHeuristicHelper(const std::string& method,
					   const std::string& prefix,
					   const NodeContainer& edgenodes,
					   const NodeContainer& intmnodes,
					   const std::string& roundtime,
					   uint32_t iterationlimit);
	~DASHeuristicHelper();

	void Run();

private:
	void PrintCache();

	void RunHeuristic();
	/*
	 * check whether the downstream routers (along routing paths) are in Marked status
	 *
	 * If all routers are marked, return true; otherwise, return false
	 */
	bool CheckDownStreamStatus(Ptr<Node> node, Ptr<L3Protocol> ndn);
	/*
	 * Aggregate the statisitcs summary and cache decisions from downstream routers of 'node'
	 */
	void AggregateInfo(Ptr<Node> node, Ptr<L3Protocol> ndn);
	/*
	 * Clear the cached contents on all routers for DAS Heuristic algorithm
	 */
	void Preprocessing();
	/*
	 * Clear the statistics and the summary in order to start a new round
	 */
	void Postprocessing();

	EventId			m_PeriodicEvent;
	Ptr<Name> 		m_prefix;
	NodeContainer	m_edgenodes;
	NodeContainer	m_intmnodes;
	std::string		m_method;
	std::string		m_roundtime;

	uint32_t 		m_times = 0;
	uint32_t 		m_limit;

};


}
}





#endif
