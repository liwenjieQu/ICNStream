
#include "ndn-videostat.h"
#include "ns3/ndn-forwarding-strategy.h"
#include "ns3/node.h"
#include "ns3/log.h"

#include <iostream>


NS_LOG_COMPONENT_DEFINE("ndn.VideoStatistics");

namespace ns3{
namespace ndn{

NS_OBJECT_ENSURE_REGISTERED (VideoStatistics);
VideoStatistics::VideoStatistics()
{

}

VideoStatistics::~VideoStatistics()
{

}
TypeId
VideoStatistics::GetTypeId()
{
  static TypeId tid = TypeId ("ns3::ndn::VideoStatistics")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()
	.AddConstructor<VideoStatistics> ()
    ;
  return tid;
}
void VideoStatistics::DoDispose()
{
	m_stat.clear();
	Object::DoDispose ();
}

void VideoStatistics::NotifyNewAggregate ()
{
	if(m_node == 0)
		m_node = GetObject<Node>();
	Object::NotifyNewAggregate ();
}

void VideoStatistics::Add(VideoIndex&& index)
{
	NS_LOG_DEBUG("[VideoStatistics]:"<< " receive Interests on NodeID:" << m_node->GetId() <<" FileID:" <<
			index.m_file << " ChunkID:" << index.m_chunk << " Bitrate:" << index.m_bitrate);
	m_stat[index] += 1;
}

}
}
