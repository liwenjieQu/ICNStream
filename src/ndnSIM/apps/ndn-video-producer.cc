/*
 * author: Wenjie Li
 * DASH-Offline
 */

#include "ndn-video-producer.h"
#include "ns3/log.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/ndn-bitrate.h"

#include "ns3/ndn-app-face.h"
#include "ns3/ndn-fib.h"

#include "ns3/ndnSIM/utils/ndn-fw-hop-count-tag.h"
#include <boost/ref.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
namespace ll = boost::lambda;

NS_LOG_COMPONENT_DEFINE ("ndn.VideoProducer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (VideoProducer);

TypeId
VideoProducer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::VideoProducer")
    .SetGroupName ("Ndn")
    .SetParent<App> ()
    .AddConstructor<VideoProducer> ()
    .AddAttribute ("Prefix","Prefix, for which producer has the data",
                   StringValue ("/"),
                   MakeNameAccessor (&VideoProducer::m_prefix),
                   MakeNameChecker ())
    .AddAttribute ("Postfix", "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
                   StringValue ("/"),
                   MakeNameAccessor (&VideoProducer::m_postfix),
                   MakeNameChecker ())
    //.AddAttribute ("PayloadSize", "Virtual payload size for Content packets",
    //              UintegerValue (1024),
    //               MakeUintegerAccessor (&Producer::m_virtualPayloadSize),
    //               MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&VideoProducer::m_freshness),
                   MakeTimeChecker ())
	.AddAttribute("RewardUnit",
				  "Reward increment",
				  StringValue("1"),
				  MakeDoubleAccessor(&VideoProducer::m_unit),
				  MakeDoubleChecker<double>())

	.AddAttribute("RewardDesign",
				  "Reward Design Choice (Pattern)",
				  StringValue("1"),
				  MakeUintegerAccessor(&VideoProducer::m_design),
				  MakeUintegerChecker<uint32_t>())
    //.AddAttribute ("Signature", "Fake signature, 0 valid signature (default), other values application-specific",
    //               UintegerValue (0),
    //               MakeUintegerAccessor (&VideoProducer::m_signature),
    //               MakeUintegerChecker<uint32_t> ())
    //.AddAttribute ("KeyLocator", "Name to be used for key locator.  If root, then key locator is not used",
    //               NameValue (),
    //               MakeNameAccessor (&VideoProducer::m_keyLocator),
    //              MakeNameChecker ())
    ;
  return tid;
}

VideoProducer::VideoProducer ()
{
	  /*
	   * In real case, the server(producer)owns all information about the format of video (including its size) and deliver such info
	   * to consumer through MEPG standard
	   */
/*
	m_dict_ratetosize["250kbps"] = 62.5;
	m_dict_ratetosize["400kbps"] = 100;
	m_dict_ratetosize["600kbps"] = 150;
	m_dict_ratetosize["900kbps"] = 225;
*/
}

// inherited from Application base class.
void
VideoProducer::StartApplication ()
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_ASSERT (GetNode ()->GetObject<Fib> () != 0);

  App::StartApplication ();

  NS_LOG_DEBUG ("[Video Producer] on NodeID:" << GetNode()->GetId() << " Starts");

  Ptr<Fib> fib = GetNode ()->GetObject<Fib> ();

  Ptr<fib::Entry> fibEntry = fib->Add (m_prefix, m_face, 0);

  fibEntry->UpdateStatus (m_face, fib::FaceMetric::NDN_FIB_GREEN);

  // // make face green, so it will be used primarily
  // StaticCast<fib::FibImpl> (fib)->modify (fibEntry,
  //                                        ll::bind (&fib::Entry::UpdateStatus,
  //                                                  ll::_1, m_face, fib::FaceMetric::NDN_FIB_GREEN));
}

void
VideoProducer::StopApplication ()
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_ASSERT (GetNode ()->GetObject<Fib> () != 0);

  App::StopApplication ();
}


void
VideoProducer::OnInterest (Ptr<const Interest> interest)
{
  App::OnInterest (interest); // tracing inside

  NS_LOG_FUNCTION (this << interest);

  if (!m_active) return;
  //The Payload of packet is counted as Byte!!!!
  Ptr<Data> data = Create<Data> (Create<Packet> (m_node->GetObject<NDNBitRate>()->GetChunkSize(ExtractBitrate(interest->GetName()))
		  * 1e3));
  data->SetName (Create<Name> (interest->GetName ()));
  data->SetAccumulatedReward(GetReward(interest));

  int32_t currentTSI = interest->GetTSI();
  currentTSI--;
  data->SetTSI(currentTSI);

  data->SetTSB(-1);
  data->InitAcc();
  data->AddAcc(interest->GetAcc());
  //data->SetPFlag(1);

  //dataName->append (m_postfix);
  //data->SetFreshness (m_freshness);
  //data->SetTimestamp (Simulator::Now());

  NS_LOG_DEBUG ("[Video Producer] DATA with name:" << data->GetName () << "\tFile:" <<
		  static_cast<uint32_t>(interest->GetName().get(-2).toNumber()) << " Chunk:" <<
		  static_cast<uint32_t>(interest->GetName().get(-1).toNumber()) << "\n" <<
		  "with size: "<< data->GetPayload()->GetSize() / 1e3 << "KB\tat TimeStamp: " << Simulator::Now().ToDouble(Time::S));
  m_face->ReceiveData (data);
  m_transmittedDatas (data, this, m_face);
}

std::string VideoProducer::ExtractBitrate(const Name& n)
{
	std::string name = n.toUri();
	std::size_t found = name.find("br");
	std::string r = name;
	if (found != std::string::npos)
	{
		std::size_t format = name.find("/",found + 2);
		if(format != std::string::npos)
			r = name.substr(found + 2, format - found - 2);
	}
	return r;
}

double
VideoProducer::GetReward(Ptr<const Interest> interest)
{
	uint64_t bd = interest->GetBRBoundary();
	uint32_t BD = static_cast<uint8_t>(bd) & 0x0f;
	Ptr<NDNBitRate> m_BRinfo = m_node->GetObject<NDNBitRate>();

	uint32_t actualBR = m_BRinfo->GetRankFromBR(ExtractBitrate(interest->GetName()));

	double r = 0;
	if(BD == 0)
		r = m_BRinfo->GetRewardFromRank(1);
	else
	{
		if(BD > actualBR)
		{

			double newrank = actualBR + m_unit;
			r = m_BRinfo->GetRewardFromRank(actualBR+1) * (1 / newrank)
			    + m_BRinfo->GetRewardFromRank(actualBR) * (1 - 1 / newrank);

			//r = m_BRinfo->GetRewardFromRank(reqBR+1);
		}
		else if(BD == actualBR)
		{
			r = m_BRinfo->GetRewardFromRank(actualBR);
		}
		else
		{
			if(m_design == 1)
			{
				r = m_BRinfo->GetRewardFromRank(actualBR - 1);
			}
			else if(m_design == 2)
			{
				r = m_BRinfo->GetRewardFromRank(BD);
			}
		}

	}
	return r;
}

} // namespace ndn
} // namespace ns3
