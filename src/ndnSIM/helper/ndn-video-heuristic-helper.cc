#include "ndn-video-heuristic-helper.h"
#include "ns3/ndn-name.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/node-list.h"
#include "ns3/point-to-point-net-device.h"
#include "../model/ndn-net-device-face.h"
#include "ns3/channel.h"
#include "ns3/channel-list.h"
#include "ns3/simulator.h"
#include "ns3/ndn-app.h"

#include "ns3/ndn-content-store.h"
#include "ns3/ndn-fib.h"
#include "ns3/ndn-popularity.h"
#include "ns3/ndn-videocache.h"

#include <vector>
#include <map>

NS_LOG_COMPONENT_DEFINE ("ndn.VideoHeuristicHelper");

namespace ns3{
namespace ndn{

DASHeuristicHelper::DASHeuristicHelper(const std::string& method,
									   const std::string& prefix,
									   const NodeContainer& edgenodes,
									   const NodeContainer& intmnodes,
									   const std::string& roundtime,
									   uint32_t iterationlimit)
	:m_edgenodes(edgenodes),
	 m_intmnodes(intmnodes),
	 m_method(method),
	 m_roundtime(roundtime)
{
	m_prefix = Create<Name>(prefix);
	m_limit = iterationlimit;
}

DASHeuristicHelper::~DASHeuristicHelper()
{
	Simulator::Cancel (m_PeriodicEvent);
}
void DASHeuristicHelper::Preprocessing()
{
	for(NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
	{
		Ptr<L3Protocol> ndn = (*node)->GetObject<L3Protocol>();
		Ptr<VideoCacheDecision> decisionPtr = ndn->GetObject<VideoCacheDecision>();
		if(decisionPtr != 0)
			decisionPtr->Clear();
	}
}

void DASHeuristicHelper::Postprocessing()
{
	for(NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
	{
		Ptr<L3Protocol> ndn = (*node)->GetObject<L3Protocol>();
		Ptr<PopularitySummary> sumPtr = ndn->GetObject<PopularitySummary>();
		if(sumPtr != 0)
			sumPtr->Clear();
	}
}
void DASHeuristicHelper::RunHeuristic()
{
	Preprocessing();
	/* For all edge routers, collecting statistics and summarizing */
	for(NodeContainer::Iterator node = m_edgenodes.Begin(); node != m_edgenodes.End(); node++)
	{
		Ptr<L3Protocol> ndn = (*node)->GetObject<L3Protocol>();
		Ptr<PopularitySummary> sumPtr = ndn->GetObject<PopularitySummary>();
		sumPtr->Summarize();
		sumPtr->CalculateUtility();

		Ptr<VideoCacheDecision> decisionPtr = ndn->GetObject<VideoCacheDecision>();
		decisionPtr->GreedyChoice();

	}

	for(uint32_t DecisionNodes = 0; DecisionNodes < m_intmnodes.size(); DecisionNodes++)
	{
		for(NodeContainer::Iterator node =  m_intmnodes.Begin(); node != m_intmnodes.End(); node++)
		{
			Ptr<L3Protocol> ndn = (*node)->GetObject<L3Protocol>();
			if(CheckDownStreamStatus(*node, ndn))
			{
				NS_LOG_DEBUG("[Heuristic]: Start Working on NodeID:" << (*node)->GetId());
				AggregateInfo(*node, ndn);

				Ptr<PopularitySummary> sumPtr = ndn->GetObject<PopularitySummary>();
				sumPtr->SetMarkStatus();
				sumPtr->CalculateUtility();

				Ptr<VideoCacheDecision> decisionPtr = ndn->GetObject<VideoCacheDecision>();
				decisionPtr->GreedyChoice();

				break;
			}
		}
	}
	Postprocessing();
	Run();
}

bool DASHeuristicHelper::CheckDownStreamStatus(Ptr<Node> node, Ptr<L3Protocol> ndn)
{
	Ptr<PopularitySummary> sumPtr = ndn->GetObject<PopularitySummary>();
	if(sumPtr == 0 || sumPtr->CheckMarkStatus())
		return false;

	bool status = true;

	for (NodeList::Iterator nodeptr = NodeList::Begin (); nodeptr != NodeList::End (); nodeptr++)
	{
		Ptr<fib::Entry> fibentry = (*nodeptr)->GetObject<Fib>()->Find(*m_prefix);
		if(fibentry != 0)
		{
			fib::FaceMetricContainer::type::index<fib::i_metric>::type::iterator entry = fibentry->GetFaces().get<fib::i_metric>().begin();
			Ptr<Face> fibentry_face = entry->GetFace();

			Ptr<NetDeviceFace> fibentry_netface = DynamicCast<ndn::NetDeviceFace> (fibentry_face);
			if(fibentry_netface != 0)
			{
				Ptr<PointToPointNetDevice> p2pdevice = DynamicCast<ns3::PointToPointNetDevice>(fibentry_netface->GetNetDevice());
				Ptr<NetDevice> nextNode_device = p2pdevice->GetChannel()->GetDevice(0);
				if(nextNode_device->GetNode()->GetId() == (*nodeptr)->GetId())
					nextNode_device = p2pdevice->GetChannel()->GetDevice(1);

				Ptr<Node> parentnode = nextNode_device->GetNode();
				if(parentnode == node && !(*nodeptr)->GetObject<PopularitySummary>()->CheckMarkStatus())
				{
					status = false;
					break;
				}
			}
		}
	}
	return status;
}

void DASHeuristicHelper::AggregateInfo(Ptr<Node> node, Ptr<L3Protocol> ndn)
{
	std::vector<Ptr<Node> > AggNodes;
	for (NodeList::Iterator nodeptr = NodeList::Begin (); nodeptr != NodeList::End (); nodeptr++)
	{
		Ptr<fib::Entry> fibentry = (*nodeptr)->GetObject<Fib>()->Find(*m_prefix);
		if(fibentry != 0)
		{
			fib::FaceMetricContainer::type::index<fib::i_metric>::type::iterator entry = fibentry->GetFaces().get<fib::i_metric>().begin();
			Ptr<Face> fibentry_face = entry->GetFace();

			Ptr<NetDeviceFace> fibentry_netface = DynamicCast<ndn::NetDeviceFace> (fibentry_face);
			if(fibentry_netface != 0)
			{
				Ptr<PointToPointNetDevice> p2pdevice = DynamicCast<ns3::PointToPointNetDevice>(fibentry_netface->GetNetDevice());
				Ptr<NetDevice> nextNode_device = p2pdevice->GetChannel()->GetDevice(0);
				if(nextNode_device->GetNode()->GetId() == (*nodeptr)->GetId())
					nextNode_device = p2pdevice->GetChannel()->GetDevice(1);

				Ptr<Node> parentnode = nextNode_device->GetNode();
				if(parentnode == node)
				{
					AggNodes.push_back(*nodeptr);
				}
			}
		}
	}

	for(auto iter = AggNodes.begin(); iter != AggNodes.end(); iter++)
	{
		NS_LOG_DEBUG("Node" << node->GetId() << "Aggregates Node:" << (*iter)->GetId());
		ndn->GetObject<VideoCacheDecision>()->AggregateDecision(*((*iter)->GetObject<VideoCacheDecision>()));
		ndn->GetObject<PopularitySummary>()->Aggregate(*((*iter)->GetObject<PopularitySummary>()));
	}
}

void DASHeuristicHelper::Run()
{
	if(m_method == "DASCache" || m_method == "StreamCache")
	{
		for (NodeList::Iterator nodeptr = NodeList::Begin (); nodeptr != NodeList::End (); nodeptr++)
		{
			Ptr<ContentStore> cs = (*nodeptr)->GetObject<ContentStore>();
			if(cs != 0)
				cs->DisableCSUpdate();
			if((*nodeptr)->GetNApplications() > 0)
			{
				Ptr<Application> BaseAppPtr = (*nodeptr)->GetApplication(0);
				BaseAppPtr->IncreaseTransition();
			}
		}
		m_times++;
		if(m_times <= m_limit)
			m_PeriodicEvent = Simulator::Schedule (Time(m_roundtime), &DASHeuristicHelper::RunHeuristic, this);
		else
		{
			Simulator::Stop(Time(m_roundtime));
			for (NodeList::Iterator nodeptr = NodeList::Begin (); nodeptr != NodeList::End (); nodeptr++)
			{
				if((*nodeptr)->GetNApplications() > 0)
				{
					Ptr<Application> BaseAppPtr = (*nodeptr)->GetApplication(0);
					BaseAppPtr->EnableRecord();
				}
			}
		}
	}
}

void DASHeuristicHelper::PrintCache()
{
	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		Ptr<ContentStore> cs = (*iter)->GetObject<ContentStore>();
		std::vector<ns3::ndn::Name> initcs;
		cs->FillinCacheRun(initcs);
		std::map<std::string, uint32_t> count;

		for(uint32_t i = 0; i < initcs.size(); i++)
		{
			std::string brstr = (initcs[i].get(-3).toUri()).substr(2);
			if(count.find(brstr) == count.end())
				count.insert(std::make_pair(brstr, 0));
			count[brstr]++;
		}

		std::string tail = "kbps";
		for(auto briter = count.begin(); briter != count.end(); briter++)
		{
			std::string videobr = briter->first;
			std::size_t found = videobr.find(tail);
		}
	}
}

}
}
