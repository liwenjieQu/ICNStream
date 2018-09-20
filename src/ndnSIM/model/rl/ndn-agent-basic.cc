/*
 * Author: Wenjie Li
 * Date: 2016-12-02
 */

#include "ns3/log.h"
#include "ns3/packet.h"

#include <cmath>
#include <sstream>
#include <iomanip>
#include "ndn-agent-basic.h"

NS_LOG_COMPONENT_DEFINE ("ndn.rl.agent.basic");

namespace ns3 {
namespace ndn {

BasicMARLState::BasicMARLState(Ptr<Node> nodeptr, int Granularity)
{
	m_node = nodeptr;
	m_BRinfo = m_node->GetObject<NDNBitRate>();
	m_cs = m_node->GetObject<ContentStore>();
	m_BRnum = m_BRinfo->GetTableSize();

	m_granularity = Granularity;
	m_partition = new double [m_BRnum];

	m_partition_int = new int [m_BRnum];

	for(uint32_t i = 0; i < m_BRnum; i++)
	{
		m_partition[i] = 0;
		m_partition_int[i] = 0;
	}
	m_init = true;
}
BasicMARLState::~BasicMARLState()
{
	delete [] m_partition;
	m_partition = nullptr;

	delete [] m_partition_int;
	m_partition_int = nullptr;
}

BasicMARLState::BasicMARLState()
{

}

bool
BasicMARLState::AdjustCachePartition(Ptr<const BasicMARLAction> agentaction)
{
	m_init = false;
	const int8_t* action = agentaction->GetChoices();
	//Legitimation check
	int8_t sum = 0, abssum = 0;
	for(uint32_t i = 0; i < m_BRnum; i++)
	{
		sum += action[i];
		abssum += std::abs(action[i]);
	}
	if(sum != 0 || abssum > 2)
		return false;


	if(abssum == 2)
	{
		uint32_t upBR = 0, downBR = 0;
		for(uint32_t i = 0; i < m_BRnum; i++)
		{
			if(action[i] == 1)
				upBR = i;
			if(action[i] == -1)
				downBR = i;
		}
		if(m_partition_int[upBR] + m_granularity > 100 || m_partition_int[downBR] - m_granularity < 0)
			return false;

		m_partition[upBR] += static_cast<double>(m_granularity) /100;
		m_partition[downBR] -= static_cast<double>(m_granularity) /100;

		m_partition_int[upBR] += m_granularity;
		m_partition_int[downBR] -= m_granularity;

		m_cs->AdjustSectionRatio(m_BRinfo->GetBRFromRank(upBR + 1),
								 m_BRinfo->GetBRFromRank(downBR + 1),
								 static_cast<double>(m_granularity) /100);
	}

	return true;
}

void
BasicMARLState::AlignPartitionArr(double* arr)
{
	m_init = false;
	int* alignedarr =  new int [m_BRnum];
	double* Diff = new double [m_BRnum];

	for(uint32_t i = 0; i < m_BRnum; i++)
	{
		double v = arr[i] / (static_cast<double>(m_granularity) /100);
		uint32_t intinreal = static_cast<uint32_t>(v);
		Diff[i] = v - intinreal - 0.5;
		if(v - intinreal >= 0.5)
			intinreal++;
		alignedarr[i] = intinreal * m_granularity;
	}
	// Test the entire array to make sure the sum of elements is 1
	uint32_t sumper = 0;
	for(uint32_t i = 0; i < m_BRnum; i++)
		sumper += alignedarr[i];

	if(sumper > 100)//The alignment needs further adjustment
	{
		NS_LOG_DEBUG("[DependentMARLState::AlignPartitionArr > 1]: The partition array needs adjustment!");
		while(sumper > 100)
		{
			uint32_t idx = 0;
			for(uint32_t j = 1; j < m_BRnum; j++)
			{
				if(Diff[j] >= 0 && Diff[j] < Diff[idx])
					idx = j;
			}
			alignedarr[idx] -= m_granularity;
			Diff[idx] = 0.5;
			sumper -= m_granularity;
		}
	}
	else if (sumper < 100)
	{
		NS_LOG_DEBUG("[DependentMARLState::AlignPartitionArr < 1]: The partition array needs adjustment!");
		while(sumper < 100)
		{
			uint32_t idx = 0;
			for(uint32_t j = 1; j < m_BRnum; j++)
			{
				if(Diff[j] <= 0 && Diff[j] > Diff[idx])
					idx = j;
			}
			alignedarr[idx] += m_granularity;
			Diff[idx] = -0.5;
			sumper += m_granularity;
		}
	}
	// The array is aligned now. Assign to m_parition in BasicMARLState.
	for(uint32_t i = 0; i< m_BRnum; i++)
	{
		m_partition_int[i] = alignedarr[i];
		m_partition[i] = alignedarr[i] / static_cast<double>(100);
		arr[i] = m_partition[i];
	}

	delete [] alignedarr;
	delete [] Diff;

	alignedarr = nullptr;
	Diff = nullptr;
}


bool
BasicMARLState::operator==(const BasicMARLState& idx) const
{
	bool temp = m_BRnum == idx.m_BRnum
			  && m_node->GetId() == idx.m_node->GetId();
	if(!temp)
		return false;
	for(uint32_t i = 0; i < m_BRnum; i++)
		temp = temp && (m_partition_int[i] == (idx.m_partition_int)[i]);
	return temp;
}

BasicMARLState&
BasicMARLState::operator =(const BasicMARLState& idx)
{
	this->m_node = idx.m_node;
	this->m_BRinfo = idx.m_BRinfo;
	this->m_cs = idx.m_cs;

	this->m_init = idx.m_init;
	this->m_BRnum = idx.m_BRnum;
	this->m_granularity = idx.m_granularity;

	for(uint32_t i = 0; i < m_BRnum; i++)
	{
		(this->m_partition)[i] = (idx.m_partition)[i];
		(this->m_partition_int)[i] = (idx.m_partition_int)[i];
	}
	return *this;
}

std::string
BasicMARLState::PrintState() const
{
	std::ostringstream ss;
	for(uint32_t i = 0; i < m_BRnum; i++)
	{
		ss <<  m_BRinfo->GetBRFromRank(i + 1) << ": ";
		ss << std::fixed << std::setprecision(2) << m_partition[i] << "\t";
	}
	return ss.str();
}

/////////////////////////////////////////////////////
////////////////BasicMARLAction//////////////////////
/////////////////////////////////////////////////////

BasicMARLAction::BasicMARLAction(Ptr<Node> nodeptr)
{
	m_node = nodeptr;
	Ptr<NDNBitRate> m_BRinfo = m_node->GetObject<NDNBitRate>();
	m_BRnum = m_BRinfo->GetTableSize();
	m_choices = new int8_t [m_BRnum];

	for(uint32_t i = 0; i < m_BRnum; i++)
		m_choices[i] = 0;
}

BasicMARLAction::BasicMARLAction(Ptr<Node> nodeptr, uint32_t upidx, uint32_t downidx)
{
	m_node = nodeptr;
	Ptr<NDNBitRate> m_BRinfo = m_node->GetObject<NDNBitRate>();
	m_BRnum = m_BRinfo->GetTableSize();
	m_choices = new int8_t [m_BRnum];

	for(uint32_t i = 0; i < m_BRnum; i++)
		m_choices[i] = 0;

	if(upidx != downidx)
	{
		m_choices[upidx] = 1;
		m_choices[downidx] = -1;
	}
}

BasicMARLAction::BasicMARLAction()
{

}

BasicMARLAction::~BasicMARLAction()
{
	delete [] m_choices;
	m_choices = nullptr;
}

bool
BasicMARLAction::operator==(const BasicMARLAction& idx) const
{
	bool temp = m_BRnum == idx.m_BRnum
			&& m_node->GetId() == idx.m_node->GetId();
	for(uint32_t i = 0; i < m_BRnum; i++)
		temp = temp && (m_choices[i] == (idx.m_choices)[i]);
	return temp;
}

BasicMARLAction&
BasicMARLAction::operator=(const BasicMARLAction& idx)
{
	this->m_BRnum = idx.m_BRnum;
	this->m_node = idx.m_node;

	for(uint32_t i = 0; i< m_BRnum; i++)
		(this->m_choices)[i] = (idx.m_choices)[i];
	return *this;
}

std::string
BasicMARLAction::PrintAction() const
{
	std::string u = "";
	std::string d = "";
	for(uint32_t i = 0; i < m_BRnum; i++)
	{
		if(m_choices[i] == 1)
			u = m_node->GetObject<NDNBitRate>()->GetBRFromRank(i + 1);
	}
	for(uint32_t i = 0; i < m_BRnum; i++)
	{
		if(m_choices[i] == -1)
			d = m_node->GetObject<NDNBitRate>()->GetBRFromRank(i + 1);
	}
	return "Up: " + u + "; Down: " + d;
}

}
}
