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

#include "content-store-nocache.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/ndn-name.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/nstime.h"

NS_LOG_COMPONENT_DEFINE ("ndn.cs.Nocache");

namespace ns3 {
namespace ndn {
namespace cs {

NS_OBJECT_ENSURE_REGISTERED (Nocache);

TypeId 
Nocache::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::cs::Nocache")
    .SetGroupName ("Ndn")
    .SetParent<ContentStore> ()
    .AddConstructor< Nocache > ()
    ;

  return tid;
}

Nocache::Nocache ()
{
}

Nocache::~Nocache () 
{
}

Ptr<Data>
Nocache::Lookup (Ptr<const Interest> interest)
{
  this->m_cacheMissesTrace (interest);
  return 0;
}

bool
Nocache::Add (Ptr<const Data> data)
{
  return false;
}

void
Nocache::Print (std::ostream &os) const
{
}

uint32_t
Nocache::GetSize () const
{
  return 0;
}

Ptr<cs::Entry>
Nocache::Begin ()
{
  return 0;
}

Ptr<cs::Entry>
Nocache::End ()
{
  return 0;
}

Ptr<cs::Entry>
Nocache::Next (Ptr<cs::Entry>)
{
  return 0;
}


Ptr<Data>
Nocache::Lookup (Ptr<const Interest> interest, double& r)
{
	NS_LOG_DEBUG("[CS (NO Cache)]: Node: " << m_node->GetId() << " IN LOOKUP!");
	this->m_cacheMissesTrace (interest);
	return 0;
}

/*
double
Nocache::CalReward(Ptr<Interest> interest, bool cachehit)
{
	// Policy needs to be adjusted based on the simulation result.
	// For video producer, same reward rule is applied
	if(m_node->GetId() == 0)// Root is the video producer
	{
		NS_LOG_DEBUG("[CS (NO Cache)]: Node: " << m_node->GetId() << " Call ContentStore::CalReward");
		return ContentStore::CalReward(interest, cachehit);
	}
	else
	{
		NS_LOG_DEBUG("[CS (NO Cache)]: Node: " << m_node->GetId() << " Not Calling CalReward!");
		return 0;
	}
}
*/
/*
>>>>>>> MergeWithTMM
void
Nocache::ReportPartitionStatus()
{
	if(Simulator::Now() > m_period)
	{
		if(m_node->GetId() == 0){
		uint32_t numBitRate = m_BRinfo->GetTableSize();
		std::shared_ptr<double> sp(new double [numBitRate],
					std::default_delete<double[]>());
		for(uint32_t i = 0; i < numBitRate; i++)
			sp.get()[i] = 0;

		m_cachePartitionTrace(m_transition, m_reward, sp);
		m_transition++;
		ResetReward();
		}
	}
	Simulator::Schedule(Time(Seconds(600)), &Nocache::ReportPartitionStatus, this);
}
<<<<<<< HEAD

=======
*/

} // namespace cs
} // namespace ndn
} // namespace ns3
