/*
 * Author: Wenjie Li
 * Date: 2016-12-03
 */

#include "ndn-agent-aggregate.h"
#include "ns3/ndn-agent-basic.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("ndn.rl.agent.aggregate");

namespace ns3 {
namespace ndn {

AggregateMARLState::AggregateMARLState(Ptr<Node> nodeptr, uint32_t mode)
	:DependentMARLState(nodeptr, mode)
{
	if(mode == 1 || mode == 3 || mode == 7)
	//The granularity of aggregate state is set to be 0.02
		m_agg_state = Create<BasicMARLState>(nodeptr, 2);
	else if(mode == 2 || mode == 4 || mode == 5 || mode == 6)
		m_agg_state = Create<BasicMARLState>(nodeptr, 5);
	else
		NS_LOG_ERROR("Granularity Mode UnRecognized!");
}

void
AggregateMARLState::SummarizeNeighborState()
{
	uint32_t localBRnum = GetLocalState()->GetNumBR();
	double* aggBRsize = new double [localBRnum];
	double sum = 0;
	for(uint32_t i = 0; i < localBRnum; i++)
		aggBRsize[i] = 0;

	for(auto iter = m_neighborstates.begin(); iter!= m_neighborstates.end(); iter++)
	{
		if(iter->first != GetAgentID())
		{
			if(iter->second->GetNumBR() == localBRnum)
			{
				for(uint32_t i = 0 ; i< localBRnum; i++)
				{
					aggBRsize[i] += iter->second->GetSizeFromPartition(i);
					sum += iter->second->GetSizeFromPartition(i);
				}
			}
			//else is not considered yet. We assume the each router consider the same set of bit-rate
		}
	}
	if(static_cast<int>(sum) != 0)
	{
		for(uint32_t i = 0; i < localBRnum; i++)
			aggBRsize[i] = aggBRsize[i] / sum;

		m_agg_state->AlignPartitionArr(aggBRsize);
	}
	else
		m_agg_state->SetPartition(aggBRsize);

	m_neighborstates[GetAgentID()] = ConstCast<BasicMARLState>(m_agg_state);

	delete [] aggBRsize;
	aggBRsize = nullptr;
}

////////////////////////////////////////////
/////////////AggregateMARLAction///////////
///////////////////////////////////////////

AggregateMARLAction::AggregateMARLAction(Ptr<Node> nodeptr)
	:DependentMARLAction(nodeptr)
{
	m_agg_action = Create<BasicMARLAction>(nodeptr);
}

void
AggregateMARLAction::SummarizeNeighborAction(bool useAggressiveMethod)
{
	uint32_t localBRnum = GetLocalAction()->GetNumBR();
	int8_t* aggaction = new int8_t [localBRnum];
	for(uint32_t i = 0; i < localBRnum; i++)
		aggaction[i] = 0;

	for(auto iter = m_neighboractions.begin(); iter!= m_neighboractions.end(); iter++)
	{
		if(iter->first != GetAgentID())
		{
			if(iter->second->GetNumBR() == localBRnum)
			{
				const int8_t* neighbor_local = iter->second->GetChoices();
				for(uint32_t i = 0; i < localBRnum; i++)
					aggaction[i] += neighbor_local[i];
			}
			//else is not considered yet. We assume the each router consider the same set of bit-rate
		}
	}

	// Experimental Method: Aggressive Aggregation
	if(useAggressiveMethod)
	{
		for(uint32_t i = 0; i < localBRnum; i++)
		{
			if(aggaction[i] > 0)
				aggaction[i] = 1;
			else if(aggaction[i] < 0)
				aggaction[i] = -1;
		}
	}

	m_agg_action->SetChoices(aggaction);
	m_neighboractions[GetAgentID()] = ConstCast<BasicMARLAction>(m_agg_action);

	delete [] aggaction;
	aggaction = nullptr;
}

}
}
