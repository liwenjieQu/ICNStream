/*
 * Author: Wenjie Li
 * Date: 2016-12-03
 */

/*
 * ndn-agent-aggrgate.h defines the approximate (dependent) state and action
 * This approximation is based on the aggregation.
 * class AggregateMARLState has to work with Greedy policy in one video-producer scenario, in ndn-marl-greedy.h
 */

#ifndef NDN_RL_AGENT_AGGREGATE_H
#define NDN_RL_AGENT_AGGREGATE_H

#include "ndn-agent-dependent.h"

namespace ns3 {
namespace ndn {

class AggregateMARLState: public DependentMARLState {
	using NeighborState = std::map<uint32_t, Ptr<const BasicMARLState> >;
public:

	static std::string
	GetName() { return "AggregatedState"; }

	AggregateMARLState(Ptr<Node> nodeptr, uint32_t mode = 1);
	virtual ~AggregateMARLState() {};
	/*
	 * SummarizeNeighborState aggregates the states of dependent agents into one single state
	 * Thus, m_neighborstates contains H(i) and aggregated state i
	 */
	virtual void SummarizeNeighborState();

private:
	Ptr<BasicMARLState> m_agg_state;

};

class AggregateMARLAction: public DependentMARLAction {
	using NeighborAction = std::map<uint32_t, Ptr<const BasicMARLAction> >;
public:

	static std::string
	GetName() { return "AggregatedAction"; }

	AggregateMARLAction(Ptr<Node> nodeptr);
	virtual ~AggregateMARLAction() {};

	virtual void SummarizeNeighborAction(bool);

private:
	Ptr<BasicMARLAction> m_agg_action;

};

}
}

#endif
