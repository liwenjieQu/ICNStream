/*
 * Author: Wenjie Li
 * Date: 2016-12-07
 */

#include "ns3/nstime.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/node-list.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/simulator.h"
#include "ns3/channel.h"
#include "ns3/callback.h"
#include "ns3/ndn-popularity.h"
#include "ns3/ndn-videostat.h"
#include "ns3/ndn-app.h"
#include "ns3/ndn-content-store.h"

#include "ndn-marl-helper.h"
#include "ns3/ndn-marl.h"

#include "ns3/string.h"

#include <cmath>
#include <sstream>


NS_LOG_COMPONENT_DEFINE ("ndn.MARLHelper");

namespace ns3{
namespace ndn{

MARLHelper::MARLHelper(const std::string& triggerTime,
					   const std::string& stepPeriod,
					   const std::string& episodePeriod,
					   uint32_t steps, uint32_t episodes,
					   double epsilon)
	:m_step_gap(stepPeriod),
	 m_episode_gap(episodePeriod),
	 m_trigger(triggerTime),
	 m_steplimit(steps),
	 m_episodelimit(episodes),
	 m_epsilon(epsilon),
	 m_SeqRng (0.0, 1.0)
{
	m_marlFactory.SetTypeId("ns3::ndn::rl::AggregatedState::AggregatedAction");
}

MARLHelper::~MARLHelper()
{

}

void
MARLHelper::SetLogPath(const std::string & logpath,
					 const std::string & qpath)
{
	m_log = new std::ofstream;
	m_log->open(logpath.c_str(), std::ios::out | std::ios::trunc);
	if(!m_log->is_open () || !m_log->good ())
	{
		std::cout << "Open LogFile Failure!" << std::endl;
		delete m_log;
		m_log = nullptr;
	}
	m_qrecorder = new std::fstream;
	m_qrecorder->open(qpath.c_str(), std::ios::in);
	if(!m_qrecorder->is_open () || !m_qrecorder->good ())
	{
		std::cout << "Read QTable Failure!" << std::endl;
		delete m_qrecorder;
		m_qrecorder = nullptr;
	}
	m_qTablePath = qpath;
}
void
MARLHelper::SetMARLAttributes(const std::string &marltype,
							  const std::string &attr1, const AttributeValue &value1,
							  const std::string &attr2, const AttributeValue &value2,
							  const std::string &attr3, const AttributeValue &value3,
							  const std::string &attr4, const AttributeValue &value4,
							  const std::string &attr5, const AttributeValue &value5)
{
	m_marlFactory.SetTypeId (marltype);
	if (attr1 != "")
		  m_marlFactory.Set (attr1, value1);
	if (attr2 != "")
		  m_marlFactory.Set (attr2, value2);
	if (attr3 != "")
		  m_marlFactory.Set (attr3, value3);
	if (attr4 != "")
		  m_marlFactory.Set (attr4, value4);
	if (attr5 != "")
		  m_marlFactory.Set (attr5, value5);
}

void
MARLHelper::SetTopologicalOrder(Ptr<Node> ServerNode)
{
	std::deque<Ptr<Node> > execStack;
	execStack.push_back(ServerNode);

	m_topo_order_ptr.push_back(ServerNode);
	BreadthFirstSearch(execStack);
	m_topo_order_ptr.pop_front();

	std::deque<Ptr<Node> > depStack;
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
	depStack.clear();
	m_HsetDict[ServerNode->GetId()] = depStack;
}

void
MARLHelper::BreadthFirstSearch(std::deque<Ptr<Node> >& execStack)
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
			for(auto iter = m_topo_order_ptr.begin();
					iter != m_topo_order_ptr.end();
					iter++)
			{
				if((*iter)->GetId() == probedevice->GetNode()->GetId())
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
MARLHelper::DepthFirstSearch(Ptr<Node> searchpoint, std::deque<Ptr<Node> >& execStack)
{
	//Depth-first Search
	//Construct the Dependency Set
	m_HsetDict[searchpoint->GetId()] = execStack;

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

		if(lastnode != 0 &&
				lastnode->GetId() == probedevice->GetNode()->GetId())
			continue;

		DepthFirstSearch(probedevice->GetNode(), execStack);
	}

	execStack.pop_back();
}


void
MARLHelper::InstallAll()
{
	for(auto i = m_topo_order_ptr.begin(); i != m_topo_order_ptr.end(); i++)
	{
		Ptr<Node> nodeptr = *i;
		if(nodeptr->GetObject<ContentStore>() == 0)
		{
			NS_LOG_WARN("[MARL Helper]: Install on a node without ContentStore!");
			continue;
		}
		Ptr<MARLnfa> rl = m_marlFactory.Create<MARLnfa>();
		rl->SetLogger(m_log);
		nodeptr->AggregateObject(rl);
		auto iter = m_HsetDict.find(nodeptr->GetId());
		if(iter != m_HsetDict.end())
		{
			for(auto listiter = iter->second.begin(); listiter != iter->second.end(); listiter++)
				rl->AddToHset(*listiter);
		}
	}
}

void
MARLHelper::StartRL()
{
	if(m_init)
	{
		m_init = false;
		InputQTable();
		Simulator::Schedule(m_trigger, &MARLHelper::StartRL, this);
	}
	else
	{
		std::cout << "Start Episode: " << m_currepisode << std::endl;
		//Reset to the initial status
		for(auto i = m_topo_order_ptr.begin(); i != m_topo_order_ptr.end(); i++)
		{
			Ptr<Node> nodeptr = *i;
			Ptr<MARLnfa> rlptr = nodeptr->GetObject<MARLnfa>();
			if(rlptr == 0)
			{
				NS_LOG_WARN("[MARL Helper]: StartRL has null ptr!");
				continue;
			}
			rlptr->ResetToInit();
			nodeptr->GetObject<PopularitySummary>()->Clear();
			nodeptr->GetObject<VideoStatistics>()->Clear();
		}
		Simulator::Schedule (m_step_gap, &MARLHelper::BeforeTransition, this);
	}
}

void
MARLHelper::BeforeTransition()
{
	if(m_currstep < m_steplimit && m_currepisode < m_episodelimit)
	{
		NS_LOG_DEBUG(Simulator::Now ().ToDouble(Time::S));
		if(m_log != nullptr)
			*m_log << "\nTransition " << m_currstep << "\n";
		double p_random = m_SeqRng.GetValue();
		bool exploitation = true;
		double param = (-1)/3000;

		if(p_random < std::exp(param * static_cast<double>(m_currepisode)))
			exploitation = false;

		//if(p_random < m_epsilon)
			//exploitation = false;

		if(m_currepisode % 500 == 0 || m_currepisode + 5 >= m_episodelimit)
			exploitation = true;
		if(m_currepisode + 5 >= m_episodelimit)
		{
			/*
			if(!m_measurement)
			{
				m_measurement = true;
				m_result = true;
			}
			*/
			// Update the Transition number in APP
			for(NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
			{
				Ptr<Node> Nptr = *node;
				if(Nptr->GetNApplications() > 0)
				{
					Ptr<Application> BaseAppPtr = Nptr->GetApplication(0);
					if(m_currstep == 0)
						BaseAppPtr->ResetCounter();
					BaseAppPtr->IncreaseTransition();
					BaseAppPtr->DisableRecord();
				}
			}
		}

		for(auto i = m_topo_order_ptr.begin(); i != m_topo_order_ptr.end(); i++)
		{
			Ptr<Node> Nptr = *i;
			if(m_currepisode % 500 == 0 || m_currepisode + 5 >= m_episodelimit)
			{
				Ptr<ContentStore> csptr = Nptr->GetObject<ContentStore>();
				csptr->ReportPartitionStatus();
			}
			Ptr<MARLnfa> rlptr = Nptr->GetObject<MARLnfa>();
			rlptr->UpdateCurrentState();

			bool find_another_action = rlptr->UpdateCurrentAction(exploitation);
			while(!find_another_action)
				find_another_action = rlptr->UpdateCurrentAction(exploitation);
		}
		Simulator::Schedule (Time(Seconds(1500)), &MARLHelper::GapForReplacement, this);
		Simulator::Schedule (m_step_gap + Time(Seconds(1500)), &MARLHelper::AfterTransition, this);
		m_currstep++;
	}
	else if (m_currepisode + 1 < m_episodelimit)
	{
		m_currstep = 0;
		m_currepisode++;
		for(auto i = m_topo_order_ptr.begin(); i != m_topo_order_ptr.end(); i++)
		{
			Ptr<Node> Nptr = *i;
			if(Nptr->GetId() == 8)
			{
				std::cout << "[Router " << Nptr->GetId() << "] "
						  << "Q size: " << Nptr->GetObject<MARLnfa>()->GetQStatus()
						  << "; Revisits: " << Nptr->GetObject<MARLnfa>()->GetNumOfVisit();
				std::cout << "\n\n";
			}
			Ptr<MARLnfa> rlptr = Nptr->GetObject<MARLnfa>();
			if(rlptr == 0)
			{
				NS_LOG_WARN("[MARL Helper]: StartRL has null ptr!");
				continue;
			}
			rlptr->ResetToInit();
		}
		Simulator::Schedule (m_episode_gap, &MARLHelper::StartRL, this);
	}
	else
	{
		std::cout << "MARL for Cache Partition is done!" << std::endl;
		/*
		if(!m_result)
		{
			std::ostringstream ss;
			ss << "UPDATE " << m_TableName
					<< " SET " << m_ColumnName << "=FALSE WHERE " << m_KeyName << "=" << key << ";";
			try{
				if(!con->isValid())
				{
					con->reconnect();
					con->setSchema(m_databaseName);
				}
				sql::Statement* stmt = con->createStatement();
				stmt->execute(ss.str());
				delete stmt;
				stmt = nullptr;
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
		*/
		OutputQTable();
		if(m_qrecorder != nullptr){
			m_qrecorder->flush();
			m_qrecorder->close();
		}
		if(m_log != nullptr){
			m_log->flush();
			m_log->close();
		}
		Simulator::Stop();
	}
}

void
MARLHelper::AfterTransition()
{
	for(auto i = m_topo_order_ptr.begin(); i != m_topo_order_ptr.end(); i++)
	{
		Ptr<Node> nodeptr = *i;
		Ptr<MARLnfa> rlptr = nodeptr->GetObject<MARLnfa>();
		bool status = rlptr->Transition();
		/*
		if(m_measurement && m_currstep < m_steplimit)
			m_result = m_result && status;
		*/
	}
	BeforeTransition();//Next round begins
}

void
MARLHelper::GapForReplacement()
{
	for(auto i = m_topo_order_ptr.begin(); i != m_topo_order_ptr.end(); i++)
	{
		Ptr<Node> Nptr = *i;
		Ptr<MARLnfa> rlptr = Nptr->GetObject<MARLnfa>();
		rlptr->ResetCSReward();
		Nptr->GetObject<PopularitySummary>()->Clear();
		Nptr->GetObject<VideoStatistics>()->Clear();

	}
	if(m_currepisode + 5 >= m_episodelimit)
	{
		for(NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
		{
			Ptr<Node> Nptr = *node;
			if(Nptr->GetNApplications() > 0)
			{
				Ptr<Application> BaseAppPtr = Nptr->GetApplication(0);
				BaseAppPtr->EnableRecord();
			}
		}
	}
}

void
MARLHelper::PrepareSqlUpdate(sql::Connection* c, const std::string& tn,
		  const std::string& cn, const std::string& kn,
		  uint32_t kv, const std::string& name)
{
	con = c;
	m_TableName = tn;
	m_ColumnName = cn;
	m_KeyName = kn;
	m_databaseName = name;
	key = kv;
}

void
MARLHelper::InputQTable()
{
	std::string line;
	for(auto i = m_topo_order_ptr.begin(); i != m_topo_order_ptr.end(); i++)
	{
		if(m_qrecorder != nullptr && std::getline(*m_qrecorder,line))
		{
			if (line == "") continue;
			if (line[0] == '#')
			{
				Ptr<Node> Nptr = *i;
				uint32_t id = std::stoi(line.substr(1));
				if(id != Nptr->GetId())
				{
					NS_FATAL_ERROR("QTABLE DOES NOT MATCH FORMAT!");
				}
				Ptr<MARLnfa> rlptr = Nptr->GetObject<MARLnfa>();
				rlptr->InputQStatus(m_qrecorder);
			}
		}
	}
	if(m_qrecorder != nullptr)
		m_qrecorder->close();
}

void MARLHelper::OutputQTable()
{
	if(m_qrecorder == nullptr)
		m_qrecorder = new std::fstream;
	m_qrecorder->open(m_qTablePath.c_str(), std::ios::out | std::ios::trunc);
	if(!m_qrecorder->is_open () || !m_qrecorder->good ())
	{
		std::cout << "Write QTable Failure!" << std::endl;
		delete m_qrecorder;
		m_qrecorder = nullptr;
	}
	for(auto i = m_topo_order_ptr.begin(); i != m_topo_order_ptr.end(); i++)
	{
		if(m_qrecorder != nullptr)
		{
			Ptr<Node> Nptr = *i;
			*m_qrecorder << "#" << Nptr->GetId() << std::endl;
			Ptr<MARLnfa> rlptr = Nptr->GetObject<MARLnfa>();
			rlptr->OutputQStatus(m_qrecorder);
		}
	}
}


}
}
