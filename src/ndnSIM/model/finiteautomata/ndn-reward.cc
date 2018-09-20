#include "ndn-reward.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/ndn-bitrate.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/ndn-content-store.h"
#include "ns3/ndn-dfa-env.h"

#include <list>

NS_LOG_COMPONENT_DEFINE("ndn.TranStatistics");

namespace ns3{
namespace ndn{

NS_OBJECT_ENSURE_REGISTERED(TranStatistics);

TypeId
TranStatistics::GetTypeId()
{
  static TypeId tid = TypeId ("ns3::ndn::TranStatistics")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()
	.AddConstructor<TranStatistics> ()

	.AddAttribute("NumberofChunks",
			"The total number of chunks per video file used in the simulation",
			StringValue("10"),
			MakeUintegerAccessor(&TranStatistics::m_maxNumchunk),
			MakeUintegerChecker<uint32_t>())

	.AddAttribute("NumberOfContents",
			"The total number of video files used in the simulation",
			StringValue("20"),
			MakeUintegerAccessor(&TranStatistics::m_maxNumFile),
			MakeUintegerChecker<uint32_t>())
    ;
  return tid;
}

TranStatistics::TranStatistics()
{

}

TranStatistics::~TranStatistics()
{
	for(uint32_t i = 0; i < m_hitratio.size(); i++)
	{
		delete m_hitratio[i];
	}
}

void TranStatistics::DoDispose()
{
	Object::DoDispose();
}

void TranStatistics::NotifyNewAggregate()
{
	if(m_statptr == 0)
	{
		m_statptr = GetObject<VideoStatistics>();
	}
	if(m_cs == 0)
	{
		m_cs = GetObject<ContentStore>();
	}
	if(m_dfa_env == 0)
	{
		m_dfa_env = GetObject<DPdfa>();
	}
	Object::NotifyNewAggregate();
}

//Apply current strategy: Only consider the delay directly from the content server
void TranStatistics::UpdateDelay(const Name& name, uint32_t signature, const Time& t)
{
	uint32_t flag = -1;
	if(flag == signature)
	{
		DelayHistory::iterator delayiter= m_delay.find(DelayByBitRate {name.get(-3).toUri(), signature});
		if(delayiter != m_delay.end())
		{
			delayiter->second.first +=1;
			delayiter->second.second = (1 - 1 / static_cast<double>(delayiter->second.first)) * delayiter->second.second +
								(1 / static_cast<double>(delayiter->second.first))* t.GetMilliSeconds();
		}
		else
		{
			m_delay[DelayByBitRate {name.get(-3).toUri(), signature}] = std::make_pair(1, t.GetMilliSeconds());
		}
	}

/*
	NS_LOG_INFO("[UpdateDelay] NodeID:" << m_node->GetId() << " for Interest on:" << " FileID:" <<name.get(-2).toNumber() << " ChunkID:"
				<<name.get(-1).toNumber() << "Bitrate:" << name.get(-3).toUri() << "\n"
				<<"Delay:" << m_delay[vi].second<<"ms");
*/
}

void TranStatistics::UpdateHitRatio(uint32_t stateid, const Time& interval)
{
	VideoIndex content;
	if(stateid < m_RewardPerState.size())	//This state is visited before
	{
		m_visits[stateid]++;
		for(auto csiter = m_cs->inCacheRun.begin(); csiter!= m_cs->inCacheRun.end(); csiter++)
		{
			content.Set(*csiter);
			auto i = m_hitratio[stateid]->find(content);
			i->second = (1 - 1 / static_cast<double>(m_visits[stateid])) * i->second +
							(1 / static_cast<double>(m_visits[stateid])) *
							(m_statptr->GetHitNum(content) / static_cast<double>(interval.GetMilliSeconds()));

		}
	}

	else if(stateid == m_RewardPerState.size())
	{
		Hit* hit = new Hit;
		for(auto csiter = m_cs->inCacheRun.begin(); csiter!= m_cs->inCacheRun.end(); csiter++)
		{
			content.Set(*csiter);
			(*hit)[content] = m_statptr->GetHitNum(content) / static_cast<double>(interval.GetMilliSeconds());
		}
		m_hitratio.push_back(hit);
		m_visits.push_back(1);
		m_RewardPerState.push_back(0);
	}
	else
		NS_LOG_ERROR("Index error in TranStatistics::UpdateHitRatio");

	m_statptr->Clear();
}

void TranStatistics::UpdateRewardPerState(uint32_t stateid, const Time& interval)
{
	/*
	 * In UpdateRewardPerState(),
	 * 1. The statistics of hits ratio and delay is updated.
	 * 2. Calculate and sort the reward value per video segment which is allowed to be removed.
	 */
	m_cs->FillinCacheRun();
	UpdateHitRatio(stateid, interval);
	CalReward(stateid);

}

void TranStatistics::CalReward(uint32_t id)
{
	m_RewardPerState[id] = 0;
	VideoIndex content;

	for(auto csiter = m_cs->inCacheRun.begin(); csiter!= m_cs->inCacheRun.end(); csiter++)
	{
		content.Set(*csiter);
		double hit = (*(m_hitratio[id]))[content];
		double delay = m_delay[DelayByBitRate {csiter->get(-3).toUri(), static_cast<uint32_t>(-1)}].second;
		double segreward = EvalVideoSeg(hit, delay, 0);

		m_RewardPerState[id] += segreward;
	}
}

double TranStatistics::EvalVideoSeg(double hitratio, double delay, uint32_t bitrate_lev)
{
	return hitratio;
}

std::shared_ptr<std::vector<VideoIndex> >
TranStatistics::GetRemovableSegment(uint32_t id)
{
	VideoIndex content;
	std::list<std::pair<VideoIndex, double> > rmlist;
	std::shared_ptr<std::vector<VideoIndex> > segmentrank(new std::vector<VideoIndex>);

	for(auto csiter = m_cs->inCacheRun.begin(); csiter!= m_cs->inCacheRun.end(); csiter++)
	{
		content.Set(*csiter);
		if(m_dfa_env->CheckRemovable(content))
		{
			double hit = (*(m_hitratio[id]))[content];
			double delay = m_delay[DelayByBitRate {csiter->get(-3).toUri(), static_cast<uint32_t>(-1)}].second;
			double segreward = EvalVideoSeg(hit, delay, 0);
			rmlist.push_back(std::make_pair(content, segreward));
		}
	}
	uint32_t listsize = rmlist.size();
	for(uint32_t i = 0; i < listsize; i++)
	{
		std::list<std::pair<VideoIndex, double> >::iterator miniter = rmlist.begin();
		for(auto listiter=rmlist.begin(); listiter != rmlist.end(); listiter++)
		{
			if(listiter->second < miniter->second)
				miniter = listiter;
		}
		segmentrank->push_back(miniter->first);
		rmlist.erase(miniter);
	}
	return segmentrank;
}



}
}
