/*
 * Added by Wenjie Li
 * Implement the ContentStore for DASCache and StreamCache caching schemes.
 *
 * Published in ICC'15 and ICC'16.
 * Optimizing the average throughput (access delay per bit) for users
 * Improving the general quality of adaptive video streaming
 *
 * Period: 2014-09~~~2015-11
 */
#ifndef NDN_CONTENT_STORE_WITH_PROBCACHE_H
#define NDN_CONTENT_STORE_WITH_PROBCACHE_H

#include "content-store-impl.h"
#include "ns3/object.h"
#include "ns3/node.h"

namespace ns3 {
namespace ndn {
namespace cs {

template<class Policy>
class ContentStoreWithProbCache : public ContentStoreImpl<Policy>
{
public:

	typedef ContentStoreImpl<Policy> base_;
	ContentStoreWithProbCache()
	{

	}
	virtual ~ContentStoreWithProbCache()
	{

	}
	static TypeId
	GetTypeId ();

	virtual inline bool
	Add(Ptr<const Data> data);

private:
  static LogComponent 	g_log; ///< @brief Logging variable
};

///////////////////////////////////////
//////////Implementation//////////////
//////////////////////////////////////


template<class Policy>
LogComponent
ContentStoreWithProbCache< Policy >::g_log = LogComponent (("ndn.cs.ProbCache." + Policy::GetName ()).c_str ());


template<class Policy>
TypeId
ContentStoreWithProbCache< Policy >::GetTypeId ()
{
  static TypeId tid = TypeId (("ns3::ndn::cs::ProbCache::"+Policy::GetName ()).c_str ())
    .SetGroupName ("Ndn")
    .SetParent<base_> ()
    .template AddConstructor< ContentStoreWithProbCache< Policy > > ()

    ;
  return tid;
}

template<class Policy>
bool ContentStoreWithProbCache<Policy>::Add(Ptr<const Data> data) {
	NS_LOG_FUNCTION(this << data->GetName ());

	Ptr<typename base_::entry> newEntry = Create<typename base_::entry>(this, data);
	//Check whether this entry should be cached according to pre-determined configuration

	if (Simulator::Now() <= this->m_period)
	{
		bool go_on = true;
		//double avgn = ((data->GetTSI() - 1) * m_nonedgesize + m_edgesize) / static_cast<double>(data->GetTSI());
		double avgn = static_cast<double>(base_::GetMaxSize());
		double p = (static_cast<double>(data->GetAcc()) / (this->m_timein * avgn)) * (static_cast<double>(data->GetTSB()) / data->GetTSI());
		UniformVariable prob(0.0, 1.0);
		if(prob.GetValue() > p)
			go_on = false;

		if(go_on)
		{
			std::pair<typename base_::super::iterator, bool> result = base_::super::insert(
					data->GetName(), newEntry, data->GetPayload()->GetSize());
			if (result.first != base_::super::end()) {
				if (result.second) {
					newEntry->SetTrie(result.first);

					this->m_didAddEntry(newEntry);
					return true;
				}
			}
		}
	}
	return false;
}

}
}
}
#endif
