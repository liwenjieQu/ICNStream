/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:  Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *          Ilya Moiseenko <iliamo@cs.ucla.edu>
 */

#include "ndn-forwarding-strategy.h"

#include "ns3/ndn-pit.h"
#include "ns3/ndn-pit-entry.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-fib.h"
#include "ns3/ndn-content-store.h"
#include "ns3/ndn-face.h"
#include "ns3/ndn-popularity.h"
#include "ns3/ndn-videocache.h"
#include "ns3/ndn-reward.h"

#include "ns3/assert.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"
#include "ns3/string.h"

#include "ns3/net-device.h"
#include "ns3/channel.h"
#include "../ndn-net-device-face.h"
#include "ns3/point-to-point-net-device.h"

#include "ns3/ndnSIM/utils/ndn-fw-hop-count-tag.h"

#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/tuple/tuple.hpp>

#include <map>

namespace ll = boost::lambda;

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (ForwardingStrategy);

NS_LOG_COMPONENT_DEFINE (ForwardingStrategy::GetLogName ().c_str ());


std::string
ForwardingStrategy::GetLogName ()
{
  return "ndn.fw";
}

TypeId ForwardingStrategy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ForwardingStrategy")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutInterests",  "OutInterests",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outInterests))
    .AddTraceSource ("InInterests",   "InInterests",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inInterests))
    .AddTraceSource ("DropInterests", "DropInterests", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropInterests))

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutData",  "OutData",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outData))
    .AddTraceSource ("InData",   "InData",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inData))
    .AddTraceSource ("DropData", "DropData", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropData))

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("SatisfiedInterests",  "SatisfiedInterests",  MakeTraceSourceAccessor (&ForwardingStrategy::m_satisfiedInterests))
    .AddTraceSource ("TimedOutInterests",   "TimedOutInterests",   MakeTraceSourceAccessor (&ForwardingStrategy::m_timedOutInterests))

	//.AddTraceSource("LinkData", "LinkData", MakeTraceSourceAccessor(&ForwardingStrategy::m_linkData))

    .AddAttribute ("CacheUnsolicitedDataFromApps", "Cache unsolicited data that has been pushed from applications",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ForwardingStrategy::m_cacheUnsolicitedDataFromApps),
                   MakeBooleanChecker ())
    
    .AddAttribute ("CacheUnsolicitedData", "Cache overheard data that have not been requested",
                   BooleanValue (false),
                   MakeBooleanAccessor (&ForwardingStrategy::m_cacheUnsolicitedData),
                   MakeBooleanChecker ())

    .AddAttribute ("DetectRetransmissions", "If non-duplicate interest is received on the same face more than once, "
                                            "it is considered a retransmission",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ForwardingStrategy::m_detectRetransmissions),
                   MakeBooleanChecker ())
	.AddAttribute("StartMeasureTime", "The length of simulation time during which the performance is measured",
				   StringValue("400s"),
				   MakeTimeAccessor(&ForwardingStrategy::m_period),
				   MakeTimeChecker())
/*
    .AddAttribute ("EnableInterestAggregation", "true: enable; false: forward any interest packet the router receives",
                  BooleanValue (true),
                  MakeBooleanAccessor (&ForwardingStrategy::m_choice),
                  MakeBooleanChecker ())
*/
    ;
  return tid;
}

ForwardingStrategy::ForwardingStrategy ()
{
}

ForwardingStrategy::~ForwardingStrategy ()
{
}

void
ForwardingStrategy::NotifyNewAggregate ()
{
  if (m_pit == 0)
    {
      m_pit = GetObject<Pit> ();
    }
  if (m_fib == 0)
    {
      m_fib = GetObject<Fib> ();
    }
  if (m_contentStore == 0)
    {
      m_contentStore = GetObject<ContentStore> ();
    }
  if(m_node == 0)
  {
	  m_node = GetObject<Node>();
  }
  if(m_stat_summary == 0)
  {
	  m_stat_summary = GetObject<PopularitySummary>();
  }
  if(m_stats == 0)
  {
	  m_stats = GetObject<VideoStatistics>();
  }

  Object::NotifyNewAggregate ();
}

void
ForwardingStrategy::DoDispose ()
{
  m_pit = 0;
  m_contentStore = 0;
  m_fib = 0;

  Object::DoDispose ();
}

/*
 * Authored by Wenjie Li
 * Add statistics to table (m_stats)
 */

void ForwardingStrategy::AddStatistics(Ptr<const Interest> interest)
{
	if(m_stats != 0)//Only for edge routers
	{
		m_stats->Add(VideoIndex {interest->GetName().get(-3).toUri(),
								 static_cast<uint32_t>(interest->GetName().get(-2).toNumber()),
								 static_cast<uint32_t>(interest->GetName().get(-1).toNumber())});
	}
}

/*
 * author: Wenjie Li
 * The default app will not generate similar interest packet
 *
 */
void
ForwardingStrategy::OnInterest (Ptr<Face> inFace,
                                Ptr<Interest> interest)
{
	NS_LOG_FUNCTION (inFace << interest->GetName ());
	m_inInterests (interest, inFace);

	Ptr<pit::Entry> pitEntry = m_pit->Lookup (*interest);//Only compare the Name
	bool similarInterest = true;
	if(pitEntry == 0)
	{
		similarInterest = false;
		pitEntry = m_pit->Create (interest);
		if(pitEntry != 0)
			DidCreatePitEntry (inFace, interest, pitEntry);
		else
		{
			FailedToCreatePitEntry (inFace, interest);
			return;
		}
	}

	AddStatistics(interest);

	Ptr<Data> contentObject = 0;
	double reqreward = 0;
	uint32_t delayresponse = 0;

	contentObject = m_contentStore->Lookup (interest, reqreward);
	NS_LOG_INFO("[FW]: (OnInterest)Node: " << m_node->GetId() << " for Interest:\n"
		  << interest->GetName() << "\n Reward: " << reqreward);

	if (contentObject == 0)
		contentObject = m_contentStore->LookForTranscoding(interest, delayresponse);

	if (contentObject != 0)
	{
		contentObject->SetTSB(0);
		contentObject->SetTSI(interest->GetTSI());
		contentObject->InitAcc();
		contentObject->AddAcc(interest->GetAcc());
		contentObject->SetSignature(m_node->GetId());
		contentObject->SetHops(0);
		//contentObject->SetPFlag(0);

		FwHopCountTag hopCountTag;
		if (interest->GetPayload ()->PeekPacketTag (hopCountTag))
		{
			contentObject->GetPayload ()->AddPacketTag (hopCountTag);
		}

		pitEntry->AddIncoming (inFace/*, Seconds (1.0)*/, reqreward);

		// Do data plane performance measurements
		// WillSatisfyPendingInterest (0, pitEntry);
		contentObject->SetTimestamp(Simulator::Now());
		// Actually satisfy pending interest

		if(delayresponse == 0)
			SatisfyPendingInterest (0, contentObject, pitEntry);
		else
			Simulator::Schedule(Time(MilliSeconds(delayresponse)),
					&ForwardingStrategy::DelayedSatisfyPendingInterest, this, contentObject, interest);

		return;
	}

	interest->AddTSI();
	interest->AddAcc(m_contentStore->GetCapacity());

	if (similarInterest && ShouldSuppressIncomingInterest (inFace, interest, pitEntry))
	{
		if(m_node->GetId() > 15)
			NS_LOG_WARN ("[FW]: Interest Aggregation! AT Node: " << m_node->GetId());
		pitEntry->AddIncoming (inFace/*, interest->GetInterestLifetime ()*/, reqreward);
		// update PIT entry lifetime
		pitEntry->UpdateLifetime (interest->GetInterestLifetime ());

		// Suppress this interest if we're still expecting data from some other face
		m_dropInterests (interest, inFace);
		return;
	}
	PropagateInterest (inFace, interest, pitEntry, reqreward);
}
/*
void ForwardingStrategy::UpdateDelay(Ptr<pit::Entry> pitentry)
{
	const ns3::ndn::pit::Entry::in_container pitfaces = pitentry->GetIncoming();
	Time firstcome = pitfaces.begin()->m_arrivalTime;
	for(auto iter = pitfaces.begin(); iter != pitfaces.end(); iter++)
	{
		if(iter->m_arrivalTime < firstcome)
			firstcome = iter->m_arrivalTime;
	}
	if(m_stat_summary != 0)
	{
		m_stat_summary->UpdateDelay(pitentry->GetPrefix(), Simulator::Now() - firstcome);
	}
}
*/
void ForwardingStrategy::RecordHopDelay(Ptr<Face> inFace, Ptr<const Data> data, Ptr<pit::Entry> pitEntry)
{
	if(m_stat_summary != 0 || m_tran != 0)
	{
		std::map<uint32_t, Time> records;
	/*
		Ptr<NetDeviceFace> netface = DynamicCast<ndn::NetDeviceFace> (inFace);
		if(netface == 0) // Consumer receives data packet; no need to record
			return ;
		Ptr<PointToPointNetDevice> p2pdevice = DynamicCast<ns3::PointToPointNetDevice>(netface->GetNetDevice());
		if(p2pdevice == 0)
		{// Something is wrong!
			NS_LOG_ERROR("in ForwardingStrategy::RecordHopDelay: Something Wrong");
			return ;
		}
		Ptr<NetDevice> onedevice = p2pdevice->GetChannel()->GetDevice(0);
		Ptr<Node> sourcenode;
		if(onedevice->GetNode()->GetId() == m_node->GetId())
		{
			sourcenode = p2pdevice->GetChannel()->GetDevice(1)->GetNode();
		}
		else
		{
			sourcenode = onedevice->GetNode();
		}
	*/
		const ns3::ndn::pit::Entry::in_container pitfaces = pitEntry->GetIncoming();
		for(auto iter = pitfaces.begin(); iter != pitfaces.end(); iter++)
		{

			if(records.find(iter->m_face->GetId()) != records.end())
			{
				if(records[iter->m_face->GetId()] < iter->m_arrivalTime)
					records[iter->m_face->GetId()] = iter->m_arrivalTime;
				//NS_LOG_DEBUG("Same faceID; different arrival time");
			}
			else
				records[iter->m_face->GetId()] = iter->m_arrivalTime;
		}
		Time firstcome = records.begin()->second;
		for(auto iter = records.begin(); iter!= records.end(); iter++)
		{
			if(iter->second < firstcome)
				firstcome = iter->second;
		}
		if(m_stat_summary != 0)
		{
			m_stat_summary->UpdateDelay(pitEntry->GetPrefix(), Simulator::Now() - firstcome);
		}
		if(m_tran != 0)
		{
			m_tran->UpdateDelay(pitEntry->GetPrefix(), data->GetSignature(), Simulator::Now() - firstcome);
		}
	}
}

void
ForwardingStrategy::OnData (Ptr<Face> inFace,
                            Ptr<Data> data)
{
  NS_LOG_FUNCTION (inFace << data->GetName ());
  //if(m_node->GetId() == 1)
  NS_LOG_INFO ("[FW] DATA for" << data->GetName() <<"\tFile: "
		  << static_cast<uint32_t>(data->GetName().get(-2).toNumber())
		  << " Chunk: " << static_cast<uint32_t>(data->GetName().get(-1).toNumber()));
  m_inData (data, inFace);

  // Lookup PIT entry
  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*data);
  if (pitEntry == 0)
    {
      bool cached = false;

      if (m_cacheUnsolicitedData || (m_cacheUnsolicitedDataFromApps && (inFace->GetFlags () & Face::APPLICATION)))
        {
          // Optimistically add or update entry in the content store
          cached = m_contentStore->Add (data);
        }
      else
        {
          // Drop data packet if PIT entry is not found
          // (unsolicited data packets should not "poison" content store)

          //drop dulicated or not requested data packet
          m_dropData (data, inFace);
        }

      DidReceiveUnsolicitedData (inFace, data, cached);
      return;
    }
  else
    {
	  data->AddTSB();
      m_contentStore->Add (data);
      //NS_LOG_DEBUG("[FW] on NodeID:" << this->m_node->GetId() <<" Data: "<<data->GetName().toUri() << "Add to Cache: "
    	//	  << (cached ? "Success" : "Failure"));
      data->DecAcc(m_contentStore->GetCapacity());

      /*Added by Wenjie : 12/15/2015 measure the link utilization
      *	RecordHopDelay(inFace, data, pitEntry);
      *
      */
      if(m_node->GetObject<VideoCacheDecision>() != 0)
      {
    	  //NS_LOG_WARN("Enable RecordHopDelay for StreamCache!!!");
    	  RecordHopDelay(inFace, data, pitEntry);
      }

      /*
       * Added by Wenjie : 07/13/2016 Cache Multisection
       */
      if(data->GetTimestamp() == Time(0))
    	  data->SetTimestamp(Simulator::Now());
      else
      {
          data->AddOneHop();
          data->m_mostRecentDelay = Simulator::Now() - data->GetTimestamp();
          //NS_LOG_DEBUG("[FW] on NodeID:" << this->m_node->GetId() <<" Data: "<<data->GetName().toUri() << "; Delay from last hop: "
            //  		  << data->m_mostRecentDelay);
          data->SetTimestamp(Simulator::Now());
      }
    }

  while (pitEntry != 0)
  {
	  // Do data plane performance measurements
      //WillSatisfyPendingInterest (inFace, pitEntry);

      // Actually satisfy pending interest
      SatisfyPendingInterest (inFace, data, pitEntry);

      // Lookup another PIT entry
      pitEntry = m_pit->Lookup (*data);
  }
}

void
ForwardingStrategy::DidCreatePitEntry (Ptr<Face> inFace,
                                       Ptr<const Interest> interest,
                                       Ptr<pit::Entry> pitEntrypitEntry)
{
}

void
ForwardingStrategy::FailedToCreatePitEntry (Ptr<Face> inFace,
                                            Ptr<const Interest> interest)
{
  m_dropInterests (interest, inFace);
}

void
ForwardingStrategy::DidReceiveDuplicateInterest (Ptr<Face> inFace,
                                                 Ptr<const Interest> interest,
                                                 Ptr<pit::Entry> pitEntry)
{
  /////////////////////////////////////////////////////////////////////////////////////////
  //                                                                                     //
  // !!!! IMPORTANT CHANGE !!!! Duplicate interests will create incoming face entry !!!! //
  //                                                                                     //
  /////////////////////////////////////////////////////////////////////////////////////////
  pitEntry->AddIncoming (inFace);
  m_dropInterests (interest, inFace);
}

void
ForwardingStrategy::DidSuppressSimilarInterest (Ptr<Face> face,
                                                Ptr<const Interest> interest,
                                                Ptr<pit::Entry> pitEntry)
{
}

void
ForwardingStrategy::DidForwardSimilarInterest (Ptr<Face> inFace,
                                               Ptr<const Interest> interest,
                                               Ptr<pit::Entry> pitEntry)
{
}

void
ForwardingStrategy::DidExhaustForwardingOptions (Ptr<Face> inFace,
                                                 Ptr<const Interest> interest,
                                                 Ptr<pit::Entry> pitEntry)
{
  NS_LOG_FUNCTION (this << boost::cref (*inFace));
  if (pitEntry->AreAllOutgoingInVain ())
    {
      m_dropInterests (interest, inFace);

      // All incoming interests cannot be satisfied. Remove them
      pitEntry->ClearIncoming ();

      // Remove also outgoing
      pitEntry->ClearOutgoing ();

      // Set pruning timout on PIT entry (instead of deleting the record)
      m_pit->MarkErased (pitEntry);
    }
}



bool
ForwardingStrategy::DetectRetransmittedInterest (Ptr<Face> inFace,
                                                 Ptr<const Interest> interest,
                                                 Ptr<pit::Entry> pitEntry)
{
  pit::Entry::in_iterator existingInFace = pitEntry->GetIncoming ().find (inFace);

  bool isRetransmitted = false;

  if (existingInFace != pitEntry->GetIncoming ().end ())
    {
      // this is almost definitely a retransmission. But should we trust the user on that?
      isRetransmitted = true;
    }

  return isRetransmitted;
}

void
ForwardingStrategy::SatisfyPendingInterest( Ptr<Face> inFace,
                                            Ptr<const Data> data,
                                            Ptr<pit::Entry> pitEntry)
{
	if (inFace != 0)
		pitEntry->RemoveIncoming (inFace);

	//satisfy all pending incoming Interests
	BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
    {

		bool ok = incoming.m_face->SendData (data);

		DidSendOutData (inFace, incoming.m_face, data, pitEntry);

		if (!ok)
    	  m_dropData (data, incoming.m_face);
    }

	// All incoming interests are satisfied. Remove them
	pitEntry->ClearIncoming ();

	// Remove all outgoing faces
	pitEntry->ClearOutgoing ();

	// Set pruning timout on PIT entry (instead of deleting the record)
	m_pit->MarkErased (pitEntry);
}

void
ForwardingStrategy::DelayedSatisfyPendingInterest(Ptr<const Data> data,
	  	  	  	 	 	 	 	 	 	 	 	  Ptr<Interest> interest)
{

	Ptr<pit::Entry> pitEntry = m_pit->Lookup (*interest);//Only compare the Name
	if(pitEntry == 0)
	{
		return;
	}

	//satisfy all pending incoming Interests
	BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
    {
		incoming.m_face->SendData (data);
    }

	// All incoming interests are satisfied. Remove them
	pitEntry->ClearIncoming ();

	// Remove all outgoing faces
	pitEntry->ClearOutgoing ();

	// Set pruning timout on PIT entry (instead of deleting the record)
	m_pit->MarkErased (pitEntry);
}

void
ForwardingStrategy::DidReceiveSolicitedData (Ptr<Face> inFace,
                                             Ptr<const Data> data,
                                             bool didCreateCacheEntry)
{
  // do nothing
}

void
ForwardingStrategy::DidReceiveUnsolicitedData (Ptr<Face> inFace,
                                               Ptr<const Data> data,
                                               bool didCreateCacheEntry)
{
  // do nothing
}

void
ForwardingStrategy::WillSatisfyPendingInterest (Ptr<Face> inFace,
                                                Ptr<pit::Entry> pitEntry)
{
  pit::Entry::out_iterator out = pitEntry->GetOutgoing ().find (inFace);

  // If we have sent interest for this data via this face, then update stats.
  if (out != pitEntry->GetOutgoing ().end ())
    {
      pitEntry->GetFibEntry ()->UpdateFaceRtt (inFace, Simulator::Now () - out->m_sendTime);
    }

  m_satisfiedInterests (pitEntry);
}

bool
ForwardingStrategy::ShouldSuppressIncomingInterest (Ptr<Face> inFace,
                                                    Ptr<const Interest> interest,
                                                    Ptr<pit::Entry> pitEntry)
{
  bool isNew = pitEntry->GetIncoming ().size () == 0 && pitEntry->GetOutgoing ().size () == 0;

  if (isNew) return false; // never suppress new interests

  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (inFace, interest, pitEntry);

  return !isRetransmitted;
}

void
ForwardingStrategy::PropagateInterest (Ptr<Face> inFace,
                                       Ptr<const Interest> interest,
                                       Ptr<pit::Entry> pitEntry,
									   double reward)
{
  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (inFace, interest, pitEntry);

  pitEntry->AddIncoming (inFace/*, interest->GetInterestLifetime ()*/, reward);
  /// @todo Make lifetime per incoming interface
  pitEntry->UpdateLifetime (interest->GetInterestLifetime ());

  bool propagated = DoPropagateInterest (inFace, interest, pitEntry);

  if (!propagated && isRetransmitted) //give another chance if retransmitted
    {
      // increase max number of allowed retransmissions
      pitEntry->IncreaseAllowedRetxCount ();

      // try again
      propagated = DoPropagateInterest (inFace, interest, pitEntry);
    }

  // if (!propagated)
  //   {
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //     NS_LOG_DEBUG ("+++ Not propagated ["<< interest->GetName () <<"], but number of outgoing faces: " << pitEntry->GetOutgoing ().size ());
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //   }

  // ForwardingStrategy will try its best to forward packet to at least one interface.
  // If no interests was propagated, then there is not other option for forwarding or
  // ForwardingStrategy failed to find it.
  if (!propagated && pitEntry->AreAllOutgoingInVain ())
    {
      DidExhaustForwardingOptions (inFace, interest, pitEntry);
    }
}

bool
ForwardingStrategy::CanSendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> interest,
                                        Ptr<pit::Entry> pitEntry)
{
  if (outFace == inFace)
    {
      // NS_LOG_DEBUG ("Same as incoming");
      return false; // same face as incoming, don't forward
    }

  pit::Entry::out_iterator outgoing =
    pitEntry->GetOutgoing ().find (outFace);

  if (outgoing != pitEntry->GetOutgoing ().end ())
    {
      if (!m_detectRetransmissions)
        return false; // suppress
      else if (outgoing->m_retxCount >= pitEntry->GetMaxRetxCount ())
        {
          // NS_LOG_DEBUG ("Already forwarded before during this retransmission cycle (" <<outgoing->m_retxCount << " >= " << pitEntry->GetMaxRetxCount () << ")");
          return false; // already forwarded before during this retransmission cycle
        }
   }

  return true;
}


bool
ForwardingStrategy::TrySendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> interest,
                                        Ptr<pit::Entry> pitEntry)
{
  if (!CanSendOutInterest (inFace, outFace, interest, pitEntry))
    {
      return false;
    }

  pitEntry->AddOutgoing (outFace);

  //transmission
  bool successSend = outFace->SendInterest (interest);
  if (!successSend)
    {
      m_dropInterests (interest, outFace);
    }

  DidSendOutInterest (inFace, outFace, interest, pitEntry);

  return true;
}

void
ForwardingStrategy::DidSendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> interest,
                                        Ptr<pit::Entry> pitEntry)
{
  m_outInterests (interest, outFace);
}

void
ForwardingStrategy::DidSendOutData (Ptr<Face> inFace,
                                    Ptr<Face> outFace,
                                    Ptr<const Data> data,
                                    Ptr<pit::Entry> pitEntry)
{
  m_outData (data, inFace == 0, outFace);
}

void
ForwardingStrategy::WillEraseTimedOutPendingInterest (Ptr<pit::Entry> pitEntry)
{
  m_timedOutInterests (pitEntry);
}

void
ForwardingStrategy::AddFace (Ptr<Face> face)
{
  // do nothing here
}

void
ForwardingStrategy::RemoveFace (Ptr<Face> face)
{
  // do nothing here
}

void
ForwardingStrategy::DidAddFibEntry (Ptr<fib::Entry> fibEntry)
{
  // do nothing here
}

void
ForwardingStrategy::WillRemoveFibEntry (Ptr<fib::Entry> fibEntry)
{
  // do nothing here
}


} // namespace ndn
} // namespace ns3
