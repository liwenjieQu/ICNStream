/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011,2012 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *         Ilya Moiseenko <iliamo@cs.ucla.edu>
 *         
 */

#include "ndn-content-store.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/ndn-name.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"

#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/boolean.h"

#include <cmath>

NS_LOG_COMPONENT_DEFINE ("ndn.cs.ContentStore");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (ContentStore);

TypeId
ContentStore::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ContentStore")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()

	.AddAttribute("StartMeasureTime",
			"The length of simulation time during which the performance is measured",
			StringValue("400s"),
			MakeTimeAccessor(&ContentStore::m_period),
			MakeTimeChecker())

	.AddAttribute("RewardUnit",
			"Reward increment",
			StringValue("1"),
			MakeDoubleAccessor(&ContentStore::m_unit),
			MakeDoubleChecker<double>())

	.AddAttribute("RewardDesign",
			"Reward Design Choice (Pattern)",
			StringValue("1"),
			MakeUintegerAccessor(&ContentStore::m_design),
			MakeUintegerChecker<uint32_t>())

	.AddAttribute ("NoCachePartition", "",
			BooleanValue (true),
			MakeBooleanAccessor (&ContentStore::m_noCachePartition),
			MakeBooleanChecker ())

	.AddAttribute("TimeIn",
			"Parameter for ProbCache only",
			StringValue("10"),
			MakeDoubleAccessor(&ContentStore::m_timein),
			MakeDoubleChecker<double>())

    .AddTraceSource ("CacheHits", "Trace called every time there is a cache hit",
                     MakeTraceSourceAccessor (&ContentStore::m_cacheHitsTrace))

    .AddTraceSource ("CacheMisses", "Trace called every time there is a cache miss",
                     MakeTraceSourceAccessor (&ContentStore::m_cacheMissesTrace))

	.AddTraceSource ("CacheReward", "Trace called to record reward for each Interest",
					 MakeTraceSourceAccessor (&ContentStore::m_rewardTrace))

	.AddTraceSource ("CachePartition", "Trace called to record partition status of cache capacity",
					MakeTraceSourceAccessor (&ContentStore::m_cachePartitionTrace))
    ;

  return tid;
}
ContentStore::ContentStore()
{
	m_enableRecord = false;
	m_updateflag = true;
	m_transition = 1;
}

ContentStore::~ContentStore () 
{
}

void ContentStore::FillinCacheRun()
{

}

void ContentStore::FillinCacheRun(std::vector<ns3::ndn::Name>& v)
{

}

void ContentStore::InstallCacheEntity()
{

}
void ContentStore::ClearCachedContent()
{
	this->inCacheConfig.clear();
}
void
ContentStore::AdjustSectionRatio(const std::string& inc, const std::string& dec, double granularity)
{

}

bool
ContentStore::SetSectionRatio(const std::string BR[], double ratio[], size_t num)
{
	return true;
}

void
ContentStore::InitContentStore(const std::string BR[], size_t numBitrates)
{
	m_reward = 0;

}

double
ContentStore::GetSectionRatio(const std::string& BR)
{
	return 0;
}

std::string ContentStore::ExtractBitrate(const Name& n)
{
	std::string name = n.toUri();
	std::size_t found = name.find("br");
	std::string r = name;
	if (found != std::string::npos)
	{
		std::size_t format = name.find("/",found + 2);
		if(format != std::string::npos)
			r = name.substr(found + 2, format - found - 2);
	}
	return r;
}

Ptr<Data>
ContentStore::Lookup (Ptr<const Interest> interest, double& r)
{
	return this->Lookup(interest);
}

Ptr<Data>
ContentStore::LookForTranscoding (Ptr<Interest> interest, uint32_t& delay)
{
	return 0;
}

double
ContentStore::CalReward(Ptr<Interest> interest, bool cachehit)
{
	NS_LOG_FUNCTION(this << interest->GetName ());
	uint64_t bd = interest->GetBRBoundary();
	uint8_t BD = static_cast<uint8_t>(bd) & 0x0f;
	bd = bd >> 4;
	interest->SetBRBoundary(bd);

	uint32_t actualBR = m_BRinfo->GetRankFromBR(ExtractBitrate(interest->GetName()));
	//double p = 1 / static_cast<double>(actualBR);
	//if(interest->GetWindow() == 1)
		//p = 0;
	//uint32_t PRank = interest->GetProducerRank();
	double r = RewardRule(BD, actualBR, cachehit);

	if((Simulator::Now().Compare(m_period) >= 0 && m_noCachePartition)
		|| m_enableRecord)
	{
		this->m_rewardTrace(m_transition, ExtractBitrate(interest->GetName()), r);
	}
	return r;
}
double
ContentStore::RewardRule(uint32_t ringRB, uint32_t reqBR, bool cachehit)
{

	double r = 0;
	if(cachehit)
	{
		if(ringRB == 0)
			r = m_BRinfo->GetRewardFromRank(1);
		else
		{
			if(ringRB > reqBR)
			{

				double newrank = reqBR + m_unit;
				r = m_BRinfo->GetRewardFromRank(reqBR+1) * (1 / newrank)
				    + m_BRinfo->GetRewardFromRank(reqBR) * (1 - 1 / newrank);

				//r = m_BRinfo->GetRewardFromRank(reqBR+1);
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
	}
	return r;
}

void ContentStore::ClearCachePartition()
{

}
void ContentStore::ReportPartitionStatus()
{

}

uint32_t ContentStore::GetCurrentSize() const
{
	return 0;
}

double ContentStore::GetTranscodeRatio()
{
	return 0;
}

double ContentStore::GetTranscodeCost(bool useweight, const std::string& br)
{
	return 0;
}

void ContentStore::DoDispose ()
{
	m_node = 0;
	m_BRinfo = 0;
	Object::DoDispose ();
}

void ContentStore::NotifyNewAggregate ()
{
  if (m_node == 0)
    {
      m_node = GetObject<Node>();
    }

  if(m_BRinfo == 0)
  {
	  m_BRinfo = GetObject<NDNBitRate>();
  }
  Object::NotifyNewAggregate ();
}

namespace cs {

//////////////////////////////////////////////////////////////////////

Entry::Entry (Ptr<ContentStore> cs, Ptr<const Data> data)
  : m_cs (cs)
  , m_data (data)
{
}

const Name&
Entry::GetName () const
{
  return m_data->GetName ();
}

Ptr<const Data>
Entry::GetData () const
{
  return m_data;
}

Ptr<ContentStore>
Entry::GetContentStore ()
{
  return m_cs;
}


} // namespace cs
} // namespace ndn
} // namespace ns3
