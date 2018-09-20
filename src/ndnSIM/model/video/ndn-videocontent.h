/*
 * Author by Wenjie Li
 * Date: 2015-10-06
 */

#ifndef NDN_VIDEOCONTENT_H
#define NDN_VIDEOCONTENT_H

#include <string>
#include "boost/functional/hash.hpp"
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/name.h"

namespace ns3{
namespace ndn{

class VideoIndex
{
public:
	VideoIndex(){}
	VideoIndex(const std::string s, uint32_t f, uint32_t k)
	{
		m_bitrate = s;
		m_file = f;
		m_chunk = k;
	}
	VideoIndex(const VideoIndex& vi)
	{
		m_bitrate = vi.m_bitrate;
		m_file = vi.m_file;
		m_chunk = vi.m_chunk;
	}
	VideoIndex(const Name& requestName)
	{
		m_chunk = requestName.get(-1).toNumber();
		m_file = requestName.get(-2).toNumber();
		m_bitrate = requestName.get(-3).toUri();
	}
	VideoIndex& operator=(const VideoIndex& vi)
	{
		this->m_bitrate = vi.m_bitrate;
		this->m_chunk = vi.m_chunk;
		this->m_file = vi.m_file;
		return (*this);
	}
	inline void Set(const std::string s, uint32_t f, uint32_t k)
	{
		m_bitrate = s;
		m_file = f;
		m_chunk = k;
	}
	inline void Set(const Name& requestName)
	{
		m_chunk = requestName.get(-1).toNumber();
		m_file = requestName.get(-2).toNumber();
		m_bitrate = requestName.get(-3).toUri();
	}
	inline bool operator==(const VideoIndex& index) const{
		return (m_bitrate == index.m_bitrate) &&
				(m_file == index.m_file) &&
				(m_chunk == index.m_chunk);
	}
	bool operator<(const VideoIndex& index) const{
		return (m_file < index.m_file) ||
				(m_file ==  index.m_file && m_bitrate < index.m_bitrate) ||
				(m_file ==  index.m_file && m_bitrate == index.m_bitrate && m_chunk < index.m_chunk);
	}

	std::string m_bitrate;
	uint32_t m_file;
	uint32_t m_chunk;
};

class ValuedVideoIndex : public VideoIndex
{
public:
	ValuedVideoIndex(uint32_t f, uint32_t k , std::string s, double u)
		:VideoIndex(s, f, k)
	{
		m_cacheutility = u;
	}
	virtual ~ValuedVideoIndex()
	{

	}
	inline double GetUtility() {return m_cacheutility;}

	inline bool operator<(const ValuedVideoIndex& other)
	{
		return this->m_cacheutility < other.m_cacheutility;
	}
	double m_cacheutility;
};

class PartitionVideoIndex : public VideoIndex
{
public:
	PartitionVideoIndex(uint32_t rank, uint32_t f, uint32_t k , std::string s)
		:VideoIndex(s, f, k)
	{
		m_BRrank = rank;
	}
	PartitionVideoIndex(uint32_t rank, const VideoIndex& vi)
		:VideoIndex(vi)
	{
		m_BRrank = rank;
	}
	virtual ~PartitionVideoIndex()
	{

	}
	PartitionVideoIndex& operator=(const PartitionVideoIndex& vi)
	{
		this->m_bitrate = vi.m_bitrate;
		this->m_chunk = vi.m_chunk;
		this->m_file = vi.m_file;
		this->m_BRrank = vi.m_BRrank;
		return (*this);
	}
	inline bool operator==(const PartitionVideoIndex& index) const{
		return (m_bitrate == index.m_bitrate) &&
				(m_file == index.m_file) &&
				(m_chunk == index.m_chunk) &&
				(m_BRrank == index.m_BRrank);
	}
	uint32_t m_BRrank;
};

class DecisionIndex
{
public:
	DecisionIndex(const std::string s, uint32_t f)
	{
		m_bitrate = s;
		m_file = f;
	}
	inline bool operator==(const DecisionIndex& index) const{
		return (m_bitrate == index.m_bitrate) &&
				(m_file == index.m_file);
	}

	std::string m_bitrate;
	uint32_t m_file;
};

class VarIndex
{
public:
	VarIndex(uint32_t id, const VideoIndex& vi)
	{
		m_id = id;
		m_vi = vi;
	}
	inline bool operator==(const VarIndex& index) const{
		return (m_id == index.m_id) &&
				(m_vi == index.m_vi);
	}

	uint32_t 	m_id;
	VideoIndex	m_vi;
};

class ContentIndex
{
public:
	ContentIndex(){}
	ContentIndex(uint32_t f, uint32_t k)
	{
		m_file = f;
		m_chunk = k;
	}
	ContentIndex& operator=(const ContentIndex& ci)
	{
		this->m_chunk = ci.m_chunk;
		this->m_file = ci.m_file;
		return (*this);
	}
	inline bool operator==(const ContentIndex& index) const{
		return  (m_file == index.m_file) &&
				(m_chunk == index.m_chunk);
	}
	bool operator<(const ContentIndex& index) const{
		return  (m_file < index.m_file) ||
				(m_file ==  index.m_file && m_chunk < index.m_chunk);
	}
	uint32_t m_file;
	uint32_t m_chunk;
};


class PathVarIndex
{
public:
	PathVarIndex(uint32_t edgeid, uint32_t endid, const VideoIndex& vi)
	{
		m_edgeid = edgeid;
		m_endid = endid;
		m_vi = vi;
	}
	inline bool operator==(const PathVarIndex& index) const{
		return (m_edgeid == index.m_edgeid) &&
				(m_endid == index.m_endid) &&
				(m_vi == index.m_vi);
	}

	uint32_t 	m_edgeid;
	uint32_t 	m_endid;
	VideoIndex	m_vi;
};

class DelayTableKey
{
public:
	DelayTableKey(const std::string& br, uint32_t hop, uint32_t edgeid)
	{
		m_edgeid = edgeid;
		m_hop = hop;
		m_br = br;
	}
	inline bool operator==(const DelayTableKey& index) const{
		return (m_edgeid == index.m_edgeid) &&
				(m_hop == index.m_hop) &&
				(m_br == index.m_br);
	}
	uint32_t 	m_edgeid;
	uint32_t 	m_hop;
	std::string	m_br;
};

}
}

namespace std {
	template<>
	struct hash<ns3::ndn::DecisionIndex>
	{
		std::size_t operator()(const ns3::ndn::DecisionIndex& index) const{
			std::size_t seed = 0;
			boost::hash_combine(seed, boost::hash_value(index.m_file));
			boost::hash_combine(seed, boost::hash_value(index.m_bitrate));
			return seed;
			//return std::hash<uint32_t>()(cache.m_video_file) ^ std::hash<uint32_t>()(cache.m_video_chunk) ^ std::hash<uint32_t>()(cache.m_bitrate);
		}
	};

	template<>
	struct hash<ns3::ndn::VideoIndex>
	{
		std::size_t operator()(const ns3::ndn::VideoIndex& index) const{
			std::size_t seed = 0;
			boost::hash_combine(seed, boost::hash_value(index.m_chunk));
			boost::hash_combine(seed, boost::hash_value(index.m_file));
			boost::hash_combine(seed, boost::hash_value(index.m_bitrate));
			return seed;
			//return std::hash<uint32_t>()(cache.m_video_file) ^ std::hash<uint32_t>()(cache.m_video_chunk) ^ std::hash<uint32_t>()(cache.m_bitrate);
		}
	};

	template<>
	struct hash<ns3::ndn::PartitionVideoIndex>
	{
		std::size_t operator()(const ns3::ndn::PartitionVideoIndex& index) const{
			std::size_t seed = 0;
			boost::hash_combine(seed, boost::hash_value(index.m_chunk));
			boost::hash_combine(seed, boost::hash_value(index.m_file));
			boost::hash_combine(seed, boost::hash_value(index.m_bitrate));
			boost::hash_combine(seed, boost::hash_value(index.m_BRrank));
			return seed;
			//return std::hash<uint32_t>()(cache.m_video_file) ^ std::hash<uint32_t>()(cache.m_video_chunk) ^ std::hash<uint32_t>()(cache.m_bitrate);
		}
	};

	template<>
	struct hash<ns3::ndn::ContentIndex>
	{
		std::size_t operator()(const ns3::ndn::ContentIndex& index) const{
			std::size_t seed = 0;
			boost::hash_combine(seed, boost::hash_value(index.m_chunk));
			boost::hash_combine(seed, boost::hash_value(index.m_file));
			return seed;
			//return std::hash<uint32_t>()(cache.m_video_file) ^ std::hash<uint32_t>()(cache.m_video_chunk) ^ std::hash<uint32_t>()(cache.m_bitrate);
		}
	};

	template<>
	struct hash<ns3::ndn::VarIndex>
	{
		std::size_t operator()(const ns3::ndn::VarIndex& index) const{
			std::size_t seed = 0;
			boost::hash_combine(seed, boost::hash_value(index.m_id));
			boost::hash_combine(seed, boost::hash_value(index.m_vi.m_file));
			boost::hash_combine(seed, boost::hash_value(index.m_vi.m_chunk));
			boost::hash_combine(seed, boost::hash_value(index.m_vi.m_bitrate));
			return seed;
		}
	};

	template<>
	struct hash<ns3::ndn::PathVarIndex>
	{
		std::size_t operator()(const ns3::ndn::PathVarIndex& index) const{
			std::size_t seed = 0;
			boost::hash_combine(seed, boost::hash_value(index.m_edgeid));
			boost::hash_combine(seed, boost::hash_value(index.m_endid));
			boost::hash_combine(seed, boost::hash_value(index.m_vi.m_file));
			boost::hash_combine(seed, boost::hash_value(index.m_vi.m_chunk));
			boost::hash_combine(seed, boost::hash_value(index.m_vi.m_bitrate));
			return seed;
		}
	};

	template<>
	struct hash<ns3::ndn::DelayTableKey>
	{
		std::size_t operator()(const ns3::ndn::DelayTableKey& index) const{
			std::size_t seed = 0;
			boost::hash_combine(seed, boost::hash_value(index.m_edgeid));
			boost::hash_combine(seed, boost::hash_value(index.m_hop));
			boost::hash_combine(seed, boost::hash_value(index.m_br));
			return seed;
		}
	};
}

#endif /*ndn-videocontent.h*/
