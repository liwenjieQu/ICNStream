#include <algorithm>

#include "ns3/ndn-content-store.h"
#include "ndn-popularity.h"
#include "ndn-videocache.h"
#include "ns3/ndn-bitrate.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/string.h"
#include "ns3/name.h"



NS_LOG_COMPONENT_DEFINE("ndn.VideoCacheDecision");

namespace ns3{
namespace ndn{

NS_OBJECT_ENSURE_REGISTERED (VideoCacheDecision);

TypeId
VideoCacheDecision::GetTypeId()
{
  static TypeId tid = TypeId ("ns3::ndn::VideoCacheDecision")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()
	.AddConstructor<VideoCacheDecision> ()

    .AddAttribute ("Prefix","Name of the Interest",
                   StringValue ("/"),
                   MakeNameAccessor (&VideoCacheDecision::m_prefixName),
                   MakeNameChecker ())
    ;
  return tid;
}

VideoCacheDecision::VideoCacheDecision()
{

}
VideoCacheDecision::~VideoCacheDecision()
{

}

void VideoCacheDecision::DoDispose()
{
	m_stat_summary = 0;
	m_cs = 0;
	Object::DoDispose();
}

void VideoCacheDecision::NotifyNewAggregate()
{
	if(m_stat_summary == 0)
		m_stat_summary = GetObject<PopularitySummary>();
	if(m_cs == 0)
		m_cs = GetObject<ContentStore>();
	if(m_node == 0)
		m_node = GetObject<Node>();
	Object::NotifyNewAggregate();
}

void VideoCacheDecision::AggregateDecision(const VideoCacheDecision& other)
{
	for(auto iter = other.m_table.begin(); iter != other.m_table.end(); iter++)
	{
		if(m_table.find(iter->first) != m_table.end())
			m_table[iter->first] = std::max(iter->second, m_table[iter->first]);
		else
			m_table[iter->first] = iter->second;
	}
}
std::string VideoCacheDecision::RetrieveBitrate(const std::string& s)
{
	return s.substr(2);
}
void VideoCacheDecision::GreedyChoice()
{
	uint32_t sizelimit = m_cs->GetCapacity();
	std::vector<ValuedVideoIndex>::reverse_iterator riter = m_stat_summary->UtilityRank().rbegin();
	for(; riter != m_stat_summary->UtilityRank().rend(); riter++)
	{
		DecisionIndex d {riter->m_bitrate, riter->m_file};
		if(m_table.find(d) != m_table.end())
		{
			if(riter->m_chunk <= m_table[d])
				continue;
		}
		if(sizelimit < static_cast<uint32_t>(m_node->GetObject<NDNBitRate>()->GetChunkSize(RetrieveBitrate(riter->m_bitrate)) * 1e3))
			break;
		else
		{
			sizelimit -= static_cast<uint32_t>(m_node->GetObject<NDNBitRate>()->GetChunkSize(RetrieveBitrate(riter->m_bitrate)) * 1e3);
			//Update Decision Table
			NS_LOG_DEBUG("GreedyChoice on NodeID: "<<m_node->GetId()<<" [Bitrate:"<<d.m_bitrate<<" File:" << d.m_file
					<<" Chunk:"<< riter->m_chunk << "]");
			if(m_table[d] + 1 != riter->m_chunk)
				NS_LOG_WARN("!!!!!!!!!!!!!!!!!!!CacheTableUpdate Error!!!!!!!!!!!!!!!!!");
			m_table[d] = riter->m_chunk;
			//Update Decision Input to CS
			AddToDecision(riter->m_bitrate, riter->m_file, riter->m_chunk);
		}
	}
	m_cs->InstallCacheEntity();
}
void VideoCacheDecision::AddToDecision(const std::string& bitrate, uint32_t file, uint32_t chunk)
{
	Ptr<Name> CacheName = Create<Name> (m_prefixName);
	CacheName->append(bitrate);
	CacheName->appendNumber(file);
	CacheName->appendNumber(chunk);
	m_cs->inCacheConfig.push_back(*CacheName);
}
void VideoCacheDecision::Clear()
{
	m_table.clear();
	m_cs->ClearCachedContent();
}

}
}
