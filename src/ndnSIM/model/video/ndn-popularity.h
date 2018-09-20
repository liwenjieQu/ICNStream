/*
 * Author by Wenjie Li
 * Date 2015-10-07
 */

#ifndef NDN_POPULARITY_H
#define NDN_POPULARITY_H

#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <utility>

#include "ns3/ptr.h"
#include "ns3/name.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ndn-videostat.h"


namespace ns3{
class Node;
namespace ndn{
/*
 * This class provides access to four popularity parameters for calculating caching utility.
 */

class PopularitySummary : public Object
{
public:
	static TypeId GetTypeId ();

	PopularitySummary();
	virtual ~PopularitySummary();

	void UpdateDelay(const Name&, const Time&);
	inline bool CheckMarkStatus() {return IsMarked;}
	inline void SetMarkStatus() {IsMarked = true;}

	void Aggregate(const PopularitySummary&);
	void Summarize();
	void Clear();
	void CalculateUtility();
	std::vector<ValuedVideoIndex>& UtilityRank();

	// Added by Wenjie Li (Date: 12/07/2016) for MARL
	bool SenseVideoRequest(uint32_t s, double* ratio);

protected:
  virtual void DoDispose (void); ///< @brief Do cleanup

  virtual void NotifyNewAggregate ();

private:
	Ptr<VideoStatistics> 					m_statptr;

	std::unordered_map<uint32_t, uint64_t> 	m_file_stat;
	std::unordered_map<uint32_t, uint64_t> 	m_chunk_stat;
	std::map<std::string, uint64_t> 		m_bitrate_stat;
	std::map<uint32_t, double> 				m_file_dis;
	std::map<std::string, double> 			m_bitrate_dis;
	std::map<std::string, std::pair<uint64_t,double> >			m_delay;

	bool 	IsEdge = true;
	bool 	IsMarked = false;
	bool 	firstRun = true;
	bool 	ForMARLonly = false; //Added by Wenjie Li on 12/07/2016

	double  m_pro_nextchunk;

	void SummarizeToBitrate();
	void SummarizeToFile();
	void SummarizeToChunk();

	void AggregateToFile(const PopularitySummary&);
	void AggregateToBitrate(const PopularitySummary&);

	std::string RetrieveBitrate(const std:: string&);
	void Print();
private:
	uint32_t				m_maxNumchunk;
	uint32_t				m_maxNumFile;

	std::vector<ValuedVideoIndex> m_utilities;

	Ptr<Node>				m_node;
};



}
}

#endif
