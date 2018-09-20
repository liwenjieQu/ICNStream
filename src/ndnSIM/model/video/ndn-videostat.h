/*
 * Author by Wenjie Li
 * Date: 2015-10-06
 */

#ifndef NDN_VIDEOSTAT_H
#define NDN_VIDEOSTAT_H

#include "ndn-videocontent.h"
#include "ns3/ptr.h"
#include "ns3/object.h"

#include <unordered_map>
#include <vector>

namespace ns3{
class Node;

namespace ndn{

class VideoStatistics : public Object
{
public:
	static TypeId GetTypeId ();

	VideoStatistics();
	virtual ~VideoStatistics();

	void Add(VideoIndex&&);

	inline void Clear()
	{
		m_stat.clear();
	}
	inline uint64_t	GetHitNum(const VideoIndex& vi)
	{
		auto iter = m_stat.find(vi);
		if(iter != m_stat.end())
			return iter->second;
		else
			return 0;
	}
	
	inline uint64_t GetHitNum(VideoIndex&& vi)
	{
		auto iter = m_stat.find(vi);
		if(iter != m_stat.end())
			return iter->second;
		else
			return 0;
	}

	inline const std::unordered_map<VideoIndex, uint64_t>& GetTable() {return m_stat;}

	inline void ResetHitNum(const VideoIndex& vi)
	{
		auto statiter = m_stat.find(vi);
		if(statiter != m_stat.end())
			statiter->second = 0;
	}

protected:
  virtual void DoDispose (void); ///< @brief Do cleanup

  virtual void NotifyNewAggregate ();
private:
	std::unordered_map<VideoIndex, uint64_t> m_stat;
	Ptr<Node>								 m_node;
};


}
}


#endif
