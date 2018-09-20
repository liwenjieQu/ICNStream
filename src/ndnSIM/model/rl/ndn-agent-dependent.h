/*
 * Author: Wenjie Li
 * Date: 2016-11-28
 */

/*
 * This file contains the Definition on State and Action in Multi-Agent Reinforcement Learning Framework (Dependent)
 * Please refer to my paper for definition of 'Dependency Set'.
 *
 */
#ifndef NDN_RL_AGENT_DEPENDENT_H
#define NDN_RL_AGENT_DEPENDENT_H

#include "ndn-agent-basic.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/node.h"

#include "ns3/ndn-content-store.h"
#include "ns3/ndn-bitrate.h"

#include <string>
#include <vector>
#include <map>
#include <string>


namespace ns3 {
namespace ndn {

class DependentMARLAction;

class DependentMARLState: public SimpleRefCount<DependentMARLState> {
	using NeighborState = std::map<uint32_t, Ptr<const BasicMARLState> >;
public:

	static std::string
	GetName() { return "NormalState"; }

	DependentMARLState(Ptr<Node> nodeptr, uint32_t mode = 1);
	virtual ~DependentMARLState();

	//bool operator==(const DependentMARLState& idx) const;
	//DependentMARLState& operator=(const DependentMARLState&);

	inline uint32_t GetAgentID() const {
		return m_node->GetId();
	}

	inline Ptr<const BasicMARLState> GetLocalState() const {
		return ConstCast<BasicMARLState>(m_localstate);
	}
	inline const NeighborState& GetNeighborStates() const {
		return m_neighborstates;
	}

	inline Ptr<const BasicMARLState> GetRequestStatus() const{
		return ConstCast<BasicMARLState>(m_inrequest);
	}
public:
	/* SetNeighborState: Keep track of local states of dependent agents in H(i)
	 * Add Ptr<BasicMARLState> to dictionary
	 */
	void SetNeighborState(Ptr<const DependentMARLState>);

	/*
	 * Apply State Aggregation on neignborhood states
	 */
	virtual void SummarizeNeighborState();

	bool ApplyAction(Ptr<const DependentMARLAction> DepActionPtr);

	/*
	 * Update the status of m_inrequest
	 */
	void UpdateRequest();

	void Assign(Ptr<const DependentMARLState>);

private:
	DependentMARLState();
	DependentMARLState(const DependentMARLState&);

private:
	Ptr<BasicMARLState> m_inrequest;
	Ptr<BasicMARLState> m_localstate;

protected:
	/*A dictionary is used to keep the local states of agents in H(i).
	 *The key is NodeID; value is Ptr<const BasicMARLState>
	 */
	Ptr<Node> m_node;
	NeighborState m_neighborstates;

};

class DependentMARLAction: public SimpleRefCount<DependentMARLAction> {
	using NeighborAction = std::map<uint32_t, Ptr<const BasicMARLAction> >;
public:

	static std::string
	GetName() { return "NormalAction"; }

	DependentMARLAction(Ptr<Node> nodeptr, double bias = 0.8);
	virtual ~DependentMARLAction();

	//bool operator==(const DependentMARLAction& idx) const;
	//DependentMARLAction& operator=(const DependentMARLAction& idx);

	inline uint32_t GetAgentID() const {
		return m_node->GetId();
	}

	inline Ptr<const BasicMARLAction> GetLocalAction() const {
		return ConstCast<BasicMARLAction>(m_localaction);
	}

	inline const NeighborAction& GetNeighborActions() const {
		return m_neighboractions;
	}

	void RandomSetLocalAction();
	void GuidedSetLocalAction();
	void SetLocalAction(int8_t*);


	void SetNeighborAction(Ptr<const DependentMARLAction>);

	virtual void SummarizeNeighborAction(bool);

	void Assign(Ptr<const DependentMARLAction>);

private:
	DependentMARLAction();
	DependentMARLAction(const DependentMARLAction&);
	Ptr<BasicMARLAction> m_localaction;

protected:
	Ptr<Node> m_node;
	double	  m_guidedHeuristics;
	std::map<uint32_t, Ptr<const BasicMARLAction> > m_neighboractions;

};

}
}

#endif
