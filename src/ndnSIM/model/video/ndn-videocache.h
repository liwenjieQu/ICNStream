/*
 * Author by Wenjie Li
 * Date: 2015-10-07
 */

#ifndef NDN_VIDEOCACHE_H
#define NDN_VIDEOCACHE_H

#include <vector>
#include <unordered_map>
#include <map>
#include <string>

#include "ns3/ptr.h"
#include "ns3/object.h"

namespace ns3{
class Node;
namespace ndn{

class Name;
class ContentStore;
class PopularitySummary;
class DecisionIndex;

class VideoCacheDecision : public Object
{
public:
	static TypeId GetTypeId ();

	VideoCacheDecision();
	virtual ~VideoCacheDecision();
	void AggregateDecision(const VideoCacheDecision&);
	void GreedyChoice();
	void Clear();
protected:
  // inherited from Object class
  virtual void NotifyNewAggregate (); ///< @brief Even when object is aggregated to another Object
  virtual void DoDispose (); ///< @brief Do cleanup

private:
	Ptr<PopularitySummary> 	m_stat_summary;
	Ptr<ContentStore> 		m_cs;
	Ptr<Node>				m_node;
	Name     				m_prefixName;
//	std::vector<ns3::ndn::Name> 					m_decision;
	std::unordered_map<DecisionIndex, uint32_t> 	m_table;

private:
	void AddToDecision(const std::string&, uint32_t, uint32_t);
	std::string RetrieveBitrate(const std:: string&);
};


}
}

#endif
