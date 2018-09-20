#include "ndn-bitrate.h"
#include "ns3/log.h"
#include "ns3/double.h"

#include <algorithm>

NS_LOG_COMPONENT_DEFINE("ndn.VideoBitRate");

namespace ns3{
namespace ndn{

NS_OBJECT_ENSURE_REGISTERED(NDNBitRate);


TypeId
NDNBitRate::GetTypeId()
{
	  static TypeId tid = TypeId ("ns3::ndn::VideoBitRate")
	    .SetGroupName ("Ndn")
	    .SetParent<Object> ()
		.AddConstructor<NDNBitRate> ()
		.AddAttribute("Duration",
				"The playback time contained in each video segment (in Seconds)",
				DoubleValue(4),
				MakeDoubleAccessor(&NDNBitRate::m_duration),
				MakeDoubleChecker<double>())
	    ;
	  return tid;

}
NDNBitRate::NDNBitRate()
	:m_totalsize(0)
{

}

NDNBitRate::~NDNBitRate()
{

}

void
NDNBitRate::NotifyNewAggregate()
{
	if(m_node == 0)
	{
		m_node = GetObject<Node>();
	}
	Object::NotifyNewAggregate();
}

void
NDNBitRate::DoDispose()
{
	m_node = 0;
	Object::DoDispose ();
}

void
NDNBitRate::AddBitRate(const std::string& br)
{
	double size = CalChunkSize(br);
	m_size[br] = size;
	m_totalsize += size;
	m_bitrates.push_back(br);
}

double
NDNBitRate::GetChunkSize(const std::string& br)
{
	auto iter = m_size.find(br);
	if(iter != m_size.end())
		return iter->second;
	else
	{
		std::cout<<"Something wrong in NDNBitrate match!" << std::endl;
		return 0;
	}
}

uint32_t
NDNBitRate::GetTableSize()
{
	return m_size.size();
}

double
NDNBitRate::CalChunkSize(const std::string& br)
{
	std::string tail = "kbps";
	std::size_t found = br.find(tail);
	if(found == std::string::npos)
		return 0;
	double s = std::stod(br.substr(0, found)) * m_duration / 8;
	return s;
}

uint32_t
NDNBitRate::GetRankFromBR(std::string br)
{
	/* Must guarantee the insert sequence when adding bitrates */
	//std::sort(m_bitrates.begin(), m_bitrates.end());
	uint32_t rank = 0;
	for(uint32_t i = 0; i < m_bitrates.size(); i++)
	{
		std::size_t found = m_bitrates[i].find(br);
		if(found != std::string::npos)
		{
			rank = i + 1;
			break;
		}
	}
	return rank;
}

const std::string&
NDNBitRate::GetBRFromRank(uint32_t rank)
{
	if(rank < 1)
		return m_bitrates.at(0);
	else if (rank > m_bitrates.size())
		return m_bitrates.at(m_bitrates.size() - 1);
	else
		return m_bitrates.at(rank -1);
}

std::string
NDNBitRate::GetNextHighBitrate(uint32_t rank)
{

	std::string nextbr;
	if(rank < m_bitrates.size())
		nextbr = m_bitrates[rank];
	else
		nextbr = m_bitrates[rank - 1];
	return nextbr;

}

std::string
NDNBitRate::GetNextLowBitrate(uint32_t rank)
{
	std::string nextbr;
	if(rank > 1)
		nextbr = m_bitrates[rank - 2];
	else
		nextbr = m_bitrates[0];

	return nextbr;
}

double
NDNBitRate::GetRewardFromRank(uint32_t rank)
{
	std::string br = GetBRFromRank(rank);
	double s = m_size[br];
	return s * 10 / m_totalsize;
}


}
}
