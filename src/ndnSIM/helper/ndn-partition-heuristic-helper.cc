#include "ndn-partition-heuristic-helper.h"
#include "ndn-partition-helper.h"
#include "ns3/net-device.h"
#include "ns3/node-list.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/channel.h"
#include "ns3/ndn-app.h"
#include "ns3/simulator.h"
#include "ns3/callback.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/ndn-content-store.h"

#include "ns3/ndn-bitrate.h"
#include "ns3/ndn-popularity.h"
#include "ns3/ndn-videostat.h"

#include "ns3/ndn-fib.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

NS_LOG_COMPONENT_DEFINE ("ndn.CachePartition.Heuristic");

namespace ns3{
namespace ndn{

HeuPartitionHelper::HeuPartitionHelper(const std::string& prefix,
		   	   	   	   	   	   	   	   const std::string& roundtime,
									   double rewardparam,
		   	   	   	   	   	   	   	   uint32_t B,
									   uint32_t iterationTimes,
									   uint32_t mode,
									   sql::Driver* driver,
									   const std::string& DatabaseName,
									   const std::string& user,
									   const std::string& password,
									   const std::string& ip,
									   int port,
									   uint32_t expid)
{
	m_prefix = prefix;
	m_roundtime = roundtime;
	NumB = B;
	m_expid = expid;

	m_iterationTimes = iterationTimes;
	m_itercounter = 0;

	m_alpha = rewardparam;
	m_design = mode;

	try{
		sql::ConnectOptionsMap properties;
		properties["hostName"] = "tcp://" + ip;
		properties["userName"] = user;
		properties["password"] = password;
		properties["port"] = port;
		properties["schema"] = DatabaseName;
		properties["OPT_RECONNECT"] = true;

		m_con = driver->connect(properties);
		m_stmt = m_con->createStatement();

		m_stmt->execute("CREATE TABLE IF NOT EXISTS CacheStatus (ExpID SMALLINT, Transition SMALLINT, NodeID SMALLINT, `BitRate(Kbps)` VARCHAR(4), Size INT);");

	}
	catch (sql::SQLException &e) {
		std::cout << "# ERR: SQLException in " << __FILE__;
		std::cout << "(" << __FUNCTION__ << ") on line "
		     << __LINE__ << std::endl;
		std::cout << "# ERR: " << e.what();
		std::cout << " (MySQL error code: " << e.getErrorCode();
		std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
	}
}

HeuPartitionHelper::~HeuPartitionHelper()
{
	delete m_stmt;
	m_stmt = nullptr;
	delete m_con;
	m_con = nullptr;
}

void
HeuPartitionHelper::SetTopologicalOrder(Ptr<Node> ServerNode)
{
	m_producer = ServerNode;
	std::deque<Ptr<Node> > execStack;
	execStack.push_back(ServerNode);

	m_topo_order_ptr.push_back(ServerNode);
	BreadthFirstSearch(execStack);
	m_topo_order_ptr.pop_front(); //Exclude Producer Node

	std::deque<Ptr<Node> > depStack;
	depStack.push_back(ServerNode);
	uint32_t dnum = ServerNode->GetNDevices();

	for(uint32_t i = 0; i < dnum; i++)
	{
		Ptr<ns3::PointToPointNetDevice> p2pdevice
					= DynamicCast<ns3::PointToPointNetDevice>(ServerNode->GetDevice(i));
		Ptr<NetDevice> probedevice = p2pdevice->GetChannel()->GetDevice(1);
		if(probedevice->GetNode()->GetId() == ServerNode->GetId())
			probedevice = p2pdevice->GetChannel()->GetDevice(0);

		DepthFirstSearch(probedevice->GetNode(), depStack);
	}
	//depStack.clear();
	//m_UpstreamSet[ServerNode->GetId()] = depStack;

	// FOR DEBUG ONLY:
/*
	std::cout << "Up Dependency\n";
	for(auto iter = m_UpstreamSet.begin(); iter != m_UpstreamSet.end(); iter++)
	{
		std::cout << "Node" << iter->first << "\t";
		for(auto nodeiter = iter->second.begin(); nodeiter != iter->second.end(); nodeiter++)
		{
			Ptr<Node> nodeptr = *nodeiter;
			std::cout << nodeptr->GetId() << "  ";
		}
		std::cout << std::endl;
	}
	std::cout << "Down Dependency\n";
	for(auto iter = m_DownstreamSet.begin(); iter != m_DownstreamSet.end(); iter++)
	{
		std::cout << "Node" << iter->first << "\t";
		for(auto nodeiter = iter->second.begin(); nodeiter != iter->second.end(); nodeiter++)
		{
			Ptr<Node> nodeptr = *nodeiter;
			std::cout << nodeptr->GetId() << "  ";
		}
		std::cout << std::endl;
	}

	std::cout << "Edge Nodes\t";
	for(auto nodeiter = m_edgenode_ptr.begin(); nodeiter != m_edgenode_ptr.end(); nodeiter++)
	{
		Ptr<Node> nodeptr = *nodeiter;
		std::cout << nodeptr->GetId() << "  ";
	}
	std::cout << std::endl;

	std::cout << "Topological Order\t";
	for(auto nodeiter = m_topo_order_ptr.begin(); nodeiter != m_topo_order_ptr.end(); nodeiter++)
	{
		Ptr<Node> nodeptr = *nodeiter;
		std::cout << nodeptr->GetId() << "  ";
	}
	std::cout << std::endl;
*/
}

void
HeuPartitionHelper::BreadthFirstSearch(std::deque<Ptr<Node> >& execStack)
{
	//Breadth-first Search;
	//Construct m_topo_order_ptr
	while(execStack.size() > 0)
	{
		Ptr<Node> execNode = execStack.front();
		execStack.pop_front();

		uint32_t num = execNode->GetNDevices();
		for(uint32_t i = 0 ; i < num; i++)
		{
			Ptr<ns3::PointToPointNetDevice> p2pdevice = DynamicCast<ns3::PointToPointNetDevice>(execNode->GetDevice(i));
			Ptr<NetDevice> probedevice = p2pdevice->GetChannel()->GetDevice(1);
			if(probedevice->GetNode()->GetId() == execNode->GetId())
				probedevice = p2pdevice->GetChannel()->GetDevice(0);
			bool newnode = true;
			for(auto nodeiter = m_topo_order_ptr.begin();
					nodeiter != m_topo_order_ptr.end();
					nodeiter++)
			{
				if((*nodeiter)->GetId() == probedevice->GetNode()->GetId())
				{
					newnode = false;
					break;
				}
			}
			if(newnode)
			{
				execStack.push_back(probedevice->GetNode());
				m_topo_order_ptr.push_back(probedevice->GetNode());
			}
		}
	}
}

void
HeuPartitionHelper::DepthFirstSearch(Ptr<Node> searchpoint, std::deque<Ptr<Node> >& execStack)
{
	//Depth-first Search
	//Construct the Dependency Set
	std::deque<Ptr<Node> > modexecStack = execStack;
	modexecStack.pop_front();
	m_UpstreamSet[searchpoint->GetId()] = modexecStack;

	for(auto nodeiter = execStack.begin(); nodeiter != execStack.end(); nodeiter++)
	{
		Ptr<Node> nodeptr = *nodeiter;
		if(nodeptr != m_producer)
		{
			auto downitr = m_DownstreamSet.find(nodeptr->GetId());
			if(downitr != m_DownstreamSet.end())
				downitr->second.push_back(searchpoint);
			else
			{
				std::deque<Ptr<Node> > newdownnodes;
				newdownnodes.push_back(searchpoint);
				m_DownstreamSet.insert(std::make_pair(nodeptr->GetId(), std::move(newdownnodes)));
			}
		}
	}

	uint32_t num = searchpoint->GetNDevices();
	Ptr<Node> lastnode = 0;
	if(execStack.size() > 0)
		lastnode = execStack.back();
	execStack.push_back(searchpoint);

	for(uint32_t i = 0; i < num; i++)
	{
		Ptr<ns3::PointToPointNetDevice> p2pdevice
					= DynamicCast<ns3::PointToPointNetDevice>(searchpoint->GetDevice(i));
		Ptr<NetDevice> probedevice = p2pdevice->GetChannel()->GetDevice(1);
		if(probedevice->GetNode()->GetId() == searchpoint->GetId())
			probedevice = p2pdevice->GetChannel()->GetDevice(0);

		if(lastnode != 0 && lastnode->GetId() == probedevice->GetNode()->GetId())
			continue;

		DepthFirstSearch(probedevice->GetNode(), execStack);
	}
	if(num == 1 && lastnode != 0)
		m_edgenode_ptr.push_back(searchpoint);

	execStack.pop_back();
}

void
HeuPartitionHelper::SetTopologicalOrder(NodeContainer EdgeNodes)
{
	Ptr<Name> origin = Create<Name> (boost::lexical_cast<Name> (m_prefix));
	for(NodeContainer::Iterator node = EdgeNodes.Begin(); node != EdgeNodes.End(); node++)
	{
		m_edgenode_ptr.push_back(*node);
		Ptr<Node> workingnode = *node;
		std::list<Ptr<Node> > passnodes;
		passnodes.push_back(workingnode);
		bool ontrack;

		do
		{
			ontrack = false;
			Ptr<fib::Entry> fibentry = workingnode->GetObject<Fib>()->Find(*origin);
			fib::FaceMetricContainer::type::index<fib::i_metric>::type::iterator entry
													= fibentry->GetFaces().get<fib::i_metric>().begin();
			Ptr<Face> fibentry_face = entry->GetFace();
			Ptr<NetDeviceFace> fibentry_netface = DynamicCast<ndn::NetDeviceFace> (fibentry_face);
			if(fibentry_netface != 0)
			{
				Ptr<PointToPointNetDevice> p2pdevice = DynamicCast<ns3::PointToPointNetDevice>(fibentry_netface->GetNetDevice());
				Ptr<NetDevice> nextNode_device = p2pdevice->GetChannel()->GetDevice(0);
				Ptr<Node> nextNode = nextNode_device->GetNode();
				if(nextNode->GetId() == workingnode->GetId())
				{
					nextNode_device = p2pdevice->GetChannel()->GetDevice(1);
					nextNode = nextNode_device->GetNode();
				}
				workingnode = nextNode;
				if(workingnode->GetNApplications() == 0)
					ontrack = true;
				for(auto iter = passnodes.begin(); iter != passnodes.end(); iter++)
				{
					auto upiter = m_UpstreamSet.find((*iter)->GetId());
					if(upiter == m_UpstreamSet.end())
					{
						std::deque<Ptr<Node> > upstreams;
						m_UpstreamSet.insert(std::make_pair((*iter)->GetId(), std::move(upstreams)));
						upiter = m_UpstreamSet.find((*iter)->GetId());
					}
					if (ontrack)
					{
						bool noappear = true;
						for(auto item = upiter->second.begin(); item != upiter->second.end(); item++)
						{
							if((*item)->GetId() == workingnode->GetId())
								noappear = false;
						}
						if(noappear)
							upiter->second.push_front(workingnode);
					}
				}

				if(ontrack)
				{
					auto downiter = m_DownstreamSet.find(workingnode->GetId());
					if(downiter == m_DownstreamSet.end())
					{
						std::deque<Ptr<Node> > downstreams;
						m_DownstreamSet.insert(std::make_pair(workingnode->GetId(), std::move(downstreams)));
						downiter = m_DownstreamSet.find(workingnode->GetId());
					}
					for(auto iter = passnodes.begin(); iter != passnodes.end(); iter++)
					{
						bool noappear = true;
						for(auto item = downiter->second.begin(); item != downiter->second.end(); item++)
						{
							if((*item)->GetId() == (*iter)->GetId())
								noappear = false;
						}
						if(noappear)
							downiter->second.push_front(*iter);
					}
				}
				passnodes.push_back(workingnode);

			}
		}while(ontrack);
	}

	uint32_t idx = 0;
	bool addtotopo = true;

	while(addtotopo)
	{
		addtotopo = false;
		for(auto iter = m_UpstreamSet.begin(); iter != m_UpstreamSet.end(); iter++)
		{
			if(iter->second.size() <= idx)
				continue;

			addtotopo = true;
			std::deque<Ptr<Node> >::iterator vptr = iter->second.begin() + idx;

			bool noappear = true;
			for(auto item = m_topo_order_ptr.begin(); item != m_topo_order_ptr.end(); item++)
			{
				if((*item)->GetId() == (*vptr)->GetId())
					noappear = false;
			}
			if(noappear)
				m_topo_order_ptr.push_back(*vptr);

		}
		idx++;
	}
	for(auto nodeiter = m_edgenode_ptr.begin(); nodeiter != m_edgenode_ptr.end(); nodeiter++)
	{
		Ptr<Node> nodeptr = *nodeiter;
		m_topo_order_ptr.push_back(nodeptr);
	}


/*
	std::cout << "Up Dependency\n";
	for(auto iter = m_UpstreamSet.begin(); iter != m_UpstreamSet.end(); iter++)
	{
		std::cout << "Node" << iter->first << "\t";
		for(auto nodeiter = iter->second.begin(); nodeiter != iter->second.end(); nodeiter++)
		{
			Ptr<Node> nodeptr = *nodeiter;
			std::cout << nodeptr->GetId() << "  ";
		}
		std::cout << std::endl;
	}
	std::cout << "Down Dependency\n";
	for(auto iter = m_DownstreamSet.begin(); iter != m_DownstreamSet.end(); iter++)
	{
		std::cout << "Node" << iter->first << "\t";
		for(auto nodeiter = iter->second.begin(); nodeiter != iter->second.end(); nodeiter++)
		{
			Ptr<Node> nodeptr = *nodeiter;
			std::cout << nodeptr->GetId() << "  ";
		}
		std::cout << std::endl;
	}

	std::cout << "Edge Nodes\t";
	for(auto nodeiter = m_edgenode_ptr.begin(); nodeiter != m_edgenode_ptr.end(); nodeiter++)
	{
		Ptr<Node> nodeptr = *nodeiter;
		std::cout << nodeptr->GetId() << "  ";
	}
	std::cout << std::endl;

	std::cout << "Topological Order\t";
	for(auto nodeiter = m_topo_order_ptr.begin(); nodeiter != m_topo_order_ptr.end(); nodeiter++)
	{
		Ptr<Node> nodeptr = *nodeiter;
		std::cout << nodeptr->GetId() << "  ";
	}
	std::cout << std::endl;
*/

}

void
HeuPartitionHelper::OutputCacheStatus(uint32_t count)
{
	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		Ptr<ContentStore> cs = (*iter)->GetObject<ContentStore>();
		Ptr<NDNBitRate> BRinfo = (*iter)->GetObject<NDNBitRate>();
		if(cs != 0)
		{
			std::vector<ns3::ndn::Name> cscontent;
			std::map<std::string, uint32_t> cssize;
			std::map<std::string, double> csratio;
			double total = 0;
			cs->FillinCacheRun(cscontent);

			for(uint32_t i = 0; i < cscontent.size(); i++)
			{
				std::string br = cscontent[i].get(-3).toUri();
				uint32_t chunksize = static_cast<uint32_t>(BRinfo->GetChunkSize(br.substr(2)));
				total += chunksize;
				auto briter = cssize.find(br.substr(2));
				if(briter != cssize.end())
				{
					briter->second += chunksize;
				}
				else
				{
					cssize.insert(std::make_pair(br.substr(2), chunksize));
				}

			}

			std::ostringstream ss;
			std::string tail = "kbps";
			try{
				for(auto entry = cssize.begin(); entry != cssize.end(); entry++)
				{
					std::size_t found = entry->first.find(tail);

					ss << "INSERT INTO CacheStatus VALUES(" << m_expid << ","
															<< count << ","
															<< (*iter)->GetId() << ","
															<< entry->first.substr(0, found) << ","
															<< entry->second << ");";
					m_stmt->execute(ss.str());
					ss.str("");
				}
			}
			catch (sql::SQLException &e) {
				std::cout << "# ERR: SQLException in " << __FILE__;
				std::cout << "(" << __FUNCTION__ << ") on line "
				     << __LINE__ << std::endl;
				std::cout << "# ERR: " << e.what();
				std::cout << " (MySQL error code: " << e.getErrorCode();
				std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
			}

			//safety check
			if(cs->GetCapacity() < total * 1e3)
				std::cout << "DANGER: Execution ERROR!!!!!!!\n";
/*
			for(auto entry = cssize.begin(); entry != cssize.end(); entry++)
				csratio.insert(std::make_pair(entry->first, entry->second/total * 1e2));
			m_partition.insert(std::make_pair((*iter)->GetId(), std::move(csratio)));
*/
		}
	}
}

void
HeuPartitionHelper::Initialization()
{
	m_cachestack.clear();
	m_availcapacity.clear();
	m_decision.clear();
	m_delaytable.clear();

	for(auto nodeiter = m_edgenode_ptr.begin();
			 nodeiter != m_edgenode_ptr.end();
			 nodeiter++)
	{
		Ptr<Node> nodeptr = *nodeiter;
		Ptr<ContentStore> csptr = nodeptr->GetObject<ContentStore>();

		std::vector<uint32_t> capacity;
		capacity.push_back(csptr->GetCapacity());

		auto upnode = m_UpstreamSet.find(nodeptr->GetId());
		if(upnode != m_UpstreamSet.end() && upnode->second.size() > 0)
		{
			for(auto upiter = upnode->second.rbegin();
					 upiter != upnode->second.rend();
					 upiter++)
			{
				Ptr<ContentStore> upcsptr = (*upiter)->GetObject<ContentStore>();
				//Initialize with entire cache capacity on each routing path
				capacity.push_back(upcsptr->GetCapacity());
			}
		}
		m_availcapacity.insert(std::make_pair(nodeptr->GetId(), std::move(capacity)));
	}
}


void
HeuPartitionHelper::IterativeRun()
{
	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		Ptr<ContentStore> cs = (*iter)->GetObject<ContentStore>();
		if(cs != 0)
			cs->DisableCSUpdate();

		if((*iter)->GetNApplications() > 0)
		{
			Ptr<Application> BaseAppPtr = (*iter)->GetApplication(0);
			if(m_itercounter > 4)
				BaseAppPtr->EnableRecord();
			BaseAppPtr->IncreaseTransition();
		}
	}
	Solver();
}

/*
bool
HeuPartitionHelper::CheckIterationCondition()
{
	bool goon = false;
	if(m_lastpartition.size() == 0)
		goon = true;
	else
	{
		for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
		{
			auto conditer = m_partition.find((*iter)->GetId());
			auto lastiter = m_lastpartition.find((*iter)->GetId());
			if(conditer != m_partition.end() && lastiter != m_lastpartition.end())
			{
				if(conditer->second.size() != lastiter->second.size())
					goon = true;
				else
				{
					for(auto entry = lastiter->second.begin();
							 entry != lastiter->second.end();
							 entry++)
					{
						auto item = conditer->second.find(entry->first);
						if(item == conditer->second.end())
							goon = true;
						else
						{
							double diff = std::abs(entry->second - item->second);
							if(diff > 5) // 5%
								goon = true;
						}
					}
				}
			}
		}
	}
	m_lastpartition = m_partition;
	m_partition.clear();
	return goon;
}
*/
bool
HeuPartitionHelper::CheckIterationCondition()
{
	bool goon = true;
	uint32_t totalcount = 0;
	double totalvalue = 0;

	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		if((*iter)->GetNApplications() > 0)
		{
			Ptr<Application> BaseAppPtr = (*iter)->GetApplication(0);
			std::pair<uint32_t, double> v = BaseAppPtr->GetCondition();
			totalcount += v.first;
			totalvalue += v.second;
		}
	}

	if(m_condition.size() == 3)
		m_condition.pop_front();
	double result = totalvalue / totalcount * 1e4;
	m_condition.push_back(result);

	if(m_itercounter >= 5)
	{
		std::list<double>::iterator iter = m_condition.begin();
		double first  = (*iter);
		iter++;
		double second = (*iter);
		iter++;
		double third = (*iter);

		double diff1 = (third > second) ? (third - second) : (second - third);
		double diff2 = (first > second) ? (first - second) : (second - first);
		if(diff1/second < 0.05 && diff2/first < 0.05)
		{
			goon = false;
		}
	}
	return goon;
}

void
HeuPartitionHelper::Solver()
{
	OutputCacheStatus(m_itercounter);

	Initialization();

	if(m_design <= 1)
		PathCacheByPop();
	else
		PlacementByPop();

	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		auto decisioniter = m_decision.find((*iter)->GetId());
		if(decisioniter != m_decision.end())
		{
			Ptr<ContentStore> cs = (*iter)->GetObject<ContentStore>();
			cs->ClearCachedContent();

			std::vector<Name> newcs;
			NS_LOG_DEBUG("CSNode: " << (*iter)->GetId());

			for(auto item = decisioniter->second.begin();
					 item != decisioniter->second.end();
					 item++)
			{
				Ptr<Name> nameWithSequence = Create<ns3::ndn::Name>(m_prefix);
				nameWithSequence->append(item->m_bitrate);
				nameWithSequence->appendNumber(item->m_file);
				nameWithSequence->appendNumber(item->m_chunk);
				newcs.push_back(*nameWithSequence);
				NS_LOG_DEBUG(*nameWithSequence);
			}
			cs->SetContentInCache(newcs);
		}
	}
	m_itercounter++;

	if(m_iterationTimes > m_itercounter)
	{
		for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
		{
			Ptr<VideoStatistics> stats = (*iter)->GetObject<VideoStatistics>();
			if(stats != 0)
				stats->Clear();
		}
		Simulator::Schedule(Time(m_roundtime), &HeuPartitionHelper::IterativeRun, this);
	}
	else
	{
		Simulator::Stop(Time(m_roundtime));
	}
}

void
HeuPartitionHelper::PathCacheByPop()
{
	bool stopsign;
	do{
		m_cachestack.clear();
		m_decision.clear();

		DeriveCacheStackByPath();

		stopsign = true;

		for(auto nodeiter = m_topo_order_ptr.rbegin();
				 nodeiter != m_topo_order_ptr.rend();
				 nodeiter++)
		{
			std::unordered_map<VideoIndex,double> popTable;
			CandidateTable candidate;

			MergeCacheStack(*nodeiter, popTable, candidate);

			stopsign = stopsign & AdjustStackSize(*nodeiter, popTable, candidate);
		}

	}while(!stopsign);
}


bool
HeuPartitionHelper::AdjustStackSize(Ptr<Node> nodeptr,
									std::unordered_map<VideoIndex,double>& popTable,
									CandidateTable& candidate)
{
	//Key: edge router ID where routing path is transmit through nodeptr,
	//Value: the updated cache capacity
	std::map<uint32_t, uint32_t> updatesize;
	std::vector<std::pair<VideoIndex,double> > orderedTable;

	for(auto entry = popTable.begin();
			 entry != popTable.end();
			 entry++)
	{
		orderedTable.push_back(std::make_pair(entry->first, entry->second));
	}
	std::sort(orderedTable.begin(), orderedTable.end(), ns3::ndn::cmp_by_value_video);

	Ptr<ContentStore> csptr = nodeptr->GetObject<ContentStore>();
	Ptr<NDNBitRate> BRinfo = nodeptr->GetObject<NDNBitRate>();

	uint32_t limit = csptr->GetCapacity();
	uint32_t acccapacity = 0;
	std::set<VideoIndex> cachedcontent;

	for(auto entry = orderedTable.begin();
			 entry != orderedTable.end();
			 entry++)
	{
		std::string br = entry->first.m_bitrate.substr(2);
		uint32_t chunksize = static_cast<uint32_t>(BRinfo->GetChunkSize(br)) * 1e3;
		if(acccapacity + chunksize > limit)
			break;
		else
		{
			//choose cache placement by order in the table (orderedTable)
			acccapacity += chunksize;
			cachedcontent.insert(entry->first);
			for(auto canditer = candidate.begin();
					 canditer != candidate.end();
					 canditer++)
			{
				auto match = canditer->second.find(entry->first);
				if(match != canditer->second.end())
				{
					auto updateiter = updatesize.find(canditer->first);
					if(updateiter != updatesize.end())
						updateiter->second += chunksize;
					else
						updatesize.insert(std::make_pair(canditer->first, chunksize));
				}
			}
		}
	}
	m_decision.insert(std::make_pair(nodeptr->GetId(), std::move(cachedcontent)));

	NS_LOG_DEBUG("Working on Node: " << nodeptr->GetId());
	bool condition = true;
	//Update the available cache capacity for each path
	for(auto nodeiter = m_edgenode_ptr.begin();
			 nodeiter != m_edgenode_ptr.end();
			 nodeiter++)
	{
		if((*nodeiter)->GetId() == nodeptr->GetId())// No need to update if this current node is an edge router
		{
			NS_LOG_DEBUG("This Node is an edge router");
			NS_LOG_DEBUG("Previous: " << m_availcapacity[(*nodeiter)->GetId()][0]
					  << "\tCurrent: " << acccapacity);
		}
		else
		{
			auto upnodes = m_UpstreamSet.find((*nodeiter)->GetId());
			if(upnodes != m_UpstreamSet.end() && upnodes->second.size() > 0)
			{
				uint32_t idx = 1;
				for(auto upiter = upnodes->second.rbegin();
						 upiter != upnodes->second.rend();
						 upiter++)
				{
					if((*upiter)->GetId() == nodeptr->GetId())
					{
						uint32_t previous = m_availcapacity[(*nodeiter)->GetId()][idx];
						uint32_t current = updatesize[(*nodeiter)->GetId()];

						NS_LOG_DEBUG("Source: " << (*nodeiter)->GetId()
								  << "\tPrevious: " << previous 
								  << "\tAfter: " << current);

						if(current < previous)
						{
							condition = false;
							m_availcapacity[(*nodeiter)->GetId()][idx] = current;
						}
						else if(current > previous)
						{
							std::cout << "Impossible!\n";
						}
					}
					idx++;
				}
			}
		}
	}
	
	return condition;
}

void
HeuPartitionHelper::MergeCacheStack(Ptr<Node> nodeptr,
									std::unordered_map<VideoIndex,double>& popTable,
									CandidateTable& candidate)
{
	Ptr<NDNBitRate> BRinfo = nodeptr->GetObject<NDNBitRate>();

	for(auto nodeiter = m_edgenode_ptr.begin();
			 nodeiter != m_edgenode_ptr.end();
			 nodeiter++)
	{
		auto upnode = m_UpstreamSet.find((*nodeiter)->GetId());
		bool found = false;
		uint32_t len = 0;

		if(upnode->first == nodeptr->GetId())
			found = true;
		else
		{
			for(auto upiter = upnode->second.rbegin();
					 upiter != upnode->second.rend();
					 upiter++)
			{
				len++;
				if((*upiter)->GetId() == nodeptr->GetId())
				{
					found = true;
					break;
				}
			}
		}

		if(found)
		{
			std::set<VideoIndex> pathdecision;
			if(upnode != m_UpstreamSet.end() && upnode->second.size() > 0)
			{
				for(uint32_t i = 0; i < len; i++)
				{
					std::unordered_map<uint32_t, std::set<VideoIndex> >::iterator contentiter;
					if(i == 0)
						contentiter = m_decision.find(upnode->first);
					else
					{
						auto pathiter = upnode->second.rbegin();
						pathiter = pathiter + (i - 1);
						contentiter = m_decision.find((*pathiter)->GetId());
					}


					for(auto item = contentiter->second.begin();
							 item != contentiter->second.end();
							 item++)
					{
						pathdecision.insert(*item);
					}
				}
			}

			auto entry = m_cachestack[(*nodeiter)->GetId()].begin();
			std::set<VideoIndex> localdecision;
			for(uint32_t i = 0; i <= len; i++)
			{
				uint32_t acccapacity = 0;
				uint32_t limit = m_availcapacity[(*nodeiter)->GetId()][i];
				for(;
					entry != m_cachestack[(*nodeiter)->GetId()].end();
					entry++)
				{
					std::string br = entry->first.m_bitrate.substr(2);
					uint32_t chunksize = static_cast<uint32_t>(BRinfo->GetChunkSize(br)) * 1e3;

					if(acccapacity + chunksize > limit)
						break;
					else
					{
						acccapacity += chunksize;
						if(i == len)
						{
							localdecision.insert(entry->first);
							auto popiter = popTable.find(entry->first);
							auto checkiter = pathdecision.find(entry->first);
							if(checkiter == pathdecision.end())
							{
								if(popiter == popTable.end())
									popTable.insert(std::make_pair(entry->first, entry->second));
								else
									popiter->second += entry->second;
							}
							else
							{
								if(popiter == popTable.end())
									popTable.insert(std::make_pair(entry->first, 0));
							}
						}
					}
				}
			}
			candidate.insert(std::make_pair((*nodeiter)->GetId(), std::move(localdecision)));
		}
	}
}

void
HeuPartitionHelper::DeriveCacheStackByPath()
{
	for(auto nodeiter = m_edgenode_ptr.begin();
			 nodeiter != m_edgenode_ptr.end();
			 nodeiter++) // Work on each routing path
	{
		if(m_design == 1)
		{
			uint32_t totalsize = 0;

			auto sizeiter = m_availcapacity.find((*nodeiter)->GetId());

			for(uint32_t i = 0 ; i < sizeiter->second.size(); i++)
				totalsize += sizeiter->second[i];

			std::list<std::pair<VideoIndex, double> > placelist;

			AssignCacheStack(*nodeiter, totalsize, placelist);
			m_cachestack.insert(std::make_pair((*nodeiter)->GetId(), std::move(placelist)));
		}
		else if (m_design == 0)
		{
			std::map<uint32_t, std::list<std::pair<VideoIndex, double> > > localdecision;
			for(uint32_t brrank = NumB; brrank >= 1; brrank--)
			{
				std::list<std::pair<VideoIndex, double> > placelist;
				AdjustCacheStack(*nodeiter, brrank, placelist, localdecision);
				localdecision.insert(std::make_pair(brrank, std::move(placelist)));
			}
			std::list<std::pair<VideoIndex, double> > summary;
			for(uint32_t brrank = NumB; brrank >= 1; brrank--)
			{
				auto deciter = localdecision.find(brrank);
				if(deciter != localdecision.end())
				{
					for(auto item = deciter->second.begin();
							 item != deciter->second.end();
							 item++)
						summary.push_back((*item));
				}
			}
			m_cachestack.insert(std::make_pair((*nodeiter)->GetId(), std::move(summary)));

		}

	}
}

bool
HeuPartitionHelper::AllowCacheMore(Ptr<NDNBitRate> BRinfo,
								   uint32_t edgeid,
								   uint32_t workingsize,
								   uint32_t workinglistnum,
								   const std::map<uint32_t, std::list<std::pair<VideoIndex, double> > >& localdecision)
{
	auto sizeiter = m_availcapacity.find(edgeid);
	uint32_t waitrank = NumB;
	uint32_t waitnum = 0;
	auto waititer = localdecision.find(waitrank);
	if(waititer != localdecision.end())
		waitnum = waititer->second.size();

	bool allow = false;

	for(uint32_t i = 0 ; i < sizeiter->second.size(); i++)
	{
		uint32_t leftover = sizeiter->second[i];
		while(waitrank > 0)
		{
			if(waitnum == 0)
			{
				waitrank--;
				waititer = localdecision.find(waitrank);
				if(waititer != localdecision.end())
					waitnum = waititer->second.size();
				continue;
			}
			std::string br = waititer->second.front().first.m_bitrate.substr(2);
			uint32_t chunksize = BRinfo->GetChunkSize(br) * 1e3;
			uint32_t num = leftover / chunksize;
			if(num < waitnum)
			{
				waitnum -= num;
				break;
			}
			else
			{
				leftover = leftover - waitnum * chunksize;
				waitnum = 0;
			}
		}

		if(waitrank == 0)
		{
			uint32_t num = leftover / workingsize;
			if(num >= workinglistnum + 1)
			{
				allow = true;
				break;
			}
			else
				workinglistnum -= num;
		}
	}
	return allow;
}

void
HeuPartitionHelper::AdjustCacheStack(Ptr<Node> nodeptr,
									 uint32_t brrank,
									 std::list<std::pair<VideoIndex, double> >& placelist,
									 std::map<uint32_t, std::list<std::pair<VideoIndex, double> > >& localdecision)
{
	Ptr<VideoStatistics> stats = nodeptr->GetObject<VideoStatistics>();
	Ptr<NDNBitRate> BRinfo = nodeptr->GetObject<NDNBitRate>();
	uint32_t insertsize = static_cast<uint32_t>(BRinfo->GetChunkSize(BRinfo->GetBRFromRank(brrank))) * 1e3;
	double normalcost = BRinfo->GetRewardFromRank(brrank);

	std::vector<std::pair<VideoIndex,double> > orderedTable;

	for(auto entry = stats->GetTable().begin();
			 entry != stats->GetTable().end();
			 entry++)
	{
		if(entry->first.m_bitrate.substr(2) == BRinfo->GetBRFromRank(brrank))
		{
			double v = entry->second * normalcost;
			orderedTable.push_back(std::make_pair(entry->first, v));
		}
	}

	std::sort(orderedTable.begin(), orderedTable.end(), ns3::ndn::cmp_by_value_video);

	uint32_t idx = 0;
	while(idx < orderedTable.size())
	{
		if(AllowCacheMore(BRinfo, nodeptr->GetId(), insertsize, placelist.size(), localdecision))
		{
			placelist.push_back(orderedTable[idx]);
			idx++;
		}
		else
			break;
	}

	double leastpop;
	while(true)
	{
		leastpop = std::numeric_limits<double>::max();
		std::map<uint32_t, std::list<std::pair<VideoIndex, double> > >::iterator eliminateiter;

		for(auto deciter = localdecision.begin();
				 deciter != localdecision.end();
				 deciter++)
		{
			auto moditer = deciter->second.rbegin();
			//std::cout << "BR rank: " << deciter->first << "\tSize: " << deciter->second.size() << std::endl;
			if(moditer != deciter->second.rend() && moditer->second < leastpop)
			{
				leastpop = moditer->second;
				eliminateiter = deciter;
			}
		}

		if(idx < orderedTable.size() && leastpop < orderedTable[idx].second)
		{
			eliminateiter->second.pop_back();
			while(idx < orderedTable.size()
					&& AllowCacheMore(BRinfo, nodeptr->GetId(), insertsize, placelist.size(), localdecision))
			{
				placelist.push_back(orderedTable[idx]);
				idx++;
			}
		}
		else
			break;
	}
}

void
HeuPartitionHelper::AssignCacheStack(Ptr<Node> nodeptr,
								     uint32_t totalsize,
									 std::list<std::pair<VideoIndex, double> >& placelist)
{
	Ptr<VideoStatistics> stats = nodeptr->GetObject<VideoStatistics>();
	Ptr<NDNBitRate> BRinfo = nodeptr->GetObject<NDNBitRate>();

	std::vector<std::pair<VideoIndex,double> > orderedTable;

	for(auto entry = stats->GetTable().begin();
			 entry != stats->GetTable().end();
			 entry++)
	{
		orderedTable.push_back(std::make_pair(entry->first, entry->second));
	}

	std::sort(orderedTable.begin(), orderedTable.end(), ns3::ndn::cmp_by_value_video);

	if(totalsize > 0)
	{
		uint32_t idx = 0;
		std::string br = "";
		uint32_t chunksize = 0;

		while(idx < orderedTable.size())
		{
			br = orderedTable[idx].first.m_bitrate.substr(2);
			chunksize = static_cast<uint32_t>(BRinfo->GetChunkSize(br)) * 1e3;

			if(totalsize >= chunksize)
			{
				placelist.push_back(orderedTable[idx]);
				idx++;
				totalsize -= chunksize;
			}
			else
				break;
		}
	}
}

void
HeuPartitionHelper::PlacementByPop()
{
	UpdateRequestStat();

	for(auto nodeiter = m_topo_order_ptr.rbegin(); nodeiter != m_topo_order_ptr.rend(); nodeiter++)
	{
		Ptr<Node> nodeptr = *nodeiter;
		std::unordered_map<VideoIndex,double> statTable;
		std::set<VideoIndex> result;

		for(auto pathiter = m_edgenode_ptr.begin();
				 pathiter != m_edgenode_ptr.end();
				 pathiter++)
		{
			auto upnode = m_UpstreamSet.find((*pathiter)->GetId());
			bool found = false;
			uint32_t len = 0; //Count from edge router (not from consumers)

			if(upnode->first == nodeptr->GetId())
				found = true;
			else
			{
				for(auto upiter = upnode->second.rbegin();
						 upiter != upnode->second.rend();
						 upiter++)
				{
					len++;
					if((*upiter)->GetId() == nodeptr->GetId())
					{
						found = true;
						break;
					}
				}
			}

			if(found)
			{
				std::set<VideoIndex> pathdecision;
				if(upnode != m_UpstreamSet.end() && upnode->second.size() > 0)
				{
					for(uint32_t i = 0; i < len; i++)
					{
						std::unordered_map<uint32_t, std::set<VideoIndex> >::iterator contentiter;
						if(i == 0)
							contentiter = m_decision.find(upnode->first);
						else
						{
							auto pathiter = upnode->second.rbegin();
							pathiter = pathiter + (i - 1);
							contentiter = m_decision.find((*pathiter)->GetId());
						}


						for(auto item = contentiter->second.begin();
								 item != contentiter->second.end();
								 item++)
						{
							pathdecision.insert(*item);
						}
					}
				}
				CalPopScore(*pathiter, len + 1, pathdecision, statTable);
			}
		}
		FindInitCachePartition(nodeptr,statTable,result);
		m_decision.insert(std::make_pair(nodeptr->GetId(), std::move(result)));
	}
}

void
HeuPartitionHelper::CalPopScore(Ptr<Node> edgeptr,
		 	 	 	 	 	 	uint32_t hop,
								const std::set<VideoIndex>& pathdecision,
								std::unordered_map<VideoIndex,double>& statTable)
{
	// Get Total Number of Video Requests Received by All Edge Routers
	Ptr<VideoStatistics> stats = edgeptr->GetObject<VideoStatistics>();
	Ptr<NDNBitRate> BRinfo = edgeptr->GetObject<NDNBitRate>();
	std::map<std::string, double> ripple;

	for(uint32_t rank = 1; rank <= NumB; rank++)
	{
		std::string br = BRinfo->GetBRFromRank(rank);
		uint64_t BRarr = SetBRBoundary(edgeptr, br);
		ripple.insert(std::make_pair(br, RippleRatio(rank, hop, BRarr)));
	}


	for(auto tableptr = stats->GetTable().begin();
			 tableptr != stats->GetTable().end();
			 tableptr++)
	{
		auto exist = pathdecision.find(tableptr->first);
		if(exist != pathdecision.end())
			continue;

		auto statiter = statTable.find(tableptr->first);
		double v = 0;
		uint32_t brorder = BRinfo->GetRankFromBR(tableptr->first.m_bitrate.substr(2));
		if(m_design == 2) // Pure Popularity Statistics
			v = tableptr->second;
		else if(m_design == 3)// Popularity Score \times Ripple Decaying Ratio
			v = tableptr->second * ripple[tableptr->first.m_bitrate.substr(2)];
		else if(m_design == 4)// Popularity Score \times Ripple Mask \times NormalizedDeliveryCost
			v = tableptr->second * ripple[tableptr->first.m_bitrate.substr(2)] * BRinfo->GetRewardFromRank(brorder);
		else if(m_design == 5)// Popularity Score \times NormalizedDeliveryCost
			v = tableptr->second * BRinfo->GetRewardFromRank(brorder);
		if(statiter != statTable.end())
			statiter->second += v;
		else
			statTable.insert(std::make_pair(tableptr->first, v));
	}
}

double
HeuPartitionHelper::RippleRatio(uint32_t brrank, uint32_t hop, uint64_t BRarr)
{
	bool passover = false;
	uint32_t disttorank = 0;
	uint8_t ringRB = 0;

	for(uint32_t i = 1; i <= hop; i++)
	{
		ringRB = static_cast<uint8_t>(BRarr) & 0x0f;
		BRarr = BRarr >> 4;
		if(ringRB <= brrank)
			passover = true;
		if(passover && ringRB < brrank)
			disttorank++;
	}
	double r = 0;
	if(passover)
	{
		if(m_design < 4)
			r = std::pow(m_alpha, disttorank);
		else
			r = 1;
	}
	else
	{
		disttorank = 0;
		do{
			ringRB = static_cast<uint8_t>(BRarr) & 0x0f;
			BRarr = BRarr >> 4;
			disttorank++;
		}while(ringRB > brrank);
		if(m_design < 4)
			r = std::pow(m_alpha, disttorank);
		else
			r = (disttorank <= 1) ? 1 : 0;
	}
	return r;
}

void
HeuPartitionHelper::FindInitCachePartition(Ptr<Node> nodeptr,
										   std::unordered_map<VideoIndex,double>& statTable,
										   std::set<VideoIndex>& result)
{
	Ptr<NDNBitRate> BRinfo = nodeptr->GetObject<NDNBitRate>();
	Ptr<ContentStore> csptr = nodeptr->GetObject<ContentStore>();

	// ============ Convert Video Statistics Table To Video Popularity Table ============
	std::vector<std::pair<VideoIndex,double> > popTable;
	for(auto entry = statTable.begin();
			 entry != statTable.end();
			 entry++)
	{
		popTable.push_back(std::make_pair(entry->first, entry->second));
	}
	// ============ Sort Video Statistics Table =====================
	std::sort(popTable.begin(), popTable.end(), ns3::ndn::cmp_by_value_video);

	// =============Cache Content By Popularity======================
	uint32_t cssize = csptr->GetCapacity();

	// ============= Initial Cache Partitioning ========================

	uint32_t brsize = 0;

	for(auto entry = popTable.begin(); entry != popTable.end(); entry++)
	{
		brsize = BRinfo->GetChunkSize(entry->first.m_bitrate.substr(2)) * 1e3;
		if(cssize >= brsize && entry->second > 0)
		{
			cssize -= brsize;
			result.insert(entry->first);

		}
	}
}

void
HeuPartitionHelper::UpdateRequestStat()
{
	for(auto pathiter = m_edgenode_ptr.begin();
			 pathiter != m_edgenode_ptr.end();
			 pathiter++)
	{
		Ptr<NDNBitRate> BRinfo = (*pathiter)->GetObject<NDNBitRate>();
		auto lenindict = m_availcapacity.find((*pathiter)->GetId());

		//Update the Concentric-Ring by each edge router
		for(uint32_t b = 1; b <= NumB; b++)
		{
			std::string br = BRinfo->GetBRFromRank(b);
			for(uint32_t hop = 1; hop <= lenindict->second.size() + 1; hop++)
			{
				uint32_t num = 0;
				double	 totaldelay = 0;
				for(uint32_t i = 0; i < (*pathiter)->GetNDevices(); i ++)
				{
					Ptr<ns3::PointToPointNetDevice> p2pdevice
								= DynamicCast<ns3::PointToPointNetDevice>((*pathiter)->GetDevice(i));
					Ptr<NetDevice> probedevice = p2pdevice->GetChannel()->GetDevice(1);
					if(probedevice->GetNode()->GetId() == (*pathiter)->GetId())
						probedevice = p2pdevice->GetChannel()->GetDevice(0);
					Ptr<Node> probenode = probedevice->GetNode();
					if(probenode->GetNApplications() > 0)
					{
						Ptr<Application> BaseAppPtr = probenode->GetApplication(0);
						std::pair<uint32_t, uint32_t> record =
								BaseAppPtr->DelayHistory(br, hop);
						num += record.first;
						totaldelay += record.second;
					}
				}
				m_delaytable[DelayTableKey{br, hop, (*pathiter)->GetId()}]=
											(num != 0) ? (totaldelay / num) : BRinfo->GetPlaybackTime() * 2000;
			}
		}
	}
}

uint64_t
HeuPartitionHelper::SetBRBoundary(Ptr<Node> edge, const std::string& reqbr)
{
	Ptr<NDNBitRate> BRinfoPtr = edge->GetObject<NDNBitRate>();
	double limit = BRinfoPtr->GetPlaybackTime();
	double basesize = BRinfoPtr->GetChunkSize(reqbr);
	uint32_t baserank = BRinfoPtr->GetRankFromBR(reqbr);

	uint64_t bd = 0;
	uint8_t currentbd = 0;
	auto lenindict = m_availcapacity.find(edge->GetId());
	uint8_t routelen = lenindict->second.size() + 1;

	uint32_t superrank = baserank + 1;
	double accumulatedDelay = 0;
	if(superrank <= BRinfoPtr->GetTableSize())
	{
		double supersize = BRinfoPtr->GetChunkSize(BRinfoPtr->GetBRFromRank(superrank));
		for(uint32_t hop = 1; hop <= routelen; hop++)
		{
			auto iter = m_delaytable.find(DelayTableKey{reqbr, hop, edge->GetId()});
			if(iter != m_delaytable.end())
			{
				if(accumulatedDelay + (iter->second / 1000) <= (limit * basesize / supersize) && hop > currentbd)
				{
					accumulatedDelay += (iter->second / 1000); //convert from MS to S
					uint64_t mask = superrank << (currentbd * 4);
					bd = bd | mask;
					currentbd++;
				}
				else
					break;
			}

		}
	}
	for(uint32_t hop = currentbd + 1; hop <= routelen; hop++)
	{
		auto iter = m_delaytable.find(DelayTableKey{reqbr, hop, edge->GetId()});
		if(iter != m_delaytable.end())
		{
			if(accumulatedDelay + (iter->second / 1000) <= limit && hop > currentbd)
			{
				accumulatedDelay += (iter->second / 1000); //convert from MS to S
				uint64_t mask = baserank << (currentbd * 4);
				bd = bd | mask;
				currentbd++;
			}
			else
				break;
		}
	}
	if(baserank > 1)
	{
		for(uint32_t lowerrank = baserank - 1; lowerrank >= 1; lowerrank--)
		{
			double lowersize = BRinfoPtr->GetChunkSize(BRinfoPtr->GetBRFromRank(lowerrank));
			for(uint32_t hop = currentbd + 1; hop <= routelen; hop++)
			{
				auto iter = m_delaytable.find(DelayTableKey{reqbr, hop, edge->GetId()});
				if(iter != m_delaytable.end())
				{
					if(accumulatedDelay + (iter->second / 1000) <= (limit * basesize / lowersize) && hop > currentbd)
					{
						accumulatedDelay += (iter->second / 1000);
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



}
}
