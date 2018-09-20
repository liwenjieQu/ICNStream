/*
 * Author: Wenjie Li
 * Date: 2016-12-02
 */

#include "ndn-agent-dependent.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/random-variable.h"
#include "ns3/ndn-popularity.h"

#include <iomanip>

NS_LOG_COMPONENT_DEFINE("ndn.rl.agent.depedent");

namespace ns3 {
namespace ndn {

DependentMARLState::DependentMARLState(Ptr<Node> nodeptr, uint32_t mode) {
	/*
	 * mode 1: Local: 5%; Agg: 2%; Request: 5%
	 * mode 2: Local: 10%; Agg: 5%; Request: 5%
	 * mode 3: Local: 5%; Agg: 2%; Request: 10%
	 * mode 4: Local: 10%; Agg: 5%; Request: 10%
	 * mode 5: Local: 10%; Agg: 5%; Request: Depends
	 * mode 6: Local: 10%(5%); Agg: 5%(2%); Request: 5%
	 * mode 7: Local: 10%; Agg: 2%; Request: 5%
	 */
	m_node = nodeptr;
	if(mode == 1)
	{
		m_inrequest = Create<BasicMARLState>(nodeptr, 5);
		m_localstate = Create<BasicMARLState>(nodeptr, 5);
	}
	else if(mode == 2)
	{
		m_inrequest = Create<BasicMARLState>(nodeptr, 5);
		m_localstate = Create<BasicMARLState>(nodeptr, 10);
	}
	else if (mode == 3)
	{
		m_inrequest = Create<BasicMARLState>(nodeptr, 10);
		m_localstate = Create<BasicMARLState>(nodeptr, 5);
	}
	else if (mode == 4)
	{
		m_inrequest = Create<BasicMARLState>(nodeptr, 10);
		m_localstate = Create<BasicMARLState>(nodeptr, 10);
	}
	else if (mode == 5)
	{
		if(nodeptr->GetId() < 8)
			m_inrequest = Create<BasicMARLState>(nodeptr, 5);
		else
			m_inrequest = Create<BasicMARLState>(nodeptr, 10);
		m_localstate = Create<BasicMARLState>(nodeptr, 10);
	}
	else if (mode == 6)
	{
		m_inrequest = Create<BasicMARLState>(nodeptr, 5);
		if(nodeptr->GetId() < 8)
			m_localstate = Create<BasicMARLState>(nodeptr, 10);
		else
			m_localstate = Create<BasicMARLState>(nodeptr, 5);
	}
	else if(mode == 7)
	{
		m_inrequest = Create<BasicMARLState>(nodeptr, 5);
		m_localstate = Create<BasicMARLState>(nodeptr, 10);
	}
	else
		NS_LOG_ERROR("Granularity Mode UnRecognized!");
}

DependentMARLState::DependentMARLState() {

}
DependentMARLState::~DependentMARLState() {

}

void DependentMARLState::SetNeighborState(
		Ptr<const DependentMARLState> neighbourPtr) {
	if(neighbourPtr != 0)
	{
		uint32_t id = neighbourPtr->GetAgentID();
		m_neighborstates[id] = neighbourPtr->GetLocalState();
	}
}

/*
 * If ApplyAction returns true, then after \mu seconds, we can call MARL::Transition()
 */
bool DependentMARLState::ApplyAction(
		Ptr<const DependentMARLAction> DepActionPtr) {
	Ptr<const BasicMARLAction> ac = DepActionPtr->GetLocalAction();
	return m_localstate->AdjustCachePartition(ac);
}

void DependentMARLState::UpdateRequest() {
	//Call Video statisitcs module to call the request percentage for video bit-rates;
	double* ratios = new double[m_inrequest->GetNumBR()];
	Ptr<PopularitySummary> popptr = m_node->GetObject<PopularitySummary>();
	if(popptr != 0)
	{
		popptr->SenseVideoRequest(m_inrequest->GetNumBR(), ratios);
		NS_LOG_DEBUG("[DPMARLState]: (Node)" << m_node->GetId() << "; (UpdateRequest) Ratios: " << std::fixed << std::setprecision(2)
					<< ratios[0] << " " << ratios[1] << " " << ratios[2] << " " << ratios[3]);
		m_inrequest->AlignPartitionArr(ratios);
		NS_LOG_DEBUG("[DPMARLState]: (Node)" << m_node->GetId() << "; (UpdateRequest) AlignedRatios: " << std::fixed << std::setprecision(2)
					<< ratios[0] << " " << ratios[1] << " " << ratios[2] << " " << ratios[3]);
	}
	delete [] ratios;
	ratios = nullptr;
}
void
DependentMARLState::Assign(Ptr<const DependentMARLState> sptr)
{
	*(this->m_localstate) = *(sptr->GetLocalState());
	*(this->m_inrequest) = *(sptr->GetRequestStatus());

}
void DependentMARLState::SummarizeNeighborState()
{

}
//////////////////////////////////////////////////////
/////////////DependentMARLAction//////////////////////
//////////////////////////////////////////////////////
DependentMARLAction::DependentMARLAction(Ptr<Node> nodeptr, double bias) {
	m_node = nodeptr;
	m_guidedHeuristics = bias;
	m_localaction = Create<BasicMARLAction>(nodeptr);
}

DependentMARLAction::~DependentMARLAction() {

}

DependentMARLAction::DependentMARLAction() {

}

void DependentMARLAction::SetNeighborAction(
		Ptr<const DependentMARLAction> neighbourPtr) {
	if(neighbourPtr != 0)
	{
		uint32_t id = neighbourPtr->GetAgentID();
		m_neighboractions[id] = neighbourPtr->GetLocalAction();
	}

}

void DependentMARLAction::SummarizeNeighborAction(bool useAggressiveMethod)
{

}

void
DependentMARLAction::Assign(Ptr<const DependentMARLAction> aptr)
{
	*(this->m_localaction) = *(aptr->GetLocalAction());
}


void
DependentMARLAction::RandomSetLocalAction()
{
	UniformVariable rndnum(0,1);
	uint32_t limit = m_localaction->GetNumBR();
	uint32_t n = rndnum.GetInteger(0, limit * (limit -1));

	int8_t* arr = new int8_t [limit];
	bool stopsign = false;

	for(uint32_t i = 0 ; i< limit; i++)
		arr[i] = 0;

	if (n != 0)
	{
		uint32_t idx = 0;
		uint32_t i = 0, j = 0;
		for(i = 0; i < limit; i++)
		{
			for(j = 0; j < limit; j++)
			{
				if(i != j)
				{
					idx++;
					if(idx == n)
					{
						stopsign = true;
						break;
					}
				}
			}
			if(stopsign)
				break;
		}
		arr[i] = 1;
		arr[j] = -1;
	}
	m_localaction->SetChoices(arr);
	delete [] arr;
	arr = nullptr;
}
void
DependentMARLAction::GuidedSetLocalAction()
{
	if(m_node->GetId() < 4)
		this->RandomSetLocalAction();
	else
	{
		UniformVariable rndnum(0,1);
		uint32_t limit = m_localaction->GetNumBR();
		uint32_t n = rndnum.GetInteger(1,17);//Manually configure

		int8_t* arr = new int8_t [limit];

		for(uint32_t i = 0 ; i< limit; i++)
			arr[i] = 0;

		if(n > 2)
		{
			double bias = rndnum.GetValue(0, 1);
			uint32_t idxup;
			if(m_node->GetId() >= 4)
			{
				if(bias <= m_guidedHeuristics)// Select increase partition for high Bitrates
					idxup = rndnum.GetInteger(limit/2, limit - 1);
				else
					idxup = rndnum.GetInteger(0, limit/2 - 1);
			}
			else
			{
				if(bias <= m_guidedHeuristics)// Select increase partition for low Bitrates
					idxup = rndnum.GetInteger(0, limit/2 - 1);
				else
					idxup = rndnum.GetInteger(limit/2, limit - 1);
			}
			arr[idxup] = 1;
			uint32_t idxdown = rndnum.GetInteger(0, limit - 2);
			idxdown = (idxdown >= idxup) ? idxdown + 1 : idxdown;
			arr[idxdown] = -1;
		}

		m_localaction->SetChoices(arr);
		delete [] arr;
		arr = nullptr;
	}
}

void
DependentMARLAction::SetLocalAction(int8_t* arr)
{
	m_localaction->SetChoices(arr);
}

}
}
