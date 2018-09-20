/*
 * Author: Wenjie Li
 * Date: 2016-03-28
 */


#ifndef NDN_DFA_ENV_H
#define NDN_DFA_ENV_H

#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/random-variable.h"
#include "ns3/ndn-cachestate.h"

#include "boost/functional/hash.hpp"

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <ostream>
#include <vector>

namespace ns3{
namespace ndn{

enum class CacheOperator{
	AttachFront,
	AttachEnd,
	NoReplace
};
class QFactor{
public:
	using Action = std::pair<CacheOperator, std::string>;
	QFactor()
		:m_stateid(0)
	{
		m_action.first = CacheOperator::NoReplace;
		m_action.second = " ";
	}
	QFactor(uint32_t id, CacheOperator op, std::string br)
		:m_stateid(id)
	{
		m_action.first = op;
		m_action.second = br;
	}
	QFactor(uint32_t id, const Action& ac)
		:m_stateid(id),
		 m_action(ac)
	{}
	inline bool operator==(const QFactor& qf) const{
		return m_stateid == qf.m_stateid && m_action.first == qf.m_action.first &&
				m_action.second == qf.m_action.second;
	}
public:
	uint32_t 	m_stateid;
	Action		m_action;
};
}
}

namespace std{
	template <>
	struct hash<ns3::ndn::QFactor>
	{
        std::size_t operator()(const ns3::ndn::QFactor& qf) const{
			std::size_t seed = 100;
			boost::hash_combine(seed, boost::hash_value(qf.m_stateid));
			boost::hash_combine(seed, boost::hash_value(qf.m_action.second));
			boost::hash_combine(seed, boost::hash_value(qf.m_action.first));
			return seed;
       }
	};
}


namespace ns3{
namespace ndn{

class ContentStore;
class NDNBitRate;
class MDPState;
class TranStatistics;

class DPdfa : public Object
{
public:
	using Action = std::pair<CacheOperator, std::string>;
	using OptimalPolicy = std::vector<Action>;
	using RewardValue = std::unordered_map<QFactor, double>;

	static TypeId GetTypeId ();

	DPdfa();
	virtual ~DPdfa();

	void StartDFA();
	bool CheckRemovable(const VideoIndex&);

protected:
	void Transition();
  // inherited from Object class
  virtual void NotifyNewAggregate (); ///< @brief Even when object is aggregated to another Object
  virtual void DoDispose (); ///< @brief Do cleanup

  /*
   * Randomly select a video file on which the cache operation will apply
   */
  virtual uint32_t	ZipfFileSelect();
  /*
   * Triggered after a certain period of time;
   * Generate a sequence of simulated states (trajectory)
   */
  virtual Action SimulationSeq(uint32_t);

  /*
   * Update Q-Value and Update the optimal policy
   */
  void	UpdateTotalReward(uint32_t currentid, uint32_t nextid, const Action& action);
  void	UpdateOptimalPolicy(const QFactor& qf);

  void	Move();

private:
  uint32_t 				TotalChunk;
  uint32_t				DFAFile;
  double				Skewness;
  double				m_discount;

  Ptr<ContentStore> 	m_cs;
  Ptr<NDNBitRate>		m_bitinfo;
  Ptr<TranStatistics>	m_history;

  uint32_t 				m_currentid;							//ID of the state that DFA is visiting
  uint32_t				m_sizestates;

  RewardValue								m_reward;		//dict[ID of MDP state] --> reward value
  OptimalPolicy								m_policy;		//dict[ID of MDP state] --> action in policy space
  std::unordered_set<MDPState>				m_states;
  std::unordered_set<MDPState>::iterator	m_current_iter;
  std::vector<uint32_t>						m_visits;		//The number of times each state is visited

  Time					m_interval;
  std::string			m_prefix;
  //double				m_requestrate;
  EventId				m_event;

private:
  double	TranMapping(uint32_t currentid, uint32_t nextid);
  Ptr<Name> VideoIndexToName(const VideoIndex& vi);


  /*
   * Calculate and prepare the transition probability for each video file
   */
  void 		PrepareTransProb ();

  std::vector<double> 	m_Pcum;  //cumulative probability
  UniformVariable 		m_SeqRng;
};


}
}
#endif /* ndn-dfa-env.h */

