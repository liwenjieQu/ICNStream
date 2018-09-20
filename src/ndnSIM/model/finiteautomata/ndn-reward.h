/*
 * Author: Wenjie Li
 * Date: 2016-03-29
 */

#ifndef NDN_REWARD_H
#define NDN_REWARD_H

#include <map>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>
#include <memory>

#include "ns3/ptr.h"
#include "ns3/name.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ndn-videostat.h"
#include "ns3/ndn-videocontent.h"

namespace ns3{
namespace ndn{
class DelayByBitRate
{
public:
	DelayByBitRate(const std::string& br, uint32_t id)
		:m_bitrate(br),
		 m_sourceId(id)
	{}
	inline bool operator==(const DelayByBitRate& delay) const
	{
		return this->m_bitrate == delay.m_bitrate && this->m_sourceId == delay.m_sourceId;
	}
public:
	std::string		m_bitrate;
	uint32_t		m_sourceId;
};
}
}

namespace std{
	template<>
	struct hash<ns3::ndn::DelayByBitRate>
	{
		std::size_t operator()(const ns3::ndn::DelayByBitRate& delay) const{
		std::size_t seed = 200;
		boost::hash_combine(seed, boost::hash_value(delay.m_sourceId));
		boost::hash_combine(seed, boost::hash_value(delay.m_bitrate));
		return seed;
	}
};
}

namespace ns3{
class Node;
namespace ndn{

class ContentStore;
class DPdfa;
/*
 * The calculation of rewards influence the performance of our MDP system!
 */
class TranStatistics : public Object
{
public:
	using Hit = std::map<VideoIndex, double>;
	using HitHistory = std::vector<Hit*>;
	using DelayHistory = std::unordered_map<DelayByBitRate, std::pair<uint64_t,double> >;

	static TypeId GetTypeId ();

	TranStatistics();
	virtual ~TranStatistics();

	void UpdateDelay(const Name&, uint32_t, const Time&);

	inline double GetRewardPerState(uint32_t id)
	{
		return m_RewardPerState[id];
	}

	void UpdateRewardPerState(uint32_t, const Time&);

	std::shared_ptr<std::vector<VideoIndex> > GetRemovableSegment(uint32_t id);

protected:
  virtual void DoDispose (void); ///< @brief Do cleanup
  virtual void NotifyNewAggregate ();

  void CalReward(uint32_t);											//Calculate the reward per state
  virtual double EvalVideoSeg(double, double, uint32_t);			//Do the actual evaluation for each video segment in CalReward()
  void UpdateHitRatio(uint32_t, const Time&);						//Update m_hitratio

private:
	Ptr<VideoStatistics> 		m_statptr;
//	Ptr<Node>					m_node;
	Ptr<ContentStore>			m_cs;
	Ptr<DPdfa>					m_dfa_env;

	HitHistory					m_hitratio;
	DelayHistory			 	m_delay;
	std::vector<double>			m_RewardPerState;
	std::vector<uint32_t>		m_visits;

	uint32_t				m_maxNumchunk;
	uint32_t				m_maxNumFile;
};



}
}

#endif
