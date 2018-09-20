/*
 * Author: Wenjie Li
 * Date: 2016-12-07
 * MARLnfa is a base class. Detailed implementation plz refers to ndn-marl-impl.h
 */

#include <ns3/ndn-marl-qvar.h>
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/node.h"

namespace ns3{
namespace ndn{

class MARLnfa : public Object
{
public:

	static
	TypeId GetTypeId ();

	MARLnfa();

	void SetLogger(std::ofstream* f);

	virtual ~MARLnfa();

	inline void ResetCSReward()
	{
		if(m_cs != 0)
			m_cs->ResetReward();
	}

	virtual std::size_t GetQStatus() const;

	virtual inline void AddToHset(Ptr<Node> nodeptr)
	{
		m_Hset.insert(nodeptr->GetId());
	}
	virtual void ResetToInit();

	virtual void UpdateCurrentState();
	virtual bool UpdateCurrentAction(bool);
	virtual bool ManualSetCurrentAction(int, int);
	virtual bool Transition();


	virtual inline Ptr<const DependentMARLState> GetCurrentState() const
	{
		return 0;
	}

	virtual inline Ptr<const DependentMARLAction> GetCurrentAction() const
	{
		return 0;
	}

	virtual inline Ptr<const DependentMARLState> GetNextState() const
	{
		return 0;
	}
	virtual inline Ptr<const DependentMARLAction> GetOptAction() const
	{
		return 0;
	}

	inline uint32_t	GetNumOfVisit() const
	{
		return m_visits;
	}

	virtual inline void OutputQStatus(std::fstream* qRecorder)
	{

	}

	virtual inline void InputQStatus(std::fstream* qRecorder)
	{

	}

protected:
	bool				m_newstart;
	bool				m_experimentalAggregation;		// Apply aggressive aggregation
	Ptr<Node> 			m_node;
	Ptr<ContentStore> 	m_cs;
	std::set<uint32_t> 	m_Hset;							// Dependency set

	double* 			m_csinitpartition;				// The initial cache partition (Starting state)
	uint32_t			m_visits;						// Number of re-visits on a certain state
	uint32_t			m_granPlan;						// Plan # of state granularity
	uint32_t			m_useGuidedHeuristics;			// = 0 or 1, indicator of applying guided heuristics for exploration

	std::ofstream*		m_log;

protected:
	virtual void NotifyNewAggregate (); ///< @brief Even when object is aggregated to another Object
	virtual void DoDispose (); ///< @brief Do cleanup
};


}
}
