/*
 * author: Wenjie Li
 */

#include "ndn-video-consumer.h"
#include "ns3/ndn-app-face.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/ndnSIM/utils/ndn-fw-hop-count-tag.h"
#include <sstream>

NS_LOG_COMPONENT_DEFINE("ndn.VideoConsumer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(VideoConsumer);

/*
 * author: Wenjie Li
 * According class VideoConsumer, ConsumerZipfMandelbrot, ConsumerCbr, Consumer,
 * the following attributes need to be set before initialize the application instance
 *
 * Each client needs an independent helper
 * consumerHelper.SetAttribute("Prefix", "");
 *
 * consumerHelper.SetAttribute("Frequency", ""); /lambda for poisson process //!!!!attention to rate=0
 *
 * consumerHelper.SetAttribute("NumberOfContents", "");
 * consumerHelper.SetAttribute("SkewnessFactor","");
 *
 * consumerHelper.SetAttribute("BitRate",);
 * consumerHelper.SetAttribute("NumberofChunks","");
 * consumerHelper.SetAttribute("ProbCache", "")
 *
 */
TypeId VideoConsumer::GetTypeId(void) {
	static TypeId tid =
			TypeId("ns3::ndn::VideoConsumer").SetGroupName("Ndn").SetParent<
					ConsumerZipfMandelbrot>().AddConstructor<VideoConsumer>()

			.AddAttribute("BitRate", "The bit rate this client chooses",
					StringValue("250kbps"),
					MakeStringAccessor(&VideoConsumer::m_bitrate),
					MakeStringChecker())

			.AddAttribute("ProbCache",
					"The probability of watching the next chunk",
					StringValue("0.7"),
					MakeDoubleAccessor(&VideoConsumer::m_cacheProfit),
					MakeDoubleChecker<double>());

	return tid;
}

VideoConsumer::VideoConsumer() {
}

VideoConsumer::~VideoConsumer() {
}

void VideoConsumer::SendPacket(uint32_t fileseq, uint32_t chunkseq) {
	if (!m_active)
		return;NS_LOG_FUNCTION_NOARGS ();

	uint32_t nextseq = std::numeric_limits<uint32_t>::max(); //invalid
	bool no_retransmission = false;

	while (m_retxSeqs.size()) {
		nextseq = *m_retxSeqs.begin();
		m_retxSeqs.erase(m_retxSeqs.begin());

		break;
	}

	if (nextseq == std::numeric_limits<uint32_t>::max()) {//no retransmission
		no_retransmission = true;
		if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
			if (m_seq >= m_seqMax) {
				return; // we are totally done
			}
		}
		m_seq++;
		nextseq = (fileseq - 1) * m_maxNumchunk + chunkseq;
	}

	Ptr<Name> nameWithSequence = Create<Name>(m_interestName);
	std::string s_bitrate = "br" + m_bitrate;
	nameWithSequence->append(s_bitrate);
	//nameWithSequence->appendSeqNum(nextseq);

	uint32_t calchunkseq = nextseq % m_maxNumchunk;
	if(calchunkseq == 0)
		calchunkseq = m_maxNumchunk;
	uint32_t calfileseq = ((nextseq - chunkseq) / m_maxNumchunk) + 1;
	nameWithSequence->appendNumber(calfileseq);
	nameWithSequence->appendNumber(calchunkseq);

	Ptr<Interest> interest = Create<Interest>();
	interest->SetTSI(0);
	interest->DecTSI();
	interest->InitAcc();
	interest->SetNonce(m_rand.GetValue());
	interest->SetName(nameWithSequence);
	interest->SetInterestLifetime(m_interestLifeTime);

	NS_LOG_DEBUG("[VideoConsumer] Interest with name:" << nameWithSequence->toUri() <<
			"\ton NodeID:"<< this->m_node->GetId() <<"\tat Timestamp: " << Simulator::Now ());

	WillSendOutInterest (nextseq);

	FwHopCountTag hopCountTag;
	interest->GetPayload()->AddPacketTag(hopCountTag);
	m_transmittedInterests(interest, this, m_face);
	m_face->ReceiveInterest(interest);

	bool request_next = true;
	if(no_retransmission)
	{
		if (chunkseq == m_maxNumchunk)
			request_next = false;
		UniformVariable t_keepWatching(0.0, 1.0);
		if(t_keepWatching.GetValue() > m_cacheProfit)
			request_next = false;
	}
	if(request_next && no_retransmission)
	{
		NS_LOG_DEBUG("[VideoConsumer] on NodeID" << this->m_node->GetId() << " Schedule for next chunk");
		VideoConsumer::ScheduleNextVideoChunk(fileseq, chunkseq + 1);
	}
	else if(!no_retransmission)
	{
		NS_LOG_DEBUG("[VideoConsumer] on NodeID" << this->m_node->GetId() << " Schedule for retransimission");
		VideoConsumer::ScheduleNextVideoChunk(fileseq, chunkseq);
	}
	else if(no_retransmission)
		NS_LOG_DEBUG("[VideoConsumer] on NodeID" << this->m_node->GetId() << " No Schedule for next chunk");
}

void VideoConsumer::SendVideofile() {
	if (!m_active)
		return;
	NS_LOG_FUNCTION_NOARGS ();

	uint32_t fileseq = GetNextSeq();

	Simulator::Schedule(Seconds(0.0), &VideoConsumer::SendPacket, this,
			fileseq, 1);
	ScheduleNextVideoFile();
}

void VideoConsumer::ScheduleNextVideoFile() {

	if (m_firstTime) {
		UniformVariable initiate(0.0, 10.0);
		Simulator::Schedule(Seconds(initiate.GetValue()), &VideoConsumer::SendVideofile, this);
		m_firstTime = false;
	} else
		Simulator::Schedule(
				(m_random == 0) ?
						Seconds(1.0 / m_frequency) :
						Seconds(m_random->GetValue()),
				&VideoConsumer::SendVideofile, this);
}

void VideoConsumer::ScheduleNextVideoChunk(uint32_t fileseq, uint32_t chunkseq) {

	Simulator::Schedule(Seconds(2), &VideoConsumer::SendPacket, this, fileseq, chunkseq);
}

void
VideoConsumer::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION_NOARGS ();

  // do base stuff
  NS_LOG_DEBUG("[Video Consumer] on NodeID:" << this->m_node->GetId() << " Starts. For bitrate:" << m_bitrate);
  App::StartApplication ();

  ScheduleNextVideoFile ();
}

void
VideoConsumer::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS ();
  // cleanup base stuff
  App::StopApplication ();
}

void VideoConsumer::ScheduleNextPacket()
{

}

void
VideoConsumer::OnTimeout (uint32_t sequenceNumber)
{
	//Consumer::OnTimeout(sequenceNumber);
	NS_LOG_INFO("[Video Consumer] Time out! on NodeID:" << GetNode()->GetId() << " Retransmission on Seq:" << sequenceNumber);
	//Simulator::Schedule(Seconds(0.0), &VideoConsumer::SendPacket, this, 0, 0);
}

} /* namespace ndn */
} /* namespace ns3 */
