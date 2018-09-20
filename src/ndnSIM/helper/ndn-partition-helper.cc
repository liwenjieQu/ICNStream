
#include "ndn-partition-helper.h"
#include "ns3/net-device.h"
#include "ns3/node-list.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/channel.h"
#include "ns3/ndn-app.h"
#include "ns3/ndn-bitrate.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/ndn-content-store.h"
#include <algorithm>
#include <cmath>
#include <iomanip>

#include "ns3/log.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE ("ndn.CachePartition");

namespace ns3{
namespace ndn{

PartitionHelper::PartitionHelper(uint32_t NumF, uint32_t NumC, uint32_t NumB, uint32_t NumNodes,
								 const std::string ArrB[], uint32_t EdgeSize, uint32_t IntmSize,
								 const std::string& prefix,
								 const std::string& roundtime,
								 double alpha, uint32_t id, uint32_t designchoice,
								 uint32_t iterations,
								 const std::string& logpath,
								 sql::Driver* driver,
								 const std::string& DatabaseName,
								 const std::string& user,
								 const std::string& password,
								 const std::string& ip,
								 int port)
{
	try{
		env = new GRBEnv();
	}
	catch (GRBException& e)
	{
		std::cout << e.getMessage() << std::endl;
	}
	model = nullptr;
	m_totalreq = 0;

	m_NumOfChunks = NumC;
	m_NumOfFiles = NumF;
	m_NumofBitRates = NumB;
	m_NumofNodes = NumNodes;

	m_edgesize = EdgeSize;
	m_intmsize = IntmSize;

	m_prefix = prefix;
	m_roundtime = roundtime;
	m_iterationTimes = iterations;
	m_itercounter = 0;
	m_alpha = alpha;
	m_design = designchoice;
	m_expid = id;

	m_BRnames = new std::string [NumB + 1];
	for(uint32_t i = 1; i <= NumB; i++)
		m_BRnames[i] = "br" + ArrB[i - 1];

	m_log = new std::ofstream;
	m_log->open(logpath.c_str(), std::ofstream::out | std::ofstream::trunc);
	if(!m_log->is_open () || !m_log->good ())
	{
		//std::cout << "Open LogFile Failure!" << std::endl;
		delete m_log;
		m_log = nullptr;
	}

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

PartitionHelper::~PartitionHelper()
{
	delete env;
	env = nullptr;

	delete [] m_BRnames;
	m_BRnames = nullptr;

	if(m_log != nullptr){
		m_log->flush();
		m_log->close();

		delete m_log;
		m_log = nullptr;
	}

	delete m_stmt;
	m_stmt = nullptr;
	delete m_con;
	m_con = nullptr;
}

bool
PartitionHelper::CheckIterationCondition()
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
PartitionHelper::Solver()
{
	OutputCacheStatus(m_itercounter);

	int status = SolveIntegerPro();
	//VerifyMARLDesign();

	if(model->get(GRB_DoubleAttr_MIPGap) > 4e-2)
		std::cout << "Target Gap is not reached! [ExpID]: " << m_expid
				  << "\tIteration: " << m_itercounter
				  << "\tGap: "<< model->get(GRB_DoubleAttr_MIPGap)
				  << std::endl;
	if(status == GRB_OPTIMAL)
		NS_LOG_INFO("SUCCEED TO FIND AN OPTIMAL Value: "<< model->get(GRB_DoubleAttr_ObjVal));
	else if(status == GRB_INF_OR_UNBD)
		NS_FATAL_ERROR("Infeasible!!!!");
	else if(status == GRB_TIME_LIMIT)
		NS_LOG_INFO("CANNOT SOLVE IP IN TIME LIMIT");
	else
		NS_FATAL_ERROR("Unexpected error occurs!");

	// Get Optimized Result from GUROBI and Update Content Store(CS)
	std::unordered_map<uint32_t, std::vector<Name> > optresult;
	auto iter = m_varDict.begin();
	for(; iter != m_varDict.end(); iter++)
	{
		if(iter->second.get(GRB_DoubleAttr_X) > 0)
		{
			uint32_t nodeid = iter->first.m_id;
			auto nodecs = optresult.find(nodeid);
			if(nodecs == optresult.end())
			{
				std::vector<Name> newcs;
				optresult[nodeid] = newcs;
				nodecs = optresult.find(nodeid);
			}
			Ptr<Name> nameWithSequence = Create<ns3::ndn::Name>(m_prefix);
			nameWithSequence->append(iter->first.m_vi.m_bitrate);
			nameWithSequence->appendNumber(iter->first.m_vi.m_file);
			nameWithSequence->appendNumber(iter->first.m_vi.m_chunk);
			nodecs->second.push_back(*nameWithSequence);
		}
	}
	//VerifyOptResult(optresult);
	NS_LOG_DEBUG("SEE Iteration: " << m_itercounter);
	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		Ptr<ContentStore> cs = (*iter)->GetObject<ContentStore>();
		cs->ClearCachedContent();
		if(optresult.find((*iter)->GetId()) != optresult.end())
		{
			cs->SetContentInCache(optresult[(*iter)->GetId()]);
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
		Simulator::Schedule(Time(m_roundtime), &PartitionHelper::IterativeRun, this);
	}
	else
	{
		Simulator::Stop(Time(m_roundtime));
	}

}

void
PartitionHelper::OutputCacheStatus(uint32_t count)
{
	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		Ptr<ContentStore> cs = (*iter)->GetObject<ContentStore>();
		Ptr<NDNBitRate> BRinfo = (*iter)->GetObject<NDNBitRate>();
		if(cs != 0)
		{
			std::vector<ns3::ndn::Name> cscontent;
			std::map<std::string, uint32_t> cssize;
			cs->FillinCacheRun(cscontent);

			for(uint32_t i = 0; i < cscontent.size(); i++)
			{
				std::string br = cscontent[i].get(-3).toUri();
				uint32_t chunksize = static_cast<uint32_t>(BRinfo->GetChunkSize(br.substr(2)));
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
			uint32_t safety = 0;
			try{
				for(auto entry = cssize.begin(); entry != cssize.end(); entry++)
				{
					std::size_t found = entry->first.find(tail);
					safety += entry->second;

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
			safety =  safety * 1e3;

			//safety check
			if(cs->GetCapacity() < safety)
				std::cout << "DANGER: Execution ERROR!!!!!!!\n";
		}
	}
}

void
PartitionHelper::IterativeRun()
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

void
PartitionHelper::GRBModelInit()
{
	if(model != nullptr)
	{
		delete model;
		model = nullptr;

		m_varDict.clear();
		m_pathvarDict.clear();
	}
	m_totalreq = 0;

	model = new GRBModel(*env);

	model->set(GRB_IntAttr_ModelSense, GRB_MAXIMIZE); //Optimize Objective
	model->set(GRB_IntParam_MIPFocus, 3);	  //Try reducing the gap as much as possible
	model->set(GRB_IntParam_Method, 3);	  //User concurrent solver
	model->set(GRB_IntParam_LogToConsole, 1);//Print the solving procedure on screen
	//model->set("LogFile", "/home/wenjie/ClionProjects/ndnSIM_ICC/ns-3/partition");

	model->set(GRB_DoubleParam_MIPGap,4e-2);		//Set gap distance (accuracy of result)
	model->set(GRB_IntParam_Threads, 16);
	model->set(GRB_DoubleParam_TimeLimit, 4800.0);	//Set time limit for solving optimization
}

int
PartitionHelper::SolveIntegerPro()
{
	//Reset and Start a new Gurobi model
	GRBModelInit();

	//Aggregate video requests statistics
	UpdateRequestStat();

	//Declare variables and add into Gurobi model
	AddOptVar();

	//Set Optimization Objective
	SetOptimizationObj();

	//Set Optimization Constraints
	SetOptimizationConstr();

	model->update();
	model->optimize();
	return model->get(GRB_IntAttr_Status);
}

void
PartitionHelper::AddOptVar()
{
	//model->addVar(0.0, 1.0, 0.0, GRB_BINARY);

	//Add Binary Variable for Cache Placement Decision
	for(uint32_t f = 1; f <= m_NumOfFiles; f++)
	{
		for(uint32_t k = 1; k <= m_NumOfChunks; k++)
		{
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				for(uint32_t n = 1; n < m_NumofNodes; n++)
				{
					m_varDict[VarIndex{n, VideoIndex{m_BRnames[b], f, k}}] = model->addVar(0.0, 1.0, 0.0, GRB_BINARY);
				}
				for(auto iter = m_forwardingpath.begin();
						 iter != m_forwardingpath.end();
						 iter++)
				{
					for(auto pathiter = iter->second.rbegin();
							 pathiter != iter->second.rend();
							 pathiter++)
					{
						// Add another constraint on \alpha_j(L(m), f, k, b), if j == L(m) since edgeid is included
						m_pathvarDict[PathVarIndex{
							iter->first, (*pathiter)->GetId(), VideoIndex{m_BRnames[b], f, k}}] = model->addVar(0.0, 1.0, 0.0, GRB_BINARY);
					}
				}
			}
		}
	}
}

void
PartitionHelper::SetOptimizationObj()
{
	GRBLinExpr obj;

	for(uint32_t b = 1; b <= m_NumofBitRates; b++)
	{
		for(auto iter = m_forwardingpath.begin();
				 iter != m_forwardingpath.end();
				 iter++)
		{
			Ptr<Node> EdgeNodePtr = *(iter->second.rbegin());
			Ptr<VideoStatistics> vstats = EdgeNodePtr->GetObject<VideoStatistics>();
			uint64_t ConcentricRing = SetBRBoundary(EdgeNodePtr, m_BRnames[b]);
			uint64_t bd = ConcentricRing;
			for(auto pathiter = iter->second.rbegin();
					 pathiter != iter->second.rend();
					 pathiter++)
			{
				double reward = CalReward(bd, b, EdgeNodePtr);
				for(uint32_t f = 1; f <= m_NumOfFiles; f++)
				{
					for(uint32_t k = 1; k <= m_NumOfChunks; k++)
					{
						double popularity = (vstats->GetHitNum(VideoIndex{m_BRnames[b], f, k}) * 1e4) / static_cast<double>(m_totalreq);
						if(pathiter == iter->second.rbegin())
						{
							auto GRBVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, (*pathiter)->GetId(), VideoIndex{m_BRnames[b], f, k}});
							if(GRBVarPtr != m_pathvarDict.end())
								obj += popularity * reward * GRBVarPtr->second;
							else
								NS_FATAL_ERROR("GRBVar Not Found!");
						}
						else
						{
							auto GRBVarPtr1 = m_pathvarDict.find(PathVarIndex{iter->first, (*pathiter)->GetId(), VideoIndex{m_BRnames[b], f, k}});
							auto GRBVarPtr2 = m_pathvarDict.find(PathVarIndex{iter->first, (*(pathiter - 1))->GetId(), VideoIndex{m_BRnames[b], f, k}});
							if(GRBVarPtr1 != m_pathvarDict.end() && GRBVarPtr2 != m_pathvarDict.end())
								obj += popularity * reward * (GRBVarPtr1->second - GRBVarPtr2->second);
							else
								NS_FATAL_ERROR("GRBVar Not Found!!");
						}
					}
				}
			}
		}
	}
	model->setObjective(obj);
}

void
PartitionHelper::SetOptimizationConstr()
{
	//Cache Capacity Constraints
	for(uint32_t n = 1; n < m_NumofNodes; n++)
	{
		GRBLinExpr cache_constr;
		bool isEdge = false;
		for(NodeContainer::Iterator iter = m_edgenodes.Begin();
									iter != m_edgenodes.End();
									iter++)
		{
			if((*iter)->GetId() == n)
				isEdge = true;
		}
		uint32_t cachesize = isEdge ? m_edgesize : m_intmsize;
		Ptr<NDNBitRate> BRinfoPtr= (*(m_edgenodes.Begin()))->GetObject<NDNBitRate>();
		for(uint32_t b = 1; b <= m_NumofBitRates; b++)
		{
			uint32_t coeff = BRinfoPtr->GetChunkSize(m_BRnames[b].substr(2));
			for(uint32_t f = 1; f <= m_NumOfFiles; f++)
			{
				for(uint32_t k = 1; k <= m_NumOfChunks; k++)
				{
					auto XVarPtr = m_varDict.find(VarIndex{n, VideoIndex{m_BRnames[b], f, k}});
					if(XVarPtr != m_varDict.end())
						cache_constr += coeff * (XVarPtr->second);
					else
						NS_FATAL_ERROR("GRBVar Not Found!!!");
				}
			}
		}
		model->addConstr(cache_constr, GRB_LESS_EQUAL, cachesize);
	}

	//Relationship Between \alpha and X
	for(uint32_t f = 1; f <= m_NumOfFiles; f++)
	{
		for(uint32_t k = 1; k <= m_NumOfChunks; k++)
		{
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				for(auto iter = m_forwardingpath.begin();
						 iter != m_forwardingpath.end();
						 iter++)
				{
					std::deque<Ptr<Node> >::reverse_iterator pathiter = (iter->second.rbegin()) + 1;
					for(; pathiter != iter->second.rend(); pathiter++)
					{
						if((*pathiter)->GetId() != (*(iter->second.begin()))->GetId())//Not the Video Producer
						{
							uint32_t currentid = (*pathiter)->GetId();
							uint32_t previd = (*(pathiter - 1))->GetId();

							auto currentVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, currentid, VideoIndex{m_BRnames[b], f, k}});
							auto prevVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, previd, VideoIndex{m_BRnames[b], f, k}});
							auto XVarPtr = m_varDict.find(VarIndex{currentid, VideoIndex{m_BRnames[b], f, k}});

							model->addConstr(currentVarPtr->second, GRB_GREATER_EQUAL, prevVarPtr->second);
							model->addConstr(currentVarPtr->second, GRB_GREATER_EQUAL, XVarPtr->second);
							model->addConstr(currentVarPtr->second, GRB_LESS_EQUAL, prevVarPtr->second + XVarPtr->second);

						}
					}
				}
			}
		}
	}

	//Rate Adaptation Among Chunks (Avoid BitRate Oscillation)
	/*
	for(auto iter = m_forwardingpath.begin();
			 iter != m_forwardingpath.end();
			 iter++)
	{
		std::deque<Ptr<Node> >::iterator rootptr = iter->second.begin();
		uint32_t gateid = (*(++rootptr))->GetId();
		Ptr<Node> EdgeNodePtr = *(iter->second.rbegin());
		uint64_t ConcentricRing = SetBRBoundary(EdgeNodePtr);
		ConcentricRing = ConcentricRing >> (4 * (m_longestRoutingHop > 1 ? (m_longestRoutingHop - 1): 0));
		uint8_t ProducerRing = static_cast<uint8_t>(ConcentricRing) & 0x0f;
		for(uint32_t f = 1; f <= m_NumOfFiles; f++)
		{
			for(uint32_t k = 2; k <= m_NumOfChunks; k++)
			{
				for(uint32_t b = 1; b <= m_NumofBitRates; b++)
				{
					if(b > static_cast<uint32_t>(ProducerRing))
					{
						auto currentVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, gateid, VideoIndex{m_BRnames[b], f, k}});
						auto prevVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, gateid, VideoIndex{m_BRnames[b], f, k - 1}});
						if(b > 1)
						{
							auto prevVarPtr_l = m_pathvarDict.find(PathVarIndex{iter->first, gateid, VideoIndex{m_BRnames[b - 1], f, k - 1}});
							if(b == m_NumofBitRates)
								model->addConstr(prevVarPtr->second + prevVarPtr_l->second, GRB_GREATER_EQUAL, currentVarPtr->second);
							else
							{
								auto prevVarPtr_h = m_pathvarDict.find(PathVarIndex{iter->first, gateid, VideoIndex{m_BRnames[b + 1], f, k - 1}});
								model->addConstr(prevVarPtr->second + prevVarPtr_l->second + prevVarPtr_h->second, GRB_GREATER_EQUAL, currentVarPtr->second);
							}
						}
						else
						{
							auto prevVarPtr_h = m_pathvarDict.find(PathVarIndex{iter->first, gateid, VideoIndex{m_BRnames[b + 1], f, k - 1}});
							model->addConstr(prevVarPtr->second + prevVarPtr_h->second, GRB_GREATER_EQUAL, currentVarPtr->second);
						}
					}
				}
			}
		}
	}
	*/

	//Artificial Constraints
	for(uint32_t b = 1; b <= m_NumofBitRates; b++)
	{
		for(uint32_t f = 1; f <= m_NumOfFiles; f++)
		{
			for(uint32_t k = 1; k <= m_NumOfChunks; k++)
			{
				//
				for(auto iter = m_forwardingpath.begin();
						 iter != m_forwardingpath.end();
						 iter++)
				{
					uint32_t rootid = (*(iter->second.begin()))->GetId();
					auto GRBVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, rootid, VideoIndex{m_BRnames[b], f, k}});
					model->addConstr(GRBVarPtr->second, GRB_EQUAL, 1);

					uint32_t edgeid = (*(iter->second.rbegin()))->GetId();
					GRBVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, edgeid, VideoIndex{m_BRnames[b], f, k}});
					auto XVarPtr = m_varDict.find(VarIndex{iter->first, VideoIndex{m_BRnames[b], f, k}});
					model->addConstr(GRBVarPtr->second, GRB_EQUAL, XVarPtr->second);
				}
			}
		}
	}

	double M = 1e5;
	for(NodeContainer::Iterator nodeiter = m_edgenodes.Begin();
								nodeiter != m_edgenodes.End();
								nodeiter++)
	{
		auto iter = m_forwardingpath.find((*nodeiter)->GetId());
		PopularityTable pt;
		Ptr<VideoStatistics> stats = (*nodeiter)->GetObject<VideoStatistics>();
		OrderPopTable(stats, pt);

		std::deque<Ptr<Node> >::iterator gateptr = iter->second.begin() + 1;
		uint32_t gateid = (*gateptr)->GetId();
		uint32_t producerid = (*(iter->second.begin()))->GetId();

		for(auto pathiter = iter->second.rbegin();
				 pathiter != iter->second.rend();
				 pathiter++)
		{
			if((*pathiter)->GetId() != gateid && (*pathiter)->GetId() != producerid)
			{
				for(auto popiter = pt.begin(); popiter != pt.end(); popiter++)
				{
					for(uint32_t vidx = 1; vidx < popiter->second.size(); vidx++)
					{
						std::pair<VideoIndex, uint64_t> entry = (popiter->second)[vidx];
						if(entry.second >= 1)
						{
							GRBLinExpr lhs_constr;
							auto currentVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, (*pathiter)->GetId(), entry.first});
							GRBLinExpr rhs_constr = M - M * currentVarPtr->second;
							for(uint32_t previdx = 0; previdx < vidx; previdx++)
							{
								std::pair<VideoIndex, uint64_t> preentry = (popiter->second)[previdx];
								auto prevGateVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, gateid, preentry.first});
								auto prevVarPtr = m_pathvarDict.find(PathVarIndex{iter->first, (*pathiter)->GetId(), preentry.first});
								lhs_constr += prevGateVarPtr->second - prevVarPtr->second;
							}
							model->addConstr(lhs_constr, GRB_LESS_EQUAL, rhs_constr);
						}
					}
				}
			}
		}
	}
}

bool cmp_by_value_video(const std::pair<ns3::ndn::VideoIndex, uint64_t>& lhs,
		const std::pair<ns3::ndn::VideoIndex, uint64_t>& rhs)
{ return (lhs.second > rhs.second)
		 || (lhs.second == rhs.second && lhs.first.m_file < rhs.first.m_file)
		 || (lhs.second == rhs.second && lhs.first.m_file == rhs.first.m_file && lhs.first.m_chunk < rhs.first.m_chunk)
		 || (lhs.second == rhs.second && lhs.first.m_file == rhs.first.m_file && lhs.first.m_chunk == rhs.first.m_chunk && lhs.first.m_bitrate < rhs.first.m_bitrate);
}

void
PartitionHelper::OrderPopTable(Ptr<VideoStatistics> vstat, PopularityTable& vtable)
{
	auto vtableptr = vstat->GetTable().begin();
	auto vtableend = vstat->GetTable().end();
	for(; vtableptr != vtableend; vtableptr++)
	{
		std::string br  = vtableptr->first.m_bitrate;
		vtable[br].push_back(std::make_pair(vtableptr->first, vtableptr->second));
	}
	for(auto iter = vtable.begin(); iter != vtable.end(); iter++)
		std::sort(iter->second.begin(), iter->second.end(), ns3::ndn::cmp_by_value_video);
/*
	NS_LOG_DEBUG("BR: " << (vtable.begin())->first);
	for(uint32_t testiter = 0;
			     testiter != (vtable.begin()->second).size();
			     testiter++)
	{
		NS_LOG_DEBUG("F: "<< ((vtable.begin()->second)[testiter]).first.m_file
				<< "  C: "<< ((vtable.begin()->second)[testiter]).first.m_chunk
				<< "  Value: "<< ((vtable.begin()->second)[testiter]).second);
	}
*/

}

void
PartitionHelper::UpdateRequestStat()
{
	for(NodeContainer::Iterator iter = m_edgenodes.Begin();
								iter != m_edgenodes.End();
								iter++)
	{
		// Get Total Number of Video Requests Received by All Edge Routers
		Ptr<VideoStatistics> stats = (*iter)->GetObject<VideoStatistics>();
		if(stats != 0)
		{
			auto tableptr = stats->GetTable().begin();
			auto tableend = stats->GetTable().end();
			for(; tableptr != tableend; tableptr++)
				m_totalreq += tableptr->second;
		}


		//Update the Concentric-Ring by each edge router
		for(uint32_t b = 1; b <= m_NumofBitRates; b++)
		{
			for(uint32_t hop = 1; hop <= m_longestRoutingHop; hop++)
			{
				uint32_t num = 0;
				double	 totaldelay = 0;
				for(uint32_t i = 0; i < (*iter)->GetNDevices(); i ++)
				{
					Ptr<ns3::PointToPointNetDevice> p2pdevice
								= DynamicCast<ns3::PointToPointNetDevice>((*iter)->GetDevice(i));
					Ptr<NetDevice> probedevice = p2pdevice->GetChannel()->GetDevice(1);
					if(probedevice->GetNode()->GetId() == (*iter)->GetId())
						probedevice = p2pdevice->GetChannel()->GetDevice(0);
					Ptr<Node> probenode = probedevice->GetNode();
					if(probenode->GetNApplications() > 0)
					{
						Ptr<Application> BaseAppPtr = probenode->GetApplication(0);
						std::pair<uint32_t, uint32_t> record =
								BaseAppPtr->DelayHistory(m_BRnames[b].substr(2), hop);
						num += record.first;
						totaldelay += record.second;
					}
				}
				m_delaytable[DelayTableKey{m_BRnames[b].substr(2), hop, (*iter)->GetId()}]=
											(num != 0) ? (totaldelay / num) : (*iter)->GetObject<NDNBitRate>()->GetPlaybackTime() * 2000;
			}
		}
	}
}



void
PartitionHelper::SetTopologicalOrder(Ptr<Node> ServerNode, NodeContainer edges)
{
	std::deque<Ptr<Node> > depStack;
	DepthFirstSearch(ServerNode, depStack);
	auto iter = m_forwardingpath.begin();
	m_longestRoutingHop = iter->second.size();
	for(;iter != m_forwardingpath.end();
		 iter++)
	{
		if(iter->second.size() > m_longestRoutingHop)
			m_longestRoutingHop = iter->second.size();
	}
	m_edgenodes = edges;
}

void
PartitionHelper::DepthFirstSearch(Ptr<Node> searchpoint, std::deque<Ptr<Node> >& execStack)
{
	//Depth-first Search
	// Find forwarding path from a certain edge router; return array of NodeIDs (m_forwardingpath)

	uint32_t num = searchpoint->GetNDevices();
	Ptr<Node> lastnode = 0;
	if(execStack.size() > 0)
		lastnode = execStack.back();
	execStack.push_back(searchpoint);

	if(num == 1 && lastnode != 0)
		m_forwardingpath[searchpoint->GetId()] = execStack;

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

uint64_t
PartitionHelper::SetBRBoundary(Ptr<Node> edge)
{
	Ptr<NDNBitRate> BRinfoPtr = edge->GetObject<NDNBitRate>();
	double limit = BRinfoPtr->GetPlaybackTime();
	uint64_t bd = 0;
	uint8_t currentbd = 0;
	uint8_t routelen = m_longestRoutingHop;

	for(uint32_t i = BRinfoPtr->GetTableSize(); i >= 1; i--)
	{
		double accumulatedDelay = 0;
		for(uint32_t hop = 1; hop <= routelen; hop++)
		{
			auto iter = m_delaytable.find(DelayTableKey{BRinfoPtr->GetBRFromRank(i), hop, edge->GetId()});
			if(iter != m_delaytable.end())
			{
				accumulatedDelay += (iter->second / 1000); //convert from MS to S
				if(accumulatedDelay <= limit && hop > currentbd)
				{
					uint64_t mask = i << (currentbd * 4);
					bd = bd | mask;
					currentbd++;
				}
			}
		}
	}
	return bd;
}

uint64_t
PartitionHelper::SetBRBoundary(Ptr<Node> edge, const std::string& reqbr)
{
	Ptr<NDNBitRate> BRinfoPtr = edge->GetObject<NDNBitRate>();
	double limit = BRinfoPtr->GetPlaybackTime();
	double basesize = BRinfoPtr->GetChunkSize(reqbr.substr(2));
	uint32_t baserank = BRinfoPtr->GetRankFromBR(reqbr.substr(2));

	uint64_t bd = 0;
	uint8_t currentbd = 0;
	uint8_t routelen = m_longestRoutingHop;

	uint32_t superrank = baserank + 1;
	double accumulatedDelay = 0;
	if(superrank <= BRinfoPtr->GetTableSize())
	{
		double supersize = BRinfoPtr->GetChunkSize(BRinfoPtr->GetBRFromRank(superrank));
		for(uint32_t hop = 1; hop <= routelen; hop++)
		{
			auto iter = m_delaytable.find(DelayTableKey{reqbr.substr(2), hop, edge->GetId()});
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
		auto iter = m_delaytable.find(DelayTableKey{reqbr.substr(2), hop, edge->GetId()});
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
				auto iter = m_delaytable.find(DelayTableKey{reqbr.substr(2), hop, edge->GetId()});
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

double
PartitionHelper::CalReward(uint64_t& bd, uint32_t reqBR, Ptr<Node> edge)
{
	Ptr<NDNBitRate> m_BRinfo = edge->GetObject<NDNBitRate>();
	uint8_t ringRB = static_cast<uint8_t>(bd) & 0x0f;
	bd = bd >> 4;

	double r = 0;
	if(ringRB == 0)
		r = m_BRinfo->GetRewardFromRank(1);
	else
	{
		if(ringRB > reqBR)
		{
			
			double newrank = reqBR + m_alpha;
			r = m_BRinfo->GetRewardFromRank(reqBR+1) * (1 / newrank)
			    	+ m_BRinfo->GetRewardFromRank(reqBR) * (1 - 1 / newrank);
		}
		else if(ringRB == reqBR)
		{
			r = m_BRinfo->GetRewardFromRank(reqBR);
		}
		else
		{
			if(m_design == 1)
			{
				r = m_BRinfo->GetRewardFromRank(reqBR - 1);
			}
			else if(m_design == 2)
			{
				r = m_BRinfo->GetRewardFromRank(ringRB);
			}
		}

	}
	return r;
}

void
PartitionHelper::VerifyOptResult(const std::unordered_map<uint32_t, std::vector<Name> >& optresult)
{
	// Verify whether optimized result match expectation
	// 1. Capacity
	/*
	for(NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
	{
		NS_LOG_DEBUG("CheckCapacity: " << (*node)->GetId());
		auto iter = optresult.find((*node)->GetId());
		if(iter != optresult.end())
		{
			std::map<std::string, uint32_t> count;
			for(uint32_t i = 0; i < iter->second.size(); i++)
			{
				std::string br = iter->second[i].get(-3).toUri();
				count[br]++;
			}
			for(auto statptr = count.begin(); statptr != count.end(); statptr++)
				NS_LOG_DEBUG("BitRate: " << statptr->first << "\tCount: " << statptr->second);
		}
	}
	*/

	//2. Cached Content Arragement (by different video file)


	uint32_t showid[5] = {1,5,10};

	if(m_log != nullptr)
		*m_log << "!!![Iteration: " << m_itercounter  << "]!!!\n";

	for(uint32_t tt = 0; tt < 3; tt++)
	{
		if(m_log != nullptr)
			*m_log << "Cache Arrangement File " << showid[tt] << std::endl;
		for(NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
		{
			auto iter = optresult.find((*node)->GetId());
			if(iter != optresult.end())
			{
				Ptr<VideoStatistics> stats = (*node)->GetObject<VideoStatistics>();
				if(m_log != nullptr)
					*m_log << "CS: " << (*node)->GetId() << std::endl;
				for(uint32_t i = 0; i < iter->second.size(); i++)
				{
					uint32_t fileid = static_cast<uint32_t>(iter->second[i].get(-2).toNumber());
					if(fileid == showid[tt])
					{
						if(m_log != nullptr)
						 *m_log << "VideoBR: " << iter->second[i].get(-3).toUri() << "  ChunkID: "
								<< static_cast<uint32_t>(iter->second[i].get(-1).toNumber()) << std::endl;
					}
				}
			}
		}
	}


	//Print Gateway
	/*
	auto gateiter = optresult.find(2);
	if(gateiter != optresult.end())
	{
		Ptr<Node> nodeiter;
		for(NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
		{
			if((*node)->GetId() == 2)
			{
				nodeiter = *node;
				break;
			}
		}

		Ptr<VideoStatistics> stats = nodeiter->GetObject<VideoStatistics>();
		NS_LOG_DEBUG("Print 2");
		std::map<std::string, uint32_t> sizecheck;
		for(uint32_t i = 0; i < gateiter->second.size(); i++)
		{
			sizecheck[gateiter->second[i].get(-3).toUri()]++;

			uint32_t fileid = static_cast<uint32_t>(gateiter->second[i].get(-2).toNumber());
			uint32_t num = stats->GetHitNum(VideoIndex{gateiter->second[i].get(-3).toUri(),
										fileid,
										static_cast<uint32_t>(gateiter->second[i].get(-1).toNumber())});

			NS_LOG_DEBUG("VideoBR: " << gateiter->second[i].get(-3).toUri()
					<< "  FileID: " << fileid
					<< "  ChunkID: " << static_cast<uint32_t>(gateiter->second[i].get(-1).toNumber())
					<< "\tCount: " << num);
		}
		for(auto sizeiter = sizecheck.begin(); sizeiter != sizecheck.end(); sizeiter++)
			NS_LOG_DEBUG("BR: " << sizeiter->first << "  Num: " << sizeiter->second);
	}


	auto gateiter2 = optresult.find(8);
	if(gateiter2 != optresult.end())
	{
		Ptr<Node> nodeiter;
		for(NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
		{
			if((*node)->GetId() == 8)
			{
				nodeiter = *node;
				break;
			}
		}

		Ptr<VideoStatistics> stats = nodeiter->GetObject<VideoStatistics>();
		NS_LOG_DEBUG("Print 8");
		std::map<std::string, uint32_t> sizecheck;
		for(uint32_t i = 0; i < gateiter2->second.size(); i++)
		{
			sizecheck[gateiter2->second[i].get(-3).toUri()]++;

			uint32_t fileid = static_cast<uint32_t>(gateiter2->second[i].get(-2).toNumber());
			uint32_t num = stats->GetHitNum(VideoIndex{gateiter2->second[i].get(-3).toUri(),
										fileid,
										static_cast<uint32_t>(gateiter2->second[i].get(-1).toNumber())});

			NS_LOG_DEBUG("VideoBR: " << gateiter2->second[i].get(-3).toUri()
					<< "  FileID: " << fileid
					<< "  ChunkID: " << static_cast<uint32_t>(gateiter2->second[i].get(-1).toNumber())
					<< "\tCount: " << num);
		}
		for(auto sizeiter = sizecheck.begin(); sizeiter != sizecheck.end(); sizeiter++)
			NS_LOG_DEBUG("BR: " << sizeiter->first << "  Num: " << sizeiter->second);
	}
	*/


	for(auto gateiter = optresult.begin(); gateiter != optresult.end(); gateiter++)
	{
		Ptr<Node> nodeiter;
		for(NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
		{
			if((*node)->GetId() == gateiter->first)
			{
				nodeiter = *node;
				break;
			}
		}
		if(m_log != nullptr)
			*m_log << "ContentStore: [" << gateiter->first << "]\n";
		std::map<std::string, uint32_t> sizecheck;
		for(uint32_t i = 0; i < gateiter->second.size(); i++)
		{
			sizecheck[gateiter->second[i].get(-3).toUri()]++;
		}
		for(auto sizeiter = sizecheck.begin(); sizeiter != sizecheck.end(); sizeiter++)
		{
			if(m_log != nullptr)
				*m_log << "BR: " << sizeiter->first << "  Num: " << sizeiter->second << std::endl;
		}
	}


	// Look at the coefficent in the optimization objective
	/*
	for(uint32_t b = 3; b <= m_NumofBitRates; b++)
	{
		for(auto iter = m_forwardingpath.begin();
				 iter != m_forwardingpath.end();
				 iter++)
		{
			Ptr<Node> EdgeNodePtr = *(iter->second.rbegin());
			Ptr<VideoStatistics> vstats = EdgeNodePtr->GetObject<VideoStatistics>();
			uint64_t ConcentricRing = SetBRBoundary(EdgeNodePtr, m_BRnames[b]);
			uint64_t bd = ConcentricRing;
			for(auto pathiter = iter->second.rbegin();
					 pathiter != iter->second.rend();
					 pathiter++)
			{
				double reward = CalReward(bd, b, EdgeNodePtr);
				uint32_t f = 5;
				for(uint32_t k = 9; k <= 12; k++)
				{
					double popularity = (vstats->GetHitNum(VideoIndex{m_BRnames[b], f, k}) * 1e4) / static_cast<double>(m_totalreq);
					if((*pathiter)->GetId() == 1 || (*pathiter)->GetId() == 0)
					{
						auto GRBVarPtr1 = m_pathvarDict.find(PathVarIndex{iter->first, (*pathiter)->GetId(), VideoIndex{m_BRnames[b], f, k}});
						auto GRBVarPtr2 = m_pathvarDict.find(PathVarIndex{iter->first, (*(pathiter - 1))->GetId(), VideoIndex{m_BRnames[b], f, k}});
						*m_log << "[EdgeID: " << iter->first << "; Node: " << (*pathiter)->GetId() <<"; D: " << (*(pathiter - 1))->GetId() <<"] "
							   << "{K " << k << "; BR " << m_BRnames[b] << "} "
							   << "{NodeX: " << GRBVarPtr1->second.get(GRB_DoubleAttr_X) << "; DX: " << GRBVarPtr2->second.get(GRB_DoubleAttr_X) <<"} "
							   << "{Reward: " << reward << "; Pop: " << popularity << "} "
							   << "{COEFFICIENT: " << GRBVarPtr1->second.get(GRB_DoubleAttr_Obj) << "}\n";
					}
				}
			}
		}
	}
	*/
	
}

void
PartitionHelper::VerifyMARLDesign()
{
	std::unordered_map<DelayTableKey, double> sizemap;

	std::map<std::pair<uint32_t,uint32_t>, double> rewardmap; //Consider the reward on producer!
	std::unordered_map<DelayTableKey, double> cachehitmap; // Also include the hit on producer!
	std::unordered_map<DelayTableKey, double> requestmap;

	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		Ptr<ContentStore> cs = (*iter)->GetObject<ContentStore>();
		Ptr<NDNBitRate> brinfo = (*iter)->GetObject<NDNBitRate>();
		cs->FillinCacheRun();
		std::vector<Name>::iterator csiter = cs->inCacheRun.begin();
		// Update Cache Size Per Bitrate
		for(; csiter != cs->inCacheRun.end(); csiter++)
		{
			std::string br = (*csiter).get(-3).toUri();
			double s = brinfo->GetChunkSize(br.substr(2));
			sizemap[DelayTableKey{(*csiter).get(-3).toUri(),0,(*iter)->GetId()}]+=s;
		}

		Ptr<VideoStatistics> vstats = (*iter)->GetObject<VideoStatistics>();
		if(vstats != 0)
		{
			for(uint32_t f = 1; f <= m_NumOfFiles; f++)
			{
				for(uint32_t k = 1; k <= m_NumOfChunks; k++)
				{
					for(uint32_t b = 1; b <= m_NumofBitRates; b++)
					{
						double reqnum = vstats->GetHitNum(VideoIndex{m_BRnames[b], f, k});
						requestmap[DelayTableKey{m_BRnames[b],0,(*iter)->GetId()}] += reqnum;
					}
				}
			}
		}
	}
	for(auto iter = m_forwardingpath.begin();
			 iter != m_forwardingpath.end();
			 iter++)
	{
		for(uint32_t b = 1; b <= m_NumofBitRates; b++)
		{
			Ptr<Node> EdgeNodePtr = *(iter->second.rbegin());
			Ptr<VideoStatistics> vstats = EdgeNodePtr->GetObject<VideoStatistics>();
			uint64_t ConcentricRing = SetBRBoundary(EdgeNodePtr, m_BRnames[b]);
			uint64_t bd = ConcentricRing;
			for(auto pathiter = iter->second.rbegin();
					 pathiter != iter->second.rend();
					 pathiter++)
			{
				auto tmp = pathiter + 1;
				if((*pathiter)->GetId() != (*(iter->second.begin()))->GetId())
					requestmap[DelayTableKey{m_BRnames[b],0,(*tmp)->GetId()}]
								  += requestmap[DelayTableKey{m_BRnames[b],0,(*pathiter)->GetId()}];
				double reward = CalReward(bd, b, EdgeNodePtr);
				Ptr<ContentStore> currentNodeCS = (*pathiter)->GetObject<ContentStore>();
				std::vector<Name>::iterator csiter = currentNodeCS->inCacheRun.begin();
				for(; csiter != currentNodeCS->inCacheRun.end(); csiter++)
				{
					std::string CSbr = (*csiter).get(-3).toUri();
					if(CSbr == m_BRnames[b])
					{
						uint32_t f = (*csiter).get(-2).toNumber();
						uint32_t k = (*csiter).get(-1).toNumber();
						double popularity = vstats->GetHitNum(VideoIndex{m_BRnames[b], f, k});
						cachehitmap[DelayTableKey{m_BRnames[b],iter->first,(*pathiter)->GetId()}] += popularity;
						rewardmap[std::make_pair(iter->first,(*pathiter)->GetId())] += popularity * reward;
						requestmap[DelayTableKey{m_BRnames[b],0,(*tmp)->GetId()}] -= popularity;
						vstats->ResetHitNum(VideoIndex{m_BRnames[b], f, k});
					}
				}
				if((*pathiter)->GetId() == (*(iter->second.begin()))->GetId())
				{
					for(uint32_t f = 1; f <= m_NumOfFiles; f++)
					{
						for(uint32_t k = 1; k <= m_NumOfChunks; k++)
						{
							double popularity = vstats->GetHitNum(VideoIndex{m_BRnames[b], f, k});
							cachehitmap[DelayTableKey{m_BRnames[b],iter->first,(*pathiter)->GetId()}] += 0;
							rewardmap[std::make_pair(iter->first,(*pathiter)->GetId())] += popularity * reward;
						}
					}
				}
			}
		}
	}
	// Build Statistics Table is done. Since this line we construct the output.
	for(uint32_t b = 1; b <= m_NumofBitRates; b++)
	{
		for(auto iter = m_forwardingpath.begin();
				 iter != m_forwardingpath.end();
				 iter++)
		{
			for(auto pathiter = iter->second.begin();
					 pathiter != iter->second.end();
					 pathiter++)
			{
				auto moveiter = iter->second.begin();
				for(; moveiter <= pathiter; moveiter++)
				{
					rewardmap[std::make_pair(0,(*pathiter)->GetId())]
							  += rewardmap[std::make_pair(iter->first,(*moveiter)->GetId())];
					cachehitmap[DelayTableKey{m_BRnames[b],0,(*pathiter)->GetId()}]
								+= cachehitmap[DelayTableKey{m_BRnames[b],iter->first,(*moveiter)->GetId()}];
				}
			}
		}
	}
	if(m_log != nullptr)
		*m_log << "!!![Iteration: " << m_itercounter  << "]!!!";
	if(m_log != nullptr)
		*m_log << "\n==========================CacheSize=====================================\n";
	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		if((*iter)->GetNApplications() == 0)
		{
			if(m_log != nullptr)
				*m_log << "Node: " << std::left<< std::setw(6) <<(*iter)->GetId();
			double totalsize = 0;
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				auto sizeiter = sizemap.find(DelayTableKey{m_BRnames[b],0,(*iter)->GetId()});
				if(sizeiter != sizemap.end())
					totalsize += sizeiter->second;
			}
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				auto sizeiter = sizemap.find(DelayTableKey{m_BRnames[b],0,(*iter)->GetId()});
				if(m_log != nullptr)
					*m_log << m_BRnames[b]<<": " <<std::fixed<< std::setprecision(3)
						<< ((sizeiter != sizemap.end()) ? (sizeiter->second/totalsize) : 0) <<"; ";
			}
			if(m_log != nullptr)
				*m_log << std::endl;
		}
	}
	if(m_log != nullptr)
		*m_log << "\n==========================RequestPer=====================================\n";
	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		if((*iter)->GetNApplications() == 0)
		{
			if(m_log != nullptr)
				*m_log << "Node: " << std::left<< std::setw(6) <<(*iter)->GetId();
			double mtotalreq = 0;
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				mtotalreq += requestmap[DelayTableKey{m_BRnames[b],0,(*iter)->GetId()}];
			}
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				auto reqiter = requestmap.find(DelayTableKey{m_BRnames[b],0,(*iter)->GetId()});
				if(m_log != nullptr)
					*m_log << m_BRnames[b]<<": " << std::fixed << std::setprecision(0) << ((reqiter != requestmap.end()) ? (reqiter->second) : 0) <<"("
						   << std::fixed << std::setprecision(3)
						   << ((reqiter != requestmap.end()) ? (reqiter->second / mtotalreq) : 0) <<"); ";
			}
			if(m_log != nullptr)
				*m_log << std::endl;
		}
	}
	if(m_log != nullptr)
		*m_log << "\n==========================CacheHit=====================================\n";
	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		if((*iter)->GetNApplications() == 0)
		{
			if(m_log != nullptr)
				*m_log << "Node: " << std::left<< std::setw(6) <<(*iter)->GetId();
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				double r = requestmap[DelayTableKey{m_BRnames[b],0,(*iter)->GetId()}];
				double d = cachehitmap[DelayTableKey{m_BRnames[b],0,(*iter)->GetId()}];
				if(m_log != nullptr)
					*m_log << m_BRnames[b]<<": " << std::fixed << std::setprecision(0) << d << "("
						   << std::fixed<< std::setprecision(3)
						   << ((r != 0) ? (d/r)*1e2 : 0) <<"); ";
			}
			if(m_log != nullptr)
				*m_log << std::endl;
		}
	}
	if(m_log != nullptr)
		*m_log << "\n==========================Reward=====================================\n";
	for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
	{
		if((*iter)->GetNApplications() == 0)
		{
			if(m_log != nullptr)
				*m_log << "Node: " << std::left<< std::setw(6) <<(*iter)->GetId();
			double mtotalreq = 0;
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				mtotalreq += requestmap[DelayTableKey{m_BRnames[b],0,(*iter)->GetId()}];
			}
			if(m_log != nullptr)
				*m_log << "Reward: " << std::fixed<< std::setprecision(3)
					   << rewardmap[std::make_pair(0, (*iter)->GetId())] << "("
					   << rewardmap[std::make_pair(0, (*iter)->GetId())] * 1e4/ mtotalreq << ")\n";
		}
	}
}


}
}

