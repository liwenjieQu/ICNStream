/* Author: Wenjie Li
 * Date: 2016-11-28
 */

/*
 * This file contains the Definition on State and Action in Multi-Agent Reinforcement Learning Framework (Basic)
 *
 */
#ifndef NDN_RL_AGENT_BASIC_H
#define NDN_RL_AGENT_BASIC_H

#include <string>
#include <vector>

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/node.h"

#include "ns3/ndn-content-store.h"
#include "ns3/ndn-bitrate.h"

namespace ns3 {
namespace ndn {

class BasicMARLAction;

class BasicMARLState: public SimpleRefCount<BasicMARLState> {
public:

	BasicMARLState(Ptr<Node> nodeptr, int Granularity = 5/*%*/);
	~BasicMARLState();

	bool operator==(const BasicMARLState& idx) const;
	BasicMARLState& operator=(const BasicMARLState& idx);

	inline uint32_t GetNumBR() const {
		return m_BRnum;
	}

	inline const int* GetIntPartition() const{
		return m_partition_int;
	}

	//Prerequisite: The entity calls this function must have the same m_BRnum
	inline void SetPartition(double* newpartition) {
		for (uint32_t i = 0; i < m_BRnum; i++)
		{
			m_partition[i] = newpartition[i];
			m_partition_int[i] = static_cast<int>((newpartition[i] / (static_cast<double>(m_granularity) / 100)) + 0.5) * m_granularity;
		}
		m_init = false;
	}

	/*
	 * Check whether BasicMARLState is in Initial Status
	 */
	inline bool CheckInit() const{
		return m_init;
	}

	inline void ResetToInit() {
		m_init = true;
	}
	/*
	 * Adjust the cache partition for bitrate
	 */
	bool AdjustCachePartition(Ptr<const BasicMARLAction>);
	/*
	 * arr can be request input, where the percentage for bit-rates is not consistent with the granularity.
	 * AlignPartitionArr() aligns the boundary of elements.
	 */
	void AlignPartitionArr(double* arr);

	inline double GetSizeFromPartition(uint32_t idx) const {
		if (idx < m_BRnum)
			return m_partition[idx] * m_cs->GetCapacity();
		else
			return 0;
	}
	std::string PrintState() const;

private:
	/*
	 * The Default constructor is not allowed for Basic MARL state
	 * since we need the Ptr<NDNBitRate> to initialize.
	 */
	BasicMARLState();

private:
	bool 		m_init;		// BasicMARLState is in its initial status
	//Number of Considered BitRates;
	uint32_t 	m_BRnum;

	int 		m_granularity;
	double* 	m_partition;

	/*Integer version of (double) m_partition*/
	int*		m_partition_int;

	Ptr<NDNBitRate> 	m_BRinfo;
	Ptr<ContentStore> 	m_cs;
	Ptr<Node>			m_node;
};


///////////////////////////////////
//////BasicMARLAction//////////////
///////////////////////////////////
class BasicMARLAction: public SimpleRefCount<BasicMARLAction> {
public:
	BasicMARLAction(Ptr<Node>);
	BasicMARLAction(Ptr<Node>, uint32_t, uint32_t);
	~BasicMARLAction();

	bool operator==(const BasicMARLAction& idx) const;
	BasicMARLAction& operator=(const BasicMARLAction& idx);

	inline const int8_t* GetChoices() const {
		return m_choices;
	}
	inline uint32_t GetNumBR() const {
		return m_BRnum;
	}
	inline void SetChoices(int8_t* c) {
		for (uint32_t i = 0; i < m_BRnum; i++) {
			m_choices[i] = c[i];
		}
	}

	std::string
	PrintAction() const;

private:
	BasicMARLAction();

private:
	uint32_t 	m_BRnum;	//Number of Considered BitRates;
	int8_t* 	m_choices;

	Ptr<Node> 	m_node;

};

}
}

#endif
