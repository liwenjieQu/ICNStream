/*
 * Author: Wenjie Li
 * Implement FESTIVE rate adaptation algorithm, proposed by
 * Junchen Jiang et. cl.
 * URL: http://dl.acm.org/citation.cfm?id=2413189
 * Conference: CoNEXT
 */
#include "ndn-festive-videoconsumer.h"
#include "ns3/ndn-bitrate.h"
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
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <utility>

NS_LOG_COMPONENT_DEFINE("ndn.VideoClient");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(VideoClient);

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
 * consumerHelper.SetAttribute("NumberofChunks","");
 * consumerHelper.SetAttribute("ProbCache", "")
 * consumerHelper.SetAttribute("StartMeasureTime", "")
 *
 */
uint32_t VideoClient::NumApps = 0;

TypeId VideoClient::GetTypeId(void) {
	static TypeId tid =
			TypeId("ns3::ndn::VideoClient").SetGroupName("Ndn").SetParent<
					ConsumerZipfMandelbrot>().AddConstructor<VideoClient>()

			.AddAttribute ("RetxTimer",
				    "Timeout defining how frequent retransmission timeouts should be checked",
				     StringValue ("1s"),
				     MakeTimeAccessor (&VideoClient::m_retxTimer),
				     MakeTimeChecker ())

			.AddAttribute("ProbCache",
					"The probability of watching the next chunk",
					StringValue("0.7"),
					MakeDoubleAccessor(&VideoClient::m_cacheProfit),
					MakeDoubleChecker<double>())

			.AddAttribute("CombineWeight",
					"The weight of combining efficiency and stability scores",
					StringValue("12"),
					MakeDoubleAccessor(&VideoClient::m_weight),
					MakeDoubleChecker<double>())

			.AddAttribute("TargetBuffer",
					"The threshold of buffer holding duration of video (in seconds)",
					StringValue("20"),
					MakeDoubleAccessor(&VideoClient::m_targetbuff),
					MakeDoubleChecker<double>())

			.AddAttribute("DropThreshold",
					"The threshold of degrading selected bitrate",
					StringValue("1.0"),
					MakeDoubleAccessor(&VideoClient::m_dropthres),
					MakeDoubleChecker<double>())

			.AddAttribute ("WatchingTimeStamp", "the time when the last data packet was received",
					StringValue ("0s"),
					MakeTimeAccessor (&VideoClient::m_lastwatchpoint),
				    MakeTimeChecker ())

			.AddAttribute("HistoryWindowSize",
					"The size of history window of bandwidth measurement by each client (unit: segment)",
					StringValue("5"),
					MakeUintegerAccessor(&VideoClient::m_historyWindow),
					MakeUintegerChecker<uint32_t>())

    		.AddTraceSource ("VideoPlaybackStatus", "Data describe the adaptive streaming playback",
    				MakeTraceSourceAccessor (&VideoClient::m_videoPlayTrace))

			.AddTraceSource ("VideoSwitch", "Record the number of switches during each video file playback",
					MakeTraceSourceAccessor (&VideoClient::m_switchTrace))

			.AddTraceSource ("DelaybyHop", "Record the Queuing delay by hops",
					MakeTraceSourceAccessor (&VideoClient::m_qdelayTrace));
	return tid;
}

VideoClient::VideoClient()
	:m_enableRecord(false),
	 m_transition(0),
	 m_retxbuff(0),
	 m_recoverFromFreeze(false),
	 m_rewardcount(0),
	 m_rewardvalue(0)
{
	m_appid = NumApps;
	NumApps++;
}

VideoClient::~VideoClient() {

}

void
VideoClient::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION_NOARGS ();

  // do base stuff
  NS_LOG_INFO("[Video Consumer] appID:" << m_appid << " Starts at: " << Simulator::Now());
  App::StartApplication ();
  ScheduleNextVideoFile ();
}

void
VideoClient::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS ();
  // cleanup base stuff
  App::StopApplication ();
}

void VideoClient::ScheduleNextVideoFile()
{
	if (m_firstTime)
	{
		UniformVariable initiate(0.0, 20.0);
		m_fileinTransmission = true;
		m_recoverFromFreeze = false;
		double starttime = initiate.GetValue();
		Simulator::Schedule(Seconds(starttime), &VideoClient::SendVideofile, this);
		m_firstTime = false;

		double nextinterval = m_random->GetValue() + starttime;
		m_previousBitRate = m_node->GetObject<NDNBitRate>()->GetInitBitRate();
		Simulator::Schedule(
						Seconds(nextinterval),
				&VideoClient::ScheduleNextVideoFile, this);
	}
	else
	{
		if(m_fileinTransmission)
		{
			Simulator::Schedule(Seconds(m_currentbuff > 2 ? m_currentbuff : 2), &VideoClient::ScheduleNextVideoFile, this);
		}
		else
		{
			m_estimatedBandwidth.clear();
			m_selectedBandwidth.clear();
			m_currentBitRate = "";

			m_fileinTransmission = true;
			m_recoverFromFreeze = false;
			SendVideofile();

			double nextinterval = m_random->GetValue();
			m_previousBitRate = m_node->GetObject<NDNBitRate>()->GetInitBitRate();
			Simulator::Schedule(
							Seconds(nextinterval),
					&VideoClient::ScheduleNextVideoFile, this);
		}
	}
}

void VideoClient::SendVideofile() {
	if (!m_active)
		return;
	NS_LOG_FUNCTION_NOARGS ();

	uint32_t fileseq = GetNextSeq();	//Using Zipf distribution to determine the next file sequence;
	m_currentBitRate = m_node->GetObject<NDNBitRate>()->GetInitBitRate();
	m_currentbuff = 0;
	m_numSwitchUp = 0;
	m_numSwitchDown = 0;
	m_lastwatchpoint = Simulator::Now();
	m_selectedBandwidth.push_back(m_currentBitRate);

	SendPacket(fileseq, 1, m_currentBitRate);
}


void VideoClient::SendPacket(uint32_t fileseq, uint32_t chunkseq, std::string bitrate) {
	if (!m_active)
		return;NS_LOG_FUNCTION_NOARGS ();

	uint32_t nextseq = std::numeric_limits<uint32_t>::max(); //invalid

	if(m_retxSeqs.size() > 1)
	{
		std::cout << "Retransmission Logic has fallen!\n";
		return;//safety
	}

	if(m_retxSeqs.size() == 0 && fileseq == 0)
	{
		std::cout << "I am In!\n";
		return;
	}

	if(m_retxSeqs.size() == 1)
	{
		nextseq = *m_retxSeqs.begin();
		m_retxSeqs.erase(m_retxSeqs.begin());
	}

	if (nextseq == std::numeric_limits<uint32_t>::max()) {//no retransmission
		if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
			if (m_seq >= m_seqMax) {
				return; // we are totally done
			}
		}
		//m_seq++;
		nextseq = (fileseq - 1) * m_maxNumchunk + chunkseq;
	}

	Ptr<Name> nameWithSequence = Create<Name>(m_interestName);
	std::string s_bitrate = "br" + bitrate;
	nameWithSequence->append(s_bitrate);

	uint32_t calchunkseq = nextseq % m_maxNumchunk;
	if(calchunkseq == 0)
		calchunkseq = m_maxNumchunk;
	uint32_t calfileseq = ((nextseq - calchunkseq) / m_maxNumchunk) + 1;

	nameWithSequence->appendNumber(calfileseq);
	nameWithSequence->appendNumber(calchunkseq);

	Ptr<Interest> interest = Create<Interest>();
	interest->SetTSI(0);
	interest->DecTSI();
	interest->InitAcc();
	interest->SetNonce(m_rand.GetValue());
	interest->SetName(nameWithSequence);
	interest->SetInterestLifetime(m_interestLifeTime);

	uint8_t PR = 0;
	uint64_t BRmask = this->SetBRBoundary(s_bitrate);
	interest->SetBRBoundary(BRmask);
	m_ProducerRank = PR;

	if(!m_recoverFromFreeze)
		m_retransmitBitRate = m_currentBitRate;

	NS_LOG_INFO("[VideoClient] on Node:" << m_node->GetId() << "; Interest with name:"
			<< nameWithSequence->toUri() <<"\tat TimeStamp: " << Simulator::Now ().ToDouble(Time::S));

	WillSendOutInterest (nextseq);

	//Set Retransmission Timer
	double checkbuff = (!m_recoverFromFreeze && calchunkseq > 1)
						? m_currentbuff - (Simulator::Now() - m_lastwatchpoint).ToDouble(Time::S)
						: m_interestLifeTime.GetSeconds();
	if(m_retxEvent.IsRunning())
		m_retxEvent.Cancel();
	m_retxEvent = Simulator::Schedule (Time(Seconds(checkbuff)), &VideoClient::CheckRetxTimeout, this, checkbuff);

	FwHopCountTag hopCountTag;
	interest->GetPayload()->AddPacketTag(hopCountTag);
	m_transmittedInterests(interest, this, m_face);
	m_face->ReceiveInterest(interest);

}

/*
 * implement a pure virtual function
 */
void VideoClient::ScheduleNextPacket()
{

}

void
VideoClient::CheckRetxTimeout(double checkbuff)
{
	if(m_fileinTransmission)
	{
		while (!m_seqTimeouts.empty ())
		{
			SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
		        m_seqTimeouts.get<i_timestamp> ().begin ();
			uint32_t seqNo = entry->seq;

			//(checkbuff * 1e3) - 1 : Solve Precision Issue
			if (entry->time + Time(MilliSeconds(static_cast<uint64_t>((checkbuff * 1e3) - 1))) <= Simulator::Now())
				{
					m_seqTimeouts.get<i_timestamp> ().erase (entry);
					auto iter = m_seqRetxCounts.find(seqNo);
					if(iter != m_seqRetxCounts.end())
						iter->second++;
					else
						m_seqRetxCounts[seqNo] = 1;
					if(!m_recoverFromFreeze)
						m_retxbuff = m_currentbuff;
					m_currentbuff = 0;
					m_recoverFromFreeze = true;
					VideoClient::OnTimeout (seqNo);
				}
		      else
		      {
		    	  std::cout << m_seqRetxCounts[seqNo] << std::endl;
		    	  std::cout << "CheckRetxTimeout:: Condition not satisfied! CANNOT HAPPEN!\n";
		    	  break; // nothing else to do. All later packets need not be retransmitted
		      }
		}
	}
}

void
VideoClient::OnTimeout (uint32_t sequenceNumber)
{
	  NS_LOG_INFO("Timeout!" << Simulator::Now().ToDouble(Time::S));
	  NS_LOG_FUNCTION (sequenceNumber);
	  // std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " << m_rtt->RetransmitTimeout ().ToDouble (Time::S) << "s\n";

	  uint32_t calchunkseq = sequenceNumber % m_maxNumchunk;
	  if(calchunkseq == 0)
		  calchunkseq = m_maxNumchunk;
	  uint32_t calfileseq = ((sequenceNumber - calchunkseq) / m_maxNumchunk) + 1;
	  NS_LOG_INFO("!!![VideoClient] on AppID:" << m_appid << "; RESEND interest on FileID: "
			  << calfileseq << "; ChunkID: "<< calchunkseq);

	  m_retxSeqs.insert (sequenceNumber);

      if(m_estimatedBandwidth.size() == m_historyWindow)
    	  m_estimatedBandwidth.pop_front();
      //Time delay = Time(MilliSeconds(static_cast<uint64_t>(m_currentbuff * 1e3)));
      //m_estimatedBandwidth.push_back(m_node->GetObject<NDNBitRate>()->GetChunkSize(m_currentBitRate) * 8 / delay.GetSeconds());
      Ptr<NDNBitRate> ndnbr = m_node->GetObject<NDNBitRate>();
      double t = ndnbr->GetPlaybackTime();
      m_estimatedBandwidth.push_back(ndnbr->GetChunkSize(ndnbr->GetInitBitRate()) * 8 / (t * 4));

	  ScheduleNextVideoChunk(0,0,0);
}


void
VideoClient::WillSendOutInterest(uint32_t sequenceNumber)
{
	  m_seqTimeouts.insert (SeqTimeout (sequenceNumber, Simulator::Now ()));
	  m_seqFullDelay.insert (SeqTimeout (sequenceNumber, Simulator::Now ()));

	  m_seqLastDelay.erase (sequenceNumber);
	  m_seqLastDelay.insert (SeqTimeout (sequenceNumber, Simulator::Now ()));
}
void
VideoClient::ScheduleNextVideoChunk(uint32_t fileseq, uint32_t chunkseq, double t)
{
	Time interval(Seconds(t));
    if(m_selectedBandwidth.size() == m_historyWindow)
    {
    	std::string previousStr = m_currentBitRate;
    	m_currentBitRate = QuantizeToBitrate(SmoothBandwidth(BandwidthEstimator()));
    	/*
    	if(chunkseq == 15 && previousStr == "900kbps" && (m_currentBitRate == "250kbps" || m_currentBitRate == "400kbps"))
    		NS_LOG_WARN("Time: " << Simulator::Now().ToDouble(Time::S) << "; Appid:" << m_appid << "; Sending file:"
    				<<fileseq << "; chunk:" << chunkseq<< "\nNext using BitRate: " << m_currentBitRate);
    	*/
    	m_selectedBandwidth.pop_front();
    }
    else
    {
    	m_currentBitRate = m_node->GetObject<NDNBitRate>()->GetInitBitRate();
    }
    m_selectedBandwidth.push_back(m_currentBitRate);
	Simulator::Schedule(interval, &VideoClient::SendPacket, this, fileseq, chunkseq, m_currentBitRate);
}

double
VideoClient::BandwidthEstimator()
{
	std::list<double>::iterator historyiter = m_estimatedBandwidth.begin();
	double harmonic_sum = 0;
	for(; historyiter != m_estimatedBandwidth.end(); historyiter++)
	{
		harmonic_sum += 1 / (*historyiter);
	}
	return m_estimatedBandwidth.size()/harmonic_sum;
}

double
VideoClient::SmoothBandwidth(double b)
{
	//In FESTIVE, the task of smoothing the measured bandwidth is done by BandwidthEstimator
	return b;
}

std::string
VideoClient::QuantizeToBitrate(double bandwidth)
{
	//implement the bitrate selection logic of FESTIVE
	return DelayedUpdate(ReferenceBR(bandwidth), bandwidth);
}

/*
 * Return the reference bitrate
 */
std::string
VideoClient::ReferenceBR(double bandwidth)
{
	std::string tail = "kbps";
	std::size_t found = m_currentBitRate.find(tail);
	double currentBR = std::stod(m_currentBitRate.substr(0, found));
	std::string refBR;


	uint32_t rank = m_node->GetObject<NDNBitRate>()->GetRankFromBR(m_currentBitRate);
	if(bandwidth >= currentBR)
	{
		uint32_t v = 0;
		for(auto iter = m_selectedBandwidth.rbegin(); iter != m_selectedBandwidth.rend(); iter++)
		{
			if(*iter == m_currentBitRate)
				v++;
			else
				break;
		}
		if(v >= rank)
			refBR = m_node->GetObject<NDNBitRate>()->GetNextHighBitrate(rank);
		else
			refBR = m_currentBitRate;
	}
	else
	{
		if(bandwidth < m_dropthres * currentBR)
			refBR = m_node->GetObject<NDNBitRate>()->GetNextLowBitrate(rank);
		else
			refBR = m_currentBitRate;
	}
	return refBR;
}

std::string
VideoClient::DelayedUpdate(std::string refbwstr, double estbw)
{
	std::string tail = "kbps";
	std::size_t found = m_currentBitRate.find(tail);
	double currentBR = std::stod(m_currentBitRate.substr(0, found));
	double refBR = std::stod(refbwstr.substr(0, found));

	double minbw = refBR > estbw ? estbw : refBR;

	double final_ref, final_cur;

	double score_eff = std::abs((refBR / minbw) - 1);
	double score_stab = std::pow(2, NumberofSwitches()) + 1;
	final_ref = m_weight * score_eff + score_stab;

	score_eff = std::abs((currentBR / minbw) - 1);
	score_stab = std::pow(2, NumberofSwitches());
	final_cur = m_weight * score_eff + score_stab;

	return final_ref < final_cur ? refbwstr : m_currentBitRate;
}

uint32_t
VideoClient::NumberofSwitches()
{
	uint32_t switches = 0;
	std::list<std::string>::iterator iter = m_selectedBandwidth.begin();
	if(iter != m_selectedBandwidth.end())
		iter++;
	for(; iter != m_selectedBandwidth.end(); iter++)
	{
		auto comp = iter;
		comp--;
		if(*iter != *comp)
			switches++;
	}
	return switches;
}

void
VideoClient::AddToCondition(double r)
{
	m_rewardvalue += r;
	m_rewardcount++;
}

std::pair<uint32_t, double>
VideoClient::GetCondition()
{
	return std::make_pair(m_rewardcount, m_rewardvalue);
}

void
VideoClient::OnData (Ptr<const Data> data)
{
  if (!m_active)
	  return;

  App::OnData (data); //tracing inside
  NS_LOG_FUNCTION (this << data);

  uint32_t chunkid = data->GetName ().get (-1).toNumber();
  uint32_t fileid = data->GetName().get(-2).toNumber();
  std::string bitrate = data->GetName().get(-3).toUri().substr(2);
  uint32_t seq = (fileid - 1) * m_maxNumchunk + chunkid;

  uint32_t hopCount = data->GetTSI() + 1;
  double reward = data->GetAccumulatedReward();
  AddToCondition(reward);
  SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find (seq);

  if (entry != m_seqLastDelay.end () && bitrate == m_currentBitRate)//Find it in the container, the interest has been sent before
  {
	  if(m_retxEvent.IsRunning())
		  m_retxEvent.Cancel();

	  UpdateDelay(bitrate, data->GetHops(), data->m_mostRecentDelay, data);
      if((Simulator::Now().Compare(m_measureStart) >= 0 && m_noCachePartition)
    	  || m_enableRecord)
		  m_qdelayTrace(data, bitrate, m_transition);

      // Insert the measured bandwidth to the link list
      if(m_estimatedBandwidth.size() == m_historyWindow)
    	  m_estimatedBandwidth.pop_front();
	  auto firstinsert = m_seqFullDelay.find(seq);
       Time delay = Simulator::Now () - firstinsert->time;
      if(m_retransmitBitRate != m_currentBitRate || delay > m_interestLifeTime)
      {
    	  delay = Simulator::Now () - entry->time;
      }
      double receivedBW = m_node->GetObject<NDNBitRate>()->GetChunkSize(bitrate) * 8 / delay.GetSeconds();
      m_estimatedBandwidth.push_back(receivedBW);

      Time watching_delta = Simulator::Now() - m_lastwatchpoint;

      double tracebuff;
      if(!m_recoverFromFreeze)
      {
    	  if(chunkid == 1)
    		  m_currentbuff = m_node->GetObject<NDNBitRate>()->GetPlaybackTime();
    	  else
        	  m_currentbuff = m_currentbuff + m_node->GetObject<NDNBitRate>()->GetPlaybackTime() - watching_delta.GetSeconds();
    	  if(m_currentbuff < 0)
    		  std::cout << "OnData: m_currentbuff < 0, CANNOT HAPPEN!\n";
    	  tracebuff = m_currentbuff;
      }
      else
      {
    	  m_recoverFromFreeze = false;
    	  m_currentbuff = m_node->GetObject<NDNBitRate>()->GetPlaybackTime();
    	  //tracebuff = (-1) * static_cast<double>(m_seqRetxCounts[seq]);
    	  tracebuff = (-1) * (watching_delta.ToDouble(Time::S) - m_retxbuff);
    	  m_retxbuff = 0;
      }

      ////////////////////////////
      /*
      if(m_appid == 61 && Simulator::Now() > Time(Seconds(7458)))
    	  NS_LOG_DEBUG("NowTime: " << Simulator::Now().ToDouble(Time::S)
		  	  	  	<< "\nLastTime: " << m_lastwatchpoint.ToDouble(Time::S)
		  	  	  	<< "\nWatchingDelta: " << watching_delta.ToDouble(Time::S)
					<< "\nBuffer: " << m_currentbuff);
      ////////////////////////////*/

      m_lastwatchpoint = Simulator::Now();

      if((Simulator::Now().Compare(m_measureStart) >= 0 && m_noCachePartition)
    	  || m_enableRecord)
      {
    	  if(m_currentBitRate > m_previousBitRate)
    		  m_numSwitchUp++;
    	  else if(m_currentBitRate < m_previousBitRate)
    		  m_numSwitchDown++;
      }
      m_previousBitRate = m_currentBitRate;

      //NS_LOG_DEBUG("[VideoClient] Data for FileID: "<<fileid<<"; ChunkID: " <<chunkid<<"; BitRate:" << bitrate << "; Throughput: "
    	//	  << receivedBW << "; Delay: " << Simulator::Now ()-entry->time);

      bool request_next = true;
      if (chunkid == m_maxNumchunk)
    	  request_next = false;
      else
      {
	      UniformVariable t_keepWatching(0.0, 1.0);
	      if(t_keepWatching.GetValue() > m_cacheProfit)
	    	  request_next = false;
      }

      if(request_next)
      {
    	  //NS_LOG_DEBUG("[VideoClient] on AppID: " << m_appid << "; FileID: "<< fileid <<". Schedule for next chunk" );
    	  ScheduleNextVideoChunk(fileid, chunkid + 1, RandomSchedule());

    	  if((Simulator::Now().Compare(m_measureStart) >= 0 && m_noCachePartition)
        	  || m_enableRecord)
    	  {
        	  std::tuple<uint32_t, uint32_t, uint32_t> id(m_appid, fileid, chunkid);
        	  m_videoPlayTrace(id, bitrate, m_currentBitRate, delay.GetMilliSeconds(), tracebuff, reward,
        			  hopCount, //m_ProducerRank,
			      m_transition);
    	  }

      }
      else
      {
    	  //NS_LOG_DEBUG("[VideoClient] on AppID: " << m_appid << "; FileID: "<< fileid <<". No Schedule for next chunk");
    	  m_fileinTransmission = false;
          if((Simulator::Now().Compare(m_measureStart) >= 0 && m_noCachePartition)
        	  || m_enableRecord)
          {
    		  m_switchTrace(this, m_appid, fileid, m_numSwitchUp, m_numSwitchDown, m_transition);
        	  std::tuple<uint32_t, uint32_t, uint32_t> id(m_appid, fileid, chunkid);
        	  m_videoPlayTrace(id, bitrate, bitrate, delay.GetMilliSeconds(), tracebuff, reward,
        			  hopCount, //m_ProducerRank,
				  m_transition);
          }
      }
      m_seqRetxCounts.erase (seq);
      m_seqFullDelay.erase (seq);
      m_seqLastDelay.erase (seq);

      m_seqTimeouts.erase (seq);
      m_retxSeqs.erase (seq);
  }
  else
	  NS_LOG_INFO("[VideoClient] Duplicate Data:" << Simulator::Now().ToDouble(Time::S) << " CurrentBR:" << m_currentBitRate<< "; DataHeader:" << bitrate);
}

double
VideoClient::RandomSchedule()
{
	double dtime = m_node->GetObject<NDNBitRate>()->GetPlaybackTime();
	UniformVariable range(-dtime, dtime);
	double theta = range.GetValue();
	if(m_currentbuff < m_targetbuff + theta)
		return 0;
	else
		return m_currentbuff - m_targetbuff - theta;
}

void
VideoClient::UpdateDelay(const std::string& BR, uint8_t numHops, const Time& recentDelay, Ptr<const Data> data)
{
	auto iter = m_delaybyhop.find(BR);

	std::map<std::string, std::vector<uint32_t> >::iterator timeptr;
	timeptr = m_recentinsertTime.find(BR);

	std::map<std::string, std::vector<std::vector<uint32_t> > >::iterator Dhistoryptr;
	Dhistoryptr = m_delayHistory.find(BR);

	if(iter == m_delaybyhop.end())
	{
		std::vector<std::deque<uint32_t> > emptyRecord;
		m_delaybyhop.insert(std::make_pair(BR, std::move(emptyRecord)));
		iter = m_delaybyhop.find(BR);

		std::vector<std::vector<uint32_t> > emptyHistory;
		m_delayHistory.insert(std::make_pair(BR, std::move(emptyHistory)));
		Dhistoryptr = m_delayHistory.find(BR);

		std::vector<uint32_t> emptystamp;
		m_recentinsertTime.insert(std::make_pair(BR, std::move(emptystamp)));
		timeptr = m_recentinsertTime.find(BR);
	}

	for(uint8_t hop = 1; hop <= numHops; hop++)
	{
		uint32_t record = 0;
		if(hop == 1)
			record = static_cast<uint32_t>(recentDelay.ToInteger (Time::MS));
		else
			record = data->GetDelay(hop - 2);
		if(iter->second.size() < hop)
		{
			std::deque<uint32_t> history;
			history.push_back(record);
			iter->second.push_back(std::move(history));

			std::vector<uint32_t> slots;
			slots.push_back(record);
			Dhistoryptr->second.push_back(std::move(slots));

			timeptr->second.push_back(static_cast<uint32_t>(Simulator::Now().ToInteger(Time::S)));
		}
		else
		{
			/*
			 * If delay data is recorded for a certain bit rate long time ago (based on request frequency m_frequency),
			 * deque needs to be refreshed.
			 */
			if(Time(Seconds(timeptr->second[hop - 1])) + Time(Seconds(1.0 * 2 / m_frequency)) <= Simulator::Now())
			{
				iter->second[hop - 1].clear(); // Clear the delay history
				NS_LOG_INFO("[UpdateDelay] Node: " << m_node->GetId() << " Time: " << Simulator::Now().ToDouble(Time::S)
						<< " Clear History for: " << iter->first << " Hop: " << static_cast<uint32_t>(hop));
			}
			else if(iter->second[hop - 1].size() > VideoClient::DelayWindow)
				iter->second[hop - 1].pop_front();
			iter->second[hop - 1].push_back(record);
			timeptr->second[hop - 1] = static_cast<uint32_t>(Simulator::Now().ToInteger(Time::S));
			Dhistoryptr->second[hop - 1].push_back(record);
		}
	}
}
/*
uint64_t
VideoClient::SetBRBoundary(uint8_t& producerRank)
{
	Ptr<NDNBitRate> BRinfoPtr = m_node->GetObject<NDNBitRate>();
	double limit = BRinfoPtr->GetPlaybackTime();
	uint64_t bd = 0;
	uint8_t currentbd = 0;
	uint8_t routelen = 0;
	auto lenptr = m_delaybyhop.find(BRinfoPtr->GetInitBitRate());
	if(lenptr != m_delaybyhop.end())
		routelen = lenptr->second.size();
	producerRank = 0;

	for(uint32_t i = BRinfoPtr->GetTableSize(); i >= 1; i--)
	{
		auto iter = m_delaybyhop.find(BRinfoPtr->GetBRFromRank(i));
		if(iter != m_delaybyhop.end())
		{
			double accumulatedDelay = 0;
			for(uint32_t hop = 1; hop <= iter->second.size(); hop++)
			{
				uint32_t avg = 0;
				for(auto j = iter->second[hop - 1].begin(); j != iter->second[hop - 1].end(); j++)
					avg = avg + (*j);
				accumulatedDelay += (static_cast<double>(avg) / iter->second[hop - 1].size())/1000; //convert from MS to S
				if(accumulatedDelay <= limit && hop > currentbd)
				{
					uint64_t mask = i << (currentbd * 4);
					bd = bd | mask;
					currentbd++;
					if(currentbd == routelen)
						producerRank = static_cast<uint8_t>(i);
				}
				if(accumulatedDelay > limit && hop < 6 && i == 1) // 5 Hops from Consumer to Producer
					NS_LOG_DEBUG("[SetBoundary] Node: "<<m_node->GetId()
							<< " Time: " << Simulator::Now().ToDouble(Time::S) << " Smallest BR cannot serve!!!");
			}
		}
	}
	return bd;
}
*/

uint64_t
VideoClient::SetBRBoundary(const std::string& reqbr)
{
	Ptr<NDNBitRate> BRinfoPtr = m_node->GetObject<NDNBitRate>();
	double limit = BRinfoPtr->GetPlaybackTime();
	double basesize = BRinfoPtr->GetChunkSize(reqbr.substr(2));
	uint32_t baserank = BRinfoPtr->GetRankFromBR(reqbr.substr(2));

	uint64_t bd = 0;
	uint8_t currentbd = 0;

	uint32_t superrank = baserank + 1;
	double accumulatedDelay = 0;
	auto baseiter = m_delaybyhop.find(BRinfoPtr->GetBRFromRank(baserank));
	if(superrank <= BRinfoPtr->GetTableSize())
	{
		double supersize = BRinfoPtr->GetChunkSize(BRinfoPtr->GetBRFromRank(superrank));
		if(baseiter != m_delaybyhop.end())
		{
			for(uint32_t hop = 1; hop <= baseiter->second.size(); hop++)
			{
				uint32_t avg = 0;
				for(auto j = baseiter->second[hop - 1].begin(); j != baseiter->second[hop - 1].end(); j++)
					avg = avg + (*j);
				double currentHopDelay = (static_cast<double>(avg) / baseiter->second[hop - 1].size())/1000;//convert from MS to S
				if(accumulatedDelay + currentHopDelay <= (limit * basesize / supersize) && hop > currentbd)
				{
					accumulatedDelay += currentHopDelay;
					uint64_t mask = superrank << (currentbd * 4);
					bd = bd | mask;
					currentbd++;
				}
				else
					break;
			}
		}
	}
	if(baseiter != m_delaybyhop.end())
	{
		for(uint32_t hop = currentbd + 1; hop <= baseiter->second.size(); hop++)
		{
			uint32_t avg = 0;
			for(auto j = baseiter->second[hop - 1].begin(); j != baseiter->second[hop - 1].end(); j++)
				avg = avg + (*j);
			double currentHopDelay = (static_cast<double>(avg) / baseiter->second[hop - 1].size())/1000;//convert from MS to S
			if(accumulatedDelay + currentHopDelay <= limit && hop > currentbd)
			{
				accumulatedDelay += currentHopDelay;
				uint64_t mask = baserank << (currentbd * 4);
				bd = bd | mask;
				currentbd++;
			}
			else
				break;
		}

		if(baserank > 1)
		{
			for(uint32_t lowerrank = baserank - 1; lowerrank >= 1; lowerrank--)
			{
				double lowersize = BRinfoPtr->GetChunkSize(BRinfoPtr->GetBRFromRank(lowerrank));
				for(uint32_t hop = currentbd + 1; hop <= baseiter->second.size(); hop++)
				{
					uint32_t avg = 0;
					for(auto j = baseiter->second[hop - 1].begin(); j != baseiter->second[hop - 1].end(); j++)
						avg = avg + (*j);
					double currentHopDelay = (static_cast<double>(avg) / baseiter->second[hop - 1].size())/1000;//convert from MS to S

					if(accumulatedDelay + currentHopDelay <= (limit * basesize / lowersize) && hop > currentbd)
					{
						accumulatedDelay += currentHopDelay;
						uint64_t mask = lowerrank << (currentbd * 4);
						bd = bd | mask;
						currentbd++;
					}
					else
						break;
				}
			}
		}
	}

	return bd;
}
std::pair<uint32_t, uint32_t>
VideoClient::DelayHistory(const std::string& BR, uint32_t hop)
{
	auto iter = m_delayHistory.find(BR);
	if(iter != m_delayHistory.end())
	{
		uint32_t num = 0;
		uint32_t totaldelay = 0;
		if(hop <= iter->second.size())
		{
			num += iter->second[hop - 1].size();
			for(uint32_t i = 0; i < num; i++)
				totaldelay += iter->second[hop - 1][i];
			iter->second[hop - 1].clear();
			return std::make_pair(num, totaldelay);
		}
	}
	return std::make_pair(0, 0);
}
} /* namespace ndn */
} /* namespace ns3 */
