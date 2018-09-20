#include "ndn-cachestate.h"
#include "ns3/log.h"
#include "ns3/ptr.h"

NS_LOG_COMPONENT_DEFINE("ndn.MDPState");
namespace ns3{
namespace ndn{


MDPState::MDPState(uint32_t id, uint32_t len)
	:m_stateid(id),
	 m_filemaxlen(len)
{

}

bool
MDPState::AddContent(const VideoIndex& vc)
{
	bool newfile = false;
	bool newitem = false;
	bool action = false;
	auto fileiter = m_status.m_filedict.find(vc.m_file);
	if (fileiter == m_status.m_filedict.end())
		newfile = true;
	else
	{
		auto j = fileiter->second.m_start.find(vc.m_bitrate);
		auto k = fileiter->second.m_end.find(vc.m_bitrate);
		if(j == fileiter->second.m_start.end())
			newitem = true;
		else if(k->second < m_filemaxlen + 1 && vc.m_chunk == k->second)
		{
			k->second++;
			action= true;
		}
		else if(j->second >= 2 && vc.m_chunk == j->second - 1)
		{
			j->second--;
			action= true;
		}
	}
	if(newfile)
	{
		Block spf;
		spf.m_start[vc.m_bitrate] = 1;
		spf.m_end[vc.m_bitrate] = 2;
		m_status.m_filedict[vc.m_file] = std::move(spf);
		if(vc.m_chunk != 1)
			NS_LOG_WARN("MDPState:AddContent; Adding video segment ID error");
		action= true;
	}
	if(newitem)
	{
		m_status.m_filedict[vc.m_file].m_start[vc.m_bitrate] = 1;
		m_status.m_filedict[vc.m_file].m_end[vc.m_bitrate] = 2;
		if(vc.m_chunk != 1)
			NS_LOG_WARN("MDPState:AddContent; Adding video segment ID error");
		action=true;
	}
	if(action)
	{
		m_size++;
		m_size_perrate[vc.m_bitrate] += 1;
	}
	return action;

}

/*
 * 1.Need to check whether the remove action will destroy the continuous block
 * 2.Clean the map if for a certain bitrate, there is no cached segment
 */
bool
MDPState::RemoveContent(const VideoIndex& vc)
{
	bool normal = true;
	bool action = false;
	auto iter = m_status.m_filedict.find(vc.m_file);
	if(iter == m_status.m_filedict.end())
		normal = false;
	if(normal && iter->second.m_start.find(vc.m_bitrate) == iter->second.m_start.end())
		normal = false;
	if(normal && iter->second.m_end[vc.m_bitrate] > iter->second.m_start[vc.m_bitrate] + 1)
	{
		if(vc.m_chunk == iter->second.m_end[vc.m_bitrate] - 1)
		{
			action = true;
			iter->second.m_end[vc.m_bitrate]--;
		}
		if(vc.m_chunk == iter->second.m_start[vc.m_bitrate])
		{
			action = true;
			iter->second.m_start[vc.m_bitrate]++;
		}
	}
	else if(normal)
	{
		if(vc.m_chunk == iter->second.m_start[vc.m_bitrate])
		{
			action = true;
			iter->second.m_start.erase(vc.m_bitrate);
			iter->second.m_end.erase(vc.m_bitrate);
		}
	}
	if(!normal)
		NS_LOG_ERROR("MDPState:RemoveContent; Removing a video segment which not exists!");
	if(action)
	{
		m_size--;
		m_size_perrate[vc.m_bitrate] -= 1;
	}
	return action;
}

MDPState::BlockRange
MDPState::GetChunkRange(const std::string& bitrate, uint32_t fileID) const
{
	BlockRange range = std::make_pair(0, 0);
	auto iter = m_status.m_filedict.find(fileID);
	if(iter != m_status.m_filedict.end())
	{
		auto j = iter->second.m_start.find(bitrate);
		auto k = iter->second.m_end.find(bitrate);
		if(j != iter->second.m_start.end())
		{
			range.first = j->second;
			range.second = k->second;
		}
	}
	return range;
}

bool
MDPState::operator==(const MDPState& state) const
{
	if(this->m_size != state.GetSize())
		return false;
	else
		return this->m_status == state.GetStatus();
}

bool
MDPState::SatisfyBaseLaw(const VideoIndex& vi) const
{
	bool removable = false;
	auto fileiter = m_status.m_filedict.find(vi.m_file);
	if(fileiter != m_status.m_filedict.end())
	{
		auto j = fileiter->second.m_start.find(vi.m_bitrate);
		auto k = fileiter->second.m_end.find(vi.m_bitrate);
		if(j != fileiter->second.m_start.end())
		{
			if(j->second == vi.m_chunk || (k->second - 1) == vi.m_chunk)
				removable = true;
		}
	}
	return removable;
}

bool
MDPState::SatisfyGeneralLaw(const VideoIndex& vi) const
{
	bool removable = false;
	auto fileiter = m_status.m_filedict.find(vi.m_file);
	if(fileiter != m_status.m_filedict.end())
	{
		auto j = fileiter->second.m_start.find(vi.m_bitrate);
		auto k = fileiter->second.m_end.find(vi.m_bitrate);
		if(j != fileiter->second.m_start.end())
		{
			if(j->second <= vi.m_chunk && k->second > vi.m_chunk)
				removable = true;
		}
	}
	return removable;
}


RecordByFile::RecordByFile()
{

}

bool
RecordByFile::operator==(const RecordByFile& cs) const
{
	if(this->m_filedict.size() != cs.m_filedict.size())
		return false;
	auto iter = this->m_filedict.begin();
	bool result = true;
	for(; iter != this->m_filedict.end(); iter++)
	{
		auto checkiter = cs.m_filedict.find(iter->first);
		if(checkiter == cs.m_filedict.end() || !(iter->second == checkiter->second))
		{
			result = false;
			break;
		}
	}
	return result;
}

Block::Block()
{

}
bool
Block::operator==(const Block& spf) const
{
	if(this->m_start.size() != spf.m_start.size() || this->m_end.size() != spf.m_end.size())
		return false;
	auto iterstart = this->m_start.begin();
	auto iterend = this->m_end.begin();
	bool result = true;
	for(; iterstart != this->m_start.end(); iterstart++)
	{
		auto checkstart = spf.m_start.find(iterstart->first);
		auto checkend = spf.m_end.find(iterend->first);
		if(checkstart == spf.m_start.end() || checkstart->second != iterstart->second || checkend->second != iterend->second)
		{
			result = false;
			break;
		}
		iterend++;
	}
	return result;
}


}
}
