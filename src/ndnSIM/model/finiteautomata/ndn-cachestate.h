/*
 * Author: Wenjie Li
 * Date: 2016-3-25
 */


#ifndef NDN_CACHESTATE_H
#define NDN_CACHESTATE_H

#include <string>
#include <map>
#include <utility>

#include "ns3/ndn-videocontent.h"
#include "boost/functional/hash.hpp"

namespace ns3{
namespace ndn{

class Block
{
public:
	Block();
	bool operator==(const Block&) const;
public:
	/*
	 * m_start and m_end marks the starting and ending ID of a continuous video block
	 */
	std::map<std::string, uint32_t> m_start; //starting from ChunkID 1 to maxLen
	std::map<std::string, uint32_t> m_end;
};


class RecordByFile
{
public:
	RecordByFile();
	bool operator==(const RecordByFile&) const;
public:
	// File id is used as the key
	std::map<uint32_t, Block> m_filedict;
};


class MDPState
{
public:
	using BlockRange = std::pair<uint32_t, uint32_t>;
	/*
	 * Initial MDP with ID set by the DFA environment
	 */

	MDPState(uint32_t id, uint32_t len);

	inline uint32_t GetID() const
	{
		return m_stateid;
	}
	const RecordByFile& GetStatus() const
	{
		return m_status;
	}

	inline uint32_t GetSize() const
	{
		return m_size;
	}

	/*
	 * Add and Remove functions update the m_size
	 */
	bool AddContent (const VideoIndex&);
	bool RemoveContent (const VideoIndex&);

	bool operator==(const MDPState& s) const;
	BlockRange GetChunkRange(const std::string& bitrate, uint32_t fileID) const;

	bool SatisfyBaseLaw(const VideoIndex&) const;
	bool SatisfyGeneralLaw(const VideoIndex&) const;

	std::map<std::string, uint32_t> 	m_size_perrate;

private:
	uint32_t 			m_stateid = 0;	//ID assigned by the DFA environment
	RecordByFile 		m_status;
	uint32_t 			m_filemaxlen = 0;	//The max length of any video file

	/*
	 * For Fast lookup (used in hash function)
	 */
	uint32_t 			m_size = 0;	//Number of cached video segments
};


}
}

namespace std {
	template<>
	struct hash<ns3::ndn::MDPState>
	{
		std::size_t operator()(const ns3::ndn::MDPState& state) const{
			std::size_t seed = 100;
			boost::hash_combine(seed, boost::hash_value(state.GetSize()));
			auto iter = state.m_size_perrate.begin();
			for(; iter!=state.m_size_perrate.end();iter++)
			{
				boost::hash_combine(seed, boost::hash_value(iter->second));
			}
			return seed;
		}
	};
}


#endif

