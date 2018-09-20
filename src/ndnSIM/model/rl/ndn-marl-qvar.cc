#include "ns3/log.h"
#include "ndn-marl-qvar.h"

NS_LOG_COMPONENT_DEFINE ("ndn.rl.marl.qvar");

namespace ns3 {
namespace ndn {


QVariable::QVariable(Ptr<Node> nodeptr,
		Ptr<DependentMARLState> s, Ptr<DependentMARLAction> a)
{
	Ptr<const ns3::ndn::BasicMARLState> sptr;
	const int* sarr;

	sptr = s->GetLocalState();
	sarr = sptr->GetIntPartition();

	for(uint32_t i = 0; i < sptr->GetNumBR(); i++)
		m_state_arr.push_back(sarr[i]);


	sptr = s->GetRequestStatus();
	sarr = sptr->GetIntPartition();

	for(uint32_t i = 0; i < sptr->GetNumBR(); i++)
		m_state_arr.push_back(sarr[i]);

	auto siter = s->GetNeighborStates().find(s->GetAgentID());
	if(siter != s->GetNeighborStates().end())
	{
		sptr = siter->second;
		sarr = sptr->GetIntPartition();

		for(uint32_t i = 0; i < sptr->GetNumBR(); i++)
			m_state_arr.push_back(sarr[i]);

	}
	else
	{
		for(auto iter = s->GetNeighborStates().begin(); iter != s->GetNeighborStates().end(); iter++)
		{
			sptr = iter->second;
			sarr = sptr->GetIntPartition();

			for(uint32_t i = 0; i < sptr->GetNumBR(); i++)
				m_state_arr.push_back(sarr[i]);
		}
	}

	ns3::Ptr<const ns3::ndn::BasicMARLAction> aptr;
	const int8_t* aarr;

	aptr = a->GetLocalAction();
	aarr = aptr->GetChoices();

	for(uint32_t i = 0; i < aptr->GetNumBR(); i++)
		m_action_arr.push_back(aarr[i]);


	auto aiter = a->GetNeighborActions().find(a->GetAgentID());
	if(aiter != a->GetNeighborActions().end())
	{
		aptr = aiter->second;
		aarr = aptr->GetChoices();

		for(uint32_t i = 0; i < aptr->GetNumBR(); i++)
			m_action_arr.push_back(aarr[i]);

	}
	else
	{
		for(auto iter = a->GetNeighborActions().begin(); iter != a->GetNeighborActions().end(); iter++)
		{
			aptr = iter->second;
			aarr = aptr->GetChoices();

			for(uint32_t i = 0; i < aptr->GetNumBR(); i++)
				m_action_arr.push_back(aarr[i]);
		}
	}
}

QVariable::QVariable(const std::vector<int8_t>& state, const std::vector<int8_t>& action)
{
	for(uint32_t i = 0; i < state.size(); i++)
		m_state_arr.push_back(state[i]);
	for(uint32_t i = 0; i < action.size(); i++)
		m_action_arr.push_back(action[i]);
}

QVariable::~QVariable()
{

}

bool
QVariable::operator==(const QVariable& index) const
{
	if(m_state_arr.size() != index.m_state_arr.size()
			|| m_action_arr.size() != index.m_action_arr.size())
		return false;

	for(uint32_t i = 0 ; i < m_state_arr.size(); i++)
	{
		if(m_state_arr[i] != (index.m_state_arr)[i])
			return false;
	}

	for(uint32_t i = 0 ; i < m_action_arr.size(); i++)
	{
		if(m_action_arr[i] != (index.m_action_arr)[i])
			return false;
	}
	return true;
}

std::size_t
QHasher::operator()(const QVariable& index) const{
	std::size_t seed = 0;
	boost::hash_combine(seed, boost::hash_value(index.m_state_arr.size()));
	boost::hash_combine(seed, boost::hash_value(index.m_action_arr.size()));

	for(uint32_t j = 0; j < index.m_state_arr.size(); j++)
		boost::hash_combine(seed, boost::hash_value((index.m_state_arr)[j]));

	for(uint32_t j = 0; j < index.m_action_arr.size(); j++)
		boost::hash_combine(seed, boost::hash_value((index.m_action_arr)[j]));

	return seed;
}

}
}
