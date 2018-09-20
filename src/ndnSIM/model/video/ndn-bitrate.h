/*
 * Author: Wenjie Li
 * 2016-03-25
 */

#ifndef NDN_BITRATE_H
#define NDN_BITRATE_H

#include <map>
#include <vector>
#include <string>
#include "ns3/object.h"
#include "ns3/node.h"
#include "ns3/ptr.h"

namespace ns3{
namespace ndn{


class NDNBitRate : public Object
{
public:
	static TypeId GetTypeId ();
	NDNBitRate();
	virtual ~NDNBitRate();

	void AddBitRate(const std::string&);
	double GetChunkSize(const std::string&);
	uint32_t GetTableSize();

	inline double GetPlaybackTime()
	{
		return m_duration;
	}

	inline std::string GetInitBitRate()
	{
		return m_bitrates[0];
	}

	uint32_t 	GetRankFromBR(std::string br);
	const std::string&	GetBRFromRank(uint32_t);

	std::string	GetNextHighBitrate(uint32_t rank);
	std::string	GetNextLowBitrate(uint32_t rank);


	double GetRewardFromRank(uint32_t rank);

protected:
  virtual void NotifyNewAggregate ();
  virtual void DoDispose (); ///< @brief Do cleanup

private:
  Ptr<Node> m_node;
  uint32_t 	m_totalsize;
  /*How long of playback time contained in each video block*/
  double m_duration;//In seconds

  std::map<std::string, double> m_size; // Unit: K-Byte
  std::vector<std::string>		m_bitrates;

private:
  double CalChunkSize(const std::string&);
};


}
}
#endif
