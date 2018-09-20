
#include "ndn-transcode-helper.h"
#include "ns3/node-list.h"
#include "ns3/ndn-app.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>

#include "ns3/log.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE ("ndn.Transcode");

namespace ns3{
namespace ndn{

TranscodeHelper::TranscodeHelper(uint32_t iterations,
								 const std::string& prefix,
								 const Time& t)
	:m_iterationTimes(iterations),
	 m_interval(t),
	 m_prefix(prefix)
{
	/*
	try{
		env = new GRBEnv();
	}
	catch (GRBException& e)
	{
		std::cout << e.getMessage() << std::endl;
	}
	model = nullptr;
	*/

}
TranscodeHelper::~TranscodeHelper()
{
	//delete env;
	//env = nullptr;
}

void
TranscodeHelper::FillEdgeCache()
{
	if(m_iterationTimes > 0)
	{
		for(NodeList::Iterator iter = NodeList::Begin (); iter != NodeList::End (); iter++)
		{
			Ptr<ContentStore> csptr = (*iter)->GetObject<ContentStore>();
			Ptr<VideoStatistics> statsptr = (*iter)->GetObject<VideoStatistics>();
			csptr->DisableCSUpdate();

			if(statsptr != 0) // Edge Router
			{
				FillByPopularity(csptr, statsptr);
				//FillByQP(csptr, statsptr);
			}

			if((*iter)->GetNApplications() > 0)
			{
				Ptr<Application> BaseAppPtr = (*iter)->GetApplication(0);
				BaseAppPtr->EnableRecord();
				BaseAppPtr->IncreaseTransition();
			}
		}
		m_iterationTimes--;
		Simulator::Schedule(Time(m_interval), &TranscodeHelper::FillEdgeCache, this);
	}
	else
		Simulator::Stop();
}


void
TranscodeHelper::FillByPopularity(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr)
{
	PopularityTable poptable;
	std::vector<ns3::ndn::Name> assigned;
	std::set<ContentIndex> trackitem;

	auto statsiter = statsptr->GetTable().begin();
	for(; statsiter != statsptr->GetTable().end(); statsiter++)
		poptable.push_back(std::make_pair(statsiter->first, statsiter->second));

	std::sort(poptable.begin(), poptable.end(), ns3::ndn::cmp_by_value_video);

	if(poptable.size() > 0)
	{
		uint32_t totalsize = csptr->GetCapacity();
		uint32_t transize =  totalsize * csptr->GetTranscodeRatio();
		uint32_t normalsize = totalsize - transize;

		uint32_t index = 0;
		Ptr<NDNBitRate> brinfo = csptr->GetObject<NDNBitRate>();
		std::string highestbr = brinfo->GetBRFromRank(brinfo->GetTableSize());

		uint32_t chunksize = brinfo->GetChunkSize(poptable[index].first.m_bitrate.substr(2)) * 1e3;

		while(normalsize >= chunksize)
		{
			// Create Name and Add to 'assigned'
			Ptr<Name> popularname = Create<Name>(m_prefix);
			popularname->append(poptable[index].first.m_bitrate);
			popularname->appendNumber(poptable[index].first.m_file);
			popularname->appendNumber(poptable[index].first.m_chunk);
			assigned.push_back(*popularname);

			if(poptable[index].first.m_bitrate == "br" + highestbr)
				trackitem.insert(ContentIndex{poptable[index].first.m_file, poptable[index].first.m_chunk});

			normalsize -= chunksize;
			index++;

			chunksize = brinfo->GetChunkSize(poptable[index].first.m_bitrate.substr(2)) * 1e3;
		}

		chunksize = brinfo->GetChunkSize(highestbr) * 1e3;

		while(transize >= chunksize)
		{
			auto trackiter = trackitem.find(ContentIndex{poptable[index].first.m_file, poptable[index].first.m_chunk});
			if(trackiter != trackitem.end())
			{
				index++;
				continue;
			}
			Ptr<Name> popularname = Create<Name>(m_prefix);
			popularname->append("br" + highestbr);
			popularname->appendNumber(poptable[index].first.m_file);
			popularname->appendNumber(poptable[index].first.m_chunk);
			assigned.push_back(*popularname);
			trackitem.insert(ContentIndex{poptable[index].first.m_file, poptable[index].first.m_chunk});

			transize -= chunksize;
			index++;
		}

		csptr->ClearCachedContent(); // Clear content in existing CS
		csptr->SetContentInCache(assigned);
	}
	statsptr->Clear();
}
/*
void
TranscodeHelper::FillByQP(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr)
{
	std::vector<ns3::ndn::Name> assigned;
	if(statsptr->GetTable().size() > 0)
	{
		int status = SolveQP(csptr, statsptr);
		if(status == GRB_OPTIMAL)
			NS_LOG_INFO("SUCCEED TO FIND AN OPTIMAL Value: "<< model->get(GRB_DoubleAttr_ObjVal));
		else if(status == GRB_INF_OR_UNBD)
			NS_FATAL_ERROR("Infeasible!!!!");
		else if(status == GRB_TIME_LIMIT)
			NS_LOG_INFO("CANNOT SOLVE IP IN TIME LIMIT");
		else
			NS_FATAL_ERROR("Unexpected error occurs!");
		for(auto iter = m_varDict.begin(); iter != m_varDict.end(); iter++)
		{
			if(iter->second.get(GRB_DoubleAttr_X) > 0)
			{
				Ptr<Name> popularname = Create<Name>(m_prefix);
				popularname->append(iter->first.m_bitrate);
				popularname->appendNumber(iter->first.m_file);
				popularname->appendNumber(iter->first.m_chunk);
				assigned.push_back(*popularname);
			}
		}

		for(uint32_t i = 0; i < assigned.size(); i++)
			std::cout << assigned[i] << std::endl;

		csptr->ClearCachedContent(); // Clear content in existing CS
		csptr->SetContentInCache(assigned);
	}
	statsptr->Clear();
}
int
TranscodeHelper::SolveQP(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr)
{
	//Reset and Start a new Gurobi model
	GRBModelInit();

	//Declare variables and add into Gurobi model
	AddOptVar(csptr);

	//Set Optimization Objective
	SetOptimizationObj(csptr, statsptr);

	//Set Optimization Constraints
	SetOptimizationConstr(csptr, statsptr);

	model->update();
	model->optimize();
	return model->get(GRB_IntAttr_Status);
}

void
TranscodeHelper::GRBModelInit()
{
	if(model != nullptr)
	{
		delete model;
		model = nullptr;

		m_varDict.clear();
	}
	model = new GRBModel(*env);

	model->set(GRB_IntAttr_ModelSense, GRB_MINIMIZE); //Optimize Objective
	model->set(GRB_IntParam_LogToConsole, 1);//Print the solving procedure on screen
	model->set(GRB_DoubleParam_TimeLimit, 3600.0);	//Set time limit for solving optimization
}

void
TranscodeHelper::AddOptVar(Ptr<ContentStore> csptr)
{
	Ptr<NDNBitRate> brinfo = csptr->GetObject<NDNBitRate>();
	for(uint32_t f = 1; f <= m_NumOfFiles; f++)
	{
		for(uint32_t k = 1; k <= m_NumOfChunks; k++)
		{
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				std::string reqbr = "br" + brinfo->GetBRFromRank(b);
				GRBVar v = model->addVar(0.0, 1.0, 0.0, GRB_BINARY);
				m_varDict.insert(std::make_pair(VideoIndex{reqbr, f, k}, v));
			}
		}
	}
}

void
TranscodeHelper::SetOptimizationObj(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr)
{
	GRBQuadExpr obj;
	Ptr<NDNBitRate> brinfo = csptr->GetObject<NDNBitRate>();
	std::string highestbr = brinfo->GetBRFromRank(m_NumofBitRates);

	for(uint32_t f = 1; f <= m_NumOfFiles; f++)
	{
		for(uint32_t k = 1; k <= m_NumOfChunks; k++)
		{
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				std::string br = "br" + brinfo->GetBRFromRank(b);
				double chunksize = brinfo->GetChunkSize(br.substr(2)); //Unit: KBytes
				auto GRBVarPtr = m_varDict.find(VideoIndex{br, f, k});
				double numreq = statsptr->GetHitNum(VideoIndex{br, f, k});

				obj += numreq
						* GetAccessDelay(1, chunksize, br, csptr)
						* GRBVarPtr->second;

				if(b != m_NumofBitRates)
				{
					auto TranscodeVarPtr = m_varDict.find(VideoIndex{"br"+highestbr, f, k});
						obj += numreq
								* GetAccessDelay(2, chunksize, br, csptr)
								* (1 - GRBVarPtr->second)
								* TranscodeVarPtr->second;

						obj += numreq
								* GetAccessDelay(3, chunksize, br, csptr)
								* (1 - GRBVarPtr->second)
								* (1 - TranscodeVarPtr->second);
				}
				else
				{
					obj += numreq
							* GetAccessDelay(3, chunksize, br, csptr)
							* (1 - GRBVarPtr->second);
				}
			}
		}
	}
	model->setObjective(obj);
}

void
TranscodeHelper::SetOptimizationConstr(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr)
{
	GRBLinExpr cache_constr;
	Ptr<NDNBitRate> brinfo = csptr->GetObject<NDNBitRate>();
	uint32_t totalsize = csptr->GetCapacity();

	for(uint32_t f = 1; f <= m_NumOfFiles; f++)
	{
		for(uint32_t k = 1; k <= m_NumOfChunks; k++)
		{
			for(uint32_t b = 1; b <= m_NumofBitRates; b++)
			{
				std::string br = "br" + brinfo->GetBRFromRank(b);
				double chunksize = brinfo->GetChunkSize(br.substr(2)); //Unit: KBytes
				auto GRBVarPtr = m_varDict.find(VideoIndex{br, f, k});

				cache_constr += chunksize * 1e3 * GRBVarPtr->second;
			}
		}
	}
	model->addConstr(cache_constr, GRB_LESS_EQUAL, totalsize);
}

double
TranscodeHelper::GetAccessDelay(uint32_t mode,
								double chunksize,
		  	  	  	  	  	    const std::string& br,
								Ptr<ContentStore> csptr)
{
	std::size_t found = m_lastmilebw.find("Kbps");
	double bw = 2000;
	if(found != std::string::npos)
		bw = std::stod(m_lastmilebw.substr(0, found));
	double lastmiledelay = chunksize * 8 * 1e3/ bw; // Unit: MilliSecond
	double accessdelay = 0;

	switch(mode)
	{
		case 1: accessdelay = lastmiledelay;
				break;
		case 2: accessdelay = lastmiledelay + csptr->GetTranscodeCost(true, br);
				break;
		case 3: accessdelay = lastmiledelay + csptr->GetTranscodeCost(false, br);
				break;
		default:accessdelay = 0;
				break;
	}
	return accessdelay;
}
*/

}
}
