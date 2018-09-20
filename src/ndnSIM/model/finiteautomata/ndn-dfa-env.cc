#include "ndn-dfa-env.h"
#include "ns3/ndn-content-store.h"
#include "ns3/ndn-bitrate.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/ndn-videocontent.h"
#include "ns3/ndn-reward.h"
#include "ns3/name.h"
#include "ns3/string.h"
#include "ns3/ndn-name.h"

#include <cmath>

NS_LOG_COMPONENT_DEFINE("ndn.DPdfa");
namespace ns3{
namespace ndn{

NS_OBJECT_ENSURE_REGISTERED (DPdfa);

TypeId
DPdfa::GetTypeId()
{
  static TypeId tid = TypeId ("ns3::ndn::DPdfa")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()
	.AddConstructor<DPdfa> ()
    .AddAttribute ("TotalChunk","The max number of chunks in each video file",
                   UintegerValue (10),
				   MakeUintegerAccessor (&DPdfa::TotalChunk),
				   MakeUintegerChecker<uint32_t>())

	.AddAttribute ("DFAFile","The number of video files considered in the DFA",
				   UintegerValue (10),
				   MakeUintegerAccessor (&DPdfa::DFAFile),
				   MakeUintegerChecker<uint32_t>())

	.AddAttribute ("SkewnessFactor", "parameter of power",
				   DoubleValue (0.7),
				   MakeDoubleAccessor (&DPdfa::Skewness),
				   MakeDoubleChecker<double> ())

	.AddAttribute ("Prefix","Name of the Interest",
				   StringValue ("/"),
				   MakeStringAccessor (&DPdfa::m_prefix),
				   MakeStringChecker ())

	.AddAttribute ("DiscountFactor", "Discount of Dynamic Programming",
			       DoubleValue (0.95),
				   MakeDoubleAccessor (&DPdfa::m_discount),
				   MakeDoubleChecker<double> ())

    ;
  return tid;
}

DPdfa::DPdfa()
	:m_currentid(0),
	 m_sizestates(1)
{
	//Generate the state 0: empty
	MDPState init(m_currentid, TotalChunk);
	m_current_iter = m_states.insert(std::move(init)).first;
}

DPdfa::~DPdfa()
{
	Simulator::Cancel (m_event);
}

void
DPdfa::DoDispose()
{
	Object::DoDispose();
}

void
DPdfa::NotifyNewAggregate()
{
	if(m_cs == 0)
		m_cs = GetObject<ContentStore>();
	if(m_bitinfo == 0)
	{
		m_bitinfo = GetObject<NDNBitRate>();
		//m_requestrate = 1 / m_bitinfo->GetPlaybackTime();
	}
	if(m_history == 0)
		m_history = GetObject<TranStatistics>();
	Object::NotifyNewAggregate();
}

uint32_t
DPdfa::ZipfFileSelect()
{
	uint32_t content_index = 1; //[1, m_N]
	double p_sum = 0;

	double p_random = m_SeqRng.GetValue();
	while (p_random == 0)
	{
		p_random = m_SeqRng.GetValue();
	}
	for (uint32_t i=1; i<=DFAFile; i++)
	{
		p_sum = m_Pcum[i];   //m_Pcum[i] = m_Pcum[i-1] + p[i], p[0] = 0;   e.g.: p_cum[1] = p[1], p_cum[2] = p[1] + p[2]
		if (p_random <= p_sum)
		{
			content_index = i;
			break;
		} //if
	} //for
	  //content_index = 1;
	NS_LOG_DEBUG("RandomNumber="<<content_index);
	return content_index;
}


void
DPdfa::PrepareTransProb ()
{
	m_Pcum = std::vector<double> (DFAFile + 1);
	m_Pcum[0] = 0.0;
	for (uint32_t i=1; i <= DFAFile; i++)
    {
		m_Pcum[i] = m_Pcum[i-1] + 1.0 / std::pow(i, Skewness);
    }
	for (uint32_t i=1; i<=DFAFile; i++)
    {
      m_Pcum[i] = m_Pcum[i] / m_Pcum[DFAFile];
      NS_LOG_LOGIC ("Cumulative probability [" << i << "]=" << m_Pcum[i]);
    }
}

void
DPdfa::Transition()
{
	/*
	 *	Based on the current state in DFA, determining the next action
	 */
	Action current_action = SimulationSeq(m_currentid);
	/*
	 * 	Update g(i,u,j), the reward value per state.
	 * 	This value is used in the later total reward calculation
	 */
	m_history->UpdateRewardPerState(m_currentid, m_interval);
	/*
	 * Two cases of actions
	 * Case 1:
	 * Stay in the current state------> m_currentid and iterator won't change
	 */
	if(current_action.first == CacheOperator::NoReplace)
	{
		NS_LOG_INFO("[DFA Transition] Apply No Replacement");
		UpdateTotalReward(m_currentid, m_currentid, current_action);
		return;
	}

	/*
	 * Case 2:
	 * Transit to another state
	 */
	bool needreplace = false;
	double available_space = m_cs->GetCapacity() - m_cs->GetCurrentSize() -
			m_bitinfo->GetChunkSize(current_action.second.substr(2)) * 1e3;
	if(available_space < 0)
		needreplace = true;

	bool needrollback = false;
	if(m_cs->GetCapacity() < m_bitinfo->GetChunkSize(current_action.second.substr(2)) * 1e3)
		needrollback = true;

	if(needrollback)
	{
		NS_LOG_INFO("[DFA Transition] Need Rollback: the size of added content is huge");
		UpdateTotalReward(m_currentid, m_currentid, current_action);
		return;
	}

	/*
	 * Select the file id for the new cache video segment.
	 * In the real scenario, fileID shall be calculated based on the real-time statistics.
	 * Use a Zipf distribution to simplify the simulation.
	 */
	uint32_t fileID = ZipfFileSelect();

	VideoIndex nextseg(current_action.second, fileID, 0);
	MDPState::BlockRange range = m_current_iter->GetChunkRange(nextseg.m_bitrate, nextseg.m_file);

	if(range.first != 0)
	{
		if(current_action.first == CacheOperator::AttachEnd)
		{
			NS_LOG_INFO("[DFA Transition] Attach content END");
			nextseg.m_chunk = range.second;
		}
		else if(current_action.first == CacheOperator::AttachFront)
		{
			NS_LOG_INFO("[DFA Transition] Attach content FRONT");
			nextseg.m_chunk = range.first - 1;
		}
	}
	else
		nextseg.m_chunk = 1;

	MDPState newstate(m_sizestates, TotalChunk);
	newstate = *m_current_iter;
	bool insertok = newstate.AddContent(nextseg);

	if(!insertok)
	{
		NS_LOG_WARN("[DFA Transition] BUG in MDPState ADDcontent!");
		/*
		 * DFA is applying an action which may break the law of video caching system
		 * This action will have the same effect as 'NoReplace'
		 */
		UpdateTotalReward(m_currentid, m_currentid, current_action);
		return;
	}

	std::vector<VideoIndex>* NeedToDelete;
	if(needreplace)
	{
		NeedToDelete = new std::vector<VideoIndex>;

		//Find low rank from content store
		std::shared_ptr<std::vector<VideoIndex> > segrank = m_history->GetRemovableSegment(m_currentid);
		std::vector<VideoIndex>::iterator checkptr = segrank->begin();
		while(available_space < 0)
		{
			//Delete from MDPState
			//Delete from Content Store
			if(checkptr != segrank->end())
			{
				NeedToDelete->push_back(*checkptr);
				newstate.RemoveContent(*checkptr);
				available_space += (m_bitinfo->GetChunkSize(checkptr->m_bitrate.substr(2))) * 1e3;
				checkptr++;
			}
		}

	}

	auto stateiter = m_states.find(newstate);
	if(stateiter != m_states.end())
	{
		m_current_iter = stateiter;
		UpdateTotalReward(m_currentid, m_current_iter->GetID(), current_action);
		m_currentid = m_current_iter->GetID();
	}
	else
	{
		m_current_iter = m_states.insert(std::move(newstate)).first;
		UpdateTotalReward(m_currentid, m_sizestates, current_action);
		m_currentid = m_sizestates;
		m_sizestates++;
	}
	/*
	 * Pending actions on Content Store now are applied
	 */
	//Delete from Content Store
	//Add content to content store
	if(needreplace)
	{
		for(size_t i = 0; i < NeedToDelete->size(); i++)
			m_cs->RemoveByMDP(VideoIndexToName((*NeedToDelete)[i]));
	}
	//TODO: Add to Content Store after receiving the first Interest
	m_cs->AddByMDP(VideoIndexToName(nextseg));

	Move();
}

Ptr<Name>
DPdfa::VideoIndexToName(const VideoIndex& vi)
{
	Ptr<Name> nameinCS = Create<Name>(m_prefix);
	nameinCS->append(vi.m_bitrate);
	nameinCS->appendNumber(vi.m_file);
	nameinCS->appendNumber(vi.m_chunk);
	return nameinCS;
}

void
DPdfa::UpdateTotalReward(uint32_t currentid, uint32_t nextid, const Action& action)
{
	QFactor op(currentid, action);
	if(m_visits.size() > currentid)
	{
		m_visits[currentid]++;
		m_reward[op] = (1 - 1 / static_cast<double>(m_visits[currentid])) * m_reward[op] +
				(1 / static_cast<double>(m_visits[currentid])) * TranMapping(currentid, nextid);

	}
	else if(m_visits.size() == currentid)
	{
		m_reward[op] = TranMapping(currentid, nextid);
		m_visits.push_back(1);
	}
	else
		NS_LOG_ERROR("[DFA UpdateReward] Lose track of state id");
	/*
	 * Change on the QValue may influence the optimal policy.
	 * Update policy every time we update the reward.
	 */
	UpdateOptimalPolicy(QFactor{currentid, action});

}

double
DPdfa::TranMapping(uint32_t currentid, uint32_t nextid)
{
	double r = m_history->GetRewardPerState(currentid);
	if(m_policy.size() > nextid)
	{
		r += m_discount * m_reward[QFactor{nextid, m_policy[nextid]}];
	}
	return r;
}

void
DPdfa::UpdateOptimalPolicy(const QFactor& qf)
{
	if(m_policy.size() > qf.m_stateid)
	{
		if(m_reward[qf] > m_reward[QFactor{qf.m_stateid, m_policy[qf.m_stateid]}])
			m_policy[qf.m_stateid] = qf.m_action;
	}
	else if(m_policy.size() == qf.m_stateid)
	{
		m_policy.push_back(qf.m_action);
	}
	else
		NS_LOG_ERROR("Transition Goes wrong");
}

DPdfa::Action
DPdfa::SimulationSeq(uint32_t id)
{
	//TODO: Generate a simulation sequence which explores MDP states many times
	Action current_action = std::make_pair(CacheOperator::NoReplace, " ");
	return current_action;
}

bool
DPdfa::CheckRemovable(const VideoIndex& vi)
{
	return m_current_iter->SatisfyBaseLaw(vi);
}

void
DPdfa::StartDFA()
{
	Move();
}

void
DPdfa::Move()
{
	// TODO: Determine the interval between transitions
	m_interval = Seconds(m_bitinfo->GetPlaybackTime());
	m_event = Simulator::Schedule(m_interval, &DPdfa::Transition, this);
}


}
}
