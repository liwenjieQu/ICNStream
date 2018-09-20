#include "ndn-popularity.h"
#include "ndn-videocontent.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <exception>

#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/name.h"
#include "ns3/node.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ndn-videocontent.h"
#include "ns3/ndn-bitrate.h"

NS_LOG_COMPONENT_DEFINE("ndn.PopularitySummary");

namespace ns3{
namespace ndn{

NS_OBJECT_ENSURE_REGISTERED(PopularitySummary);

TypeId
PopularitySummary::GetTypeId()
{
  static TypeId tid = TypeId ("ns3::ndn::PopularitySummary")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()
	.AddConstructor<PopularitySummary> ()

	.AddAttribute("NumberofChunks",
			"The total number of chunks per video file used in the simulation",
			StringValue("15"),
			MakeUintegerAccessor(&PopularitySummary::m_maxNumchunk),
			MakeUintegerChecker<uint32_t>())

	.AddAttribute("NumberOfContents",
			"The total number of video files used in the simulation",
			StringValue("25"),
			MakeUintegerAccessor(&PopularitySummary::m_maxNumFile),
			MakeUintegerChecker<uint32_t>())
    ;
  return tid;
}



PopularitySummary::PopularitySummary()
	:m_pro_nextchunk(0)
{

}

void PopularitySummary::DoDispose()
{
	Object::DoDispose();
}

PopularitySummary::~PopularitySummary()
{

}

void PopularitySummary::NotifyNewAggregate()
{
	if(m_node == 0)
	{
		m_node = GetObject<Node>();
	}
	if(m_statptr == 0)
	{
		m_statptr = GetObject<VideoStatistics>();
		if(m_statptr == 0)
			IsEdge = false;
		else
			IsEdge = true;
	}
	Object::NotifyNewAggregate();
}

void PopularitySummary::SummarizeToBitrate()
{
	try{
		double sum = 0;
		for (auto iter_bitrate = m_bitrate_stat.begin(); iter_bitrate != m_bitrate_stat.end(); iter_bitrate++)
		{
			sum += iter_bitrate->second;
		}
		for (auto iter_bitrate = m_bitrate_stat.begin(); iter_bitrate != m_bitrate_stat.end(); iter_bitrate++)
		{
			m_bitrate_dis[iter_bitrate->first] = iter_bitrate->second / sum;
		}
	}
	catch (std::overflow_error& e)
	{
		NS_LOG_WARN("[WARNING]: Overflow in PopularitySummary::SummarizeToBitrate:" << e.what());
	}

}

/* Be careful with overflow */
void PopularitySummary::SummarizeToFile()
{
	try
	{
		double sum = 0;
		for (auto iter = m_file_stat.begin(); iter != m_file_stat.end(); iter++)
		{
			sum += iter->second;
		}
		for (auto iter = m_file_stat.begin(); iter != m_file_stat.end(); iter++)
		{
			m_file_dis[iter->first] = iter->second / sum;
		}
	}
	catch (std::overflow_error& e)
	{
		NS_LOG_WARN("[WARNING]: Overflow in PopularitySummary::SummarizeToFile:" << e.what());
	}
}

void PopularitySummary::SummarizeToChunk()
{
	try{
		/* maximum liklihood to estimate the parameter in Geometirc Distribution */
		uint64_t weightsum = 0;
		uint64_t start = 1;
		while (m_chunk_stat.find(start + 1) != m_chunk_stat.end())
		{
			weightsum += (m_chunk_stat[start] - m_chunk_stat[start + 1]) * start;
			start++;
		}
		weightsum += m_chunk_stat[start] * start;
		m_pro_nextchunk = (weightsum - m_chunk_stat[1]) / static_cast<double>(weightsum);
		NS_LOG_DEBUG("[Popularity]: NodeID:" << m_node->GetId() << "E[P] value is " << m_pro_nextchunk);
	}
	catch (std::overflow_error& e)
	{
		NS_LOG_WARN("[WARNING]: Overflow in PopularitySummary::SummarizeToChunk:" << e.what());
	}
}
void PopularitySummary::Summarize()
{
	if(IsEdge)
	{
		NS_LOG_DEBUG("Summarize statistics on edge nodeID:" << m_node->GetId());
		auto iter = m_statptr->GetTable().begin();
		for(; iter != m_statptr->GetTable().end(); iter++)
		{
			/* File Popularity */
			if (iter->first.m_chunk == 1)
				m_file_stat[iter->first.m_file] += iter->second;
			m_chunk_stat[iter->first.m_chunk] += iter->second;
			m_bitrate_stat[iter->first.m_bitrate] += iter->second;
		}

		/* Summarize to Bitrate Distribution */
		SummarizeToBitrate();

		SummarizeToFile();

		SummarizeToChunk();

		IsMarked = true;
	}
}

bool
PopularitySummary::SenseVideoRequest(uint32_t s, double* ratio)
{
	ForMARLonly = true;
	auto iter = m_statptr->GetTable().begin();
	for(; iter != m_statptr->GetTable().end(); iter++)
		m_bitrate_stat[iter->first.m_bitrate] += iter->second;

	/* Summarize to Bitrate Distribution */
	SummarizeToBitrate();

	Ptr<NDNBitRate> BRinfo = m_node->GetObject<NDNBitRate>();
	for(uint32_t i = 0; i < s; i++)
	{
		std::string idx = "br" + BRinfo->GetBRFromRank(i + 1);
		auto briter = m_bitrate_dis.find(idx);
		if(briter != m_bitrate_dis.end())
			ratio[i] = briter->second;
		else
			ratio[i] = 0;
	}
	return true;
}

void PopularitySummary::AggregateToFile(const PopularitySummary& PS)
{
	try{
		for (auto iter = PS.m_file_stat.begin(); iter != PS.m_file_stat.end(); iter++)
		{
			m_file_stat[iter->first] += iter->second;
		}
		SummarizeToFile();
	}
	catch (std::overflow_error& e)
	{
		NS_LOG_WARN("[WARNING]: Overflow in PopularitySummary::AggregateToFile:" << e.what());
	}
}

void PopularitySummary::AggregateToBitrate(const PopularitySummary& PS)
{
	try{
		double sum_this = 0;
		for (auto iter = m_file_stat.begin(); iter != m_file_stat.end(); iter++)
		{
			sum_this += iter->second;
		}
		double sum_PS = 0;
		for (auto iter = PS.m_file_stat.begin(); iter != PS.m_file_stat.end(); iter++)
		{
			sum_PS += iter->second;
		}
		double temp = 0;
		for (auto iter = m_bitrate_dis.begin(); iter != m_bitrate_dis.end(); iter++)
		{
			if(PS.m_bitrate_dis.find(iter->first) == PS.m_bitrate_dis.end())
			{
				iter->second = (sum_this * iter->second) / (sum_this + sum_PS);
				temp += iter->second;
			}
		}
		for (auto iter = PS.m_bitrate_dis.begin(); iter != PS.m_bitrate_dis.end(); iter++)
		{
			m_bitrate_dis[iter->first] = (m_bitrate_dis[iter->first] * sum_this + iter->second * sum_PS ) / (sum_this + sum_PS);
			temp += m_bitrate_dis[iter->first];
		}
		for (auto iter = m_bitrate_dis.begin(); iter != m_bitrate_dis.end(); iter++)
		{
			iter->second /= temp;
		}
	}
	catch (std::overflow_error& e)
	{
		NS_LOG_WARN("[WARNING]: Overflow in PopularitySummary::AggregateToBitrate:" << e.what());
	}
}

void PopularitySummary::Aggregate(const PopularitySummary& PS)
{
	if(!IsEdge)
	{
		//Must follow this sequence
		AggregateToBitrate(PS);

		AggregateToFile(PS);

		if((PS.m_pro_nextchunk > m_pro_nextchunk) || (m_pro_nextchunk == 0))
			m_pro_nextchunk = PS.m_pro_nextchunk;
	}
}

void PopularitySummary::UpdateDelay(const Name& n, const Time& t)
{
	try{
		std::string bitrate = n.get(-3).toUri();
		m_delay[bitrate].second = (t.GetMilliSeconds() + m_delay[bitrate].first * m_delay[bitrate].second)
				/ (m_delay[bitrate].first +1);
		m_delay[bitrate].first += 1;
		NS_LOG_INFO("[UpdateDelay] NodeID:" << m_node->GetId() << " for Interest on:" << " FileID:" <<n.get(-2).toNumber() << " ChunkID:"
				<<n.get(-1).toNumber() << "Bitrate:" << n.get(-3).toUri() << "\n"
				<<"Delay:" << m_delay[bitrate].second<<"ms");
	}
	catch (std::overflow_error& e)
	{
		NS_LOG_WARN("[WARNING]: Overflow in PopularitySummary::UpdateDelay:" << e.what());
	}
}
bool Less(const ValuedVideoIndex& s1, const ValuedVideoIndex& s2)
{
	return s1.m_cacheutility < s2.m_cacheutility;
}
void PopularitySummary::CalculateUtility()
{
	for (auto fileiter = m_file_dis.begin(); fileiter != m_file_dis.end(); fileiter++)
	{
		for (auto bititer = m_bitrate_dis.begin(); bititer != m_bitrate_dis.end(); bititer++)
		{
			for (uint32_t k = 1; k <= m_maxNumchunk; k++)
			{
				NS_LOG_INFO("[Utility]: File " << fileiter->first <<"\t" << fileiter->second << "\n"
						"Chunk " << k << "\t" <<pow(m_pro_nextchunk, k - 1)<< "\n"
						"Bitrate " << bititer->first << "\t" << bititer->second << "\n"
						"Value " << m_delay[bititer->first].second / m_node->GetObject<NDNBitRate>()->GetChunkSize(RetrieveBitrate(bititer->first)));
				double delay;
				if(firstRun)
					delay = m_node->GetObject<NDNBitRate>()->GetChunkSize(RetrieveBitrate(bititer->first)) * 3.2;
					//delay = m_dict_ratetosize[RetrieveBitrate(bititer->first)] * 3.2;
				else
					delay = m_delay[bititer->first].second;
				//double v = 1e3 * fileiter->second * bititer->second * pow(m_pro_nextchunk, k - 1) *
					//	delay / m_dict_ratetosize[RetrieveBitrate(bititer->first)];
				double v = 1e3 * fileiter->second * bititer->second * pow(m_pro_nextchunk, k - 1) *
						delay / m_node->GetObject<NDNBitRate>()->GetChunkSize(RetrieveBitrate(bititer->first));
				m_utilities.push_back(ValuedVideoIndex {fileiter->first, k, bititer->first, v});
			}
		}
	}
	firstRun = false;
	std::sort(m_utilities.begin(), m_utilities.end(), Less);
	//Print();

}
void PopularitySummary::Print()
{
	std::cout<<"Statistics on Node:" << m_node->GetId() << std::endl;
	std::cout<<"File Distribution:" << std::endl;
	for (auto fileiter = m_file_dis.begin(); fileiter != m_file_dis.end(); fileiter++)
		std::cout << "FileID: " << fileiter->first << "\tProb:" << fileiter->second << std::endl;
	std::cout << std::endl << "Bitrate Distribution:" << std::endl;
	for (auto bititer = m_bitrate_dis.begin(); bititer != m_bitrate_dis.end(); bititer++)
		std::cout << "Bitrate: " << bititer->first << "\tProb:" << bititer->second << std::endl;
	std::cout << std::endl << "Rank:" << std::endl;
	std::vector<ValuedVideoIndex>::reverse_iterator iter = m_utilities.rbegin();
	for (int i = 0; i < 30; i++)
	{
		std::cout << "Rank" << i <<"  FileID:" <<iter->m_file << "\tChunkID:" <<iter->m_chunk<<"\tBitrate:" <<iter->m_bitrate
				<<"\tValue:" << iter->m_cacheutility << std::endl;
		iter++;
	}
}

std::vector<ValuedVideoIndex>& PopularitySummary::UtilityRank()
{
	return m_utilities;
}

std::string PopularitySummary::RetrieveBitrate(const std::string& s)
{
	return s.substr(2);
}
void PopularitySummary::Clear()
{
	m_bitrate_stat.clear();
	m_bitrate_dis.clear();

	if(!ForMARLonly)
	{
		m_file_stat.clear();
		m_chunk_stat.clear();
		m_file_dis.clear();
		m_delay.clear();

		m_pro_nextchunk = 0;
		m_utilities.clear();
		IsMarked = false;
	}
}

}
}
