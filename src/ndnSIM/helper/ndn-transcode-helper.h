/*
 * Author: Wenjie Li
 * Date: 2018-02-11
 * Perform (Partial) Transcoding for cached video content
 */

#ifndef NDN_TRANSCODE_HELPER_H
#define NDN_TRANSCODE_HELPER_H

#include "ns3/ptr.h"
#include "ns3/nstime.h"
#include "ns3/node.h"
#include "ns3/node-container.h"

#include "ns3/ndn-videocontent.h"
#include "ns3/ndn-videostat.h"
#include "ns3/ndn-bitrate.h"
#include "ns3/ndn-content-store.h"
#include "ns3/ndn-partition-helper.h"

#include "gurobi_c++.h"
#include <vector>

namespace ns3{
namespace ndn{


class TranscodeHelper
{
	using PopularityTable = std::vector<std::pair<VideoIndex,uint64_t> >;
public:
/*
	TranscodeHelper(uint32_t NumF, uint32_t NumC, uint32_t NumB,
					uint32_t iterations,
				    const std::string& prefix,
					const std::string& lastmilebw,
					const Time& t);
*/
	TranscodeHelper(uint32_t iterations,
				    const std::string& prefix,
					const Time& t);

	~TranscodeHelper();

	void FillEdgeCache();

private:
	void FillByPopularity(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr);
	//void FillByQP(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr);
	//int SolveQP(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr);
/*
	void GRBModelInit();
	void AddOptVar(Ptr<ContentStore> csptr);
	void SetOptimizationObj(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr);
	void SetOptimizationConstr(Ptr<ContentStore> csptr, Ptr<VideoStatistics> statsptr);
*/
	/*
	 * Get Access Delay by Mode:
	 * Mode 1: Retrieve From Edge Router (Only Includes Last Mile Delay)
	 * Mode 2: Retrieve Via Transcoding (Last Mile Delay + Transcoding Delay)
	 * Mode 3: Retrieve From Producer (Last Mile Delay + Network Core Transmission Delay)
	 */
/*
	double GetAccessDelay(uint32_t mode,
						  double chunksize,
						  const std::string& br,
						  Ptr<ContentStore> csptr);
*/
private:
	//GRBEnv* 		env;
	//GRBModel* 	model;

	//uint32_t 		m_NumOfChunks;
	//uint32_t		m_NumOfFiles;
	//uint32_t		m_NumofBitRates;

	uint32_t		m_iterationTimes;
	Time			m_interval;
	std::string		m_prefix;
	//std::string 	m_lastmilebw;

	//std::unordered_map<VideoIndex, GRBVar> m_varDict;
};


}
}

#endif
