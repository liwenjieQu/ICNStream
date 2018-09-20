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
#ifndef NDN_CONTENT_STORE_WITH_ADAPTIVE_VIDEO_H
#define NDN_CONTENT_STORE_WITH_ADAPTIVE_VIDEO_H

#include "content-store-impl.h"
#include "ns3/object.h"
#include "ns3/node.h"

namespace ns3 {
namespace ndn {
namespace cs {

template<class Policy>
class ContentStoreStreaming : public ContentStoreImpl<Policy>
{
public:

	typedef ContentStoreImpl<Policy> base_;
	ContentStoreStreaming()
	{

	}
	virtual ~ContentStoreStreaming()
	{

	}
	static TypeId
	GetTypeId ();

	virtual inline bool
	Add(Ptr<const Data> data);

public:
	virtual
	void FillinCacheRun();

	virtual
	void FillinCacheRun(std::vector<ns3::ndn::Name>&);

	virtual
	void ClearCachedContent();

	virtual
	void SetContentInCache(const std::vector<ns3::ndn::Name>& cs);

	virtual
	void InstallCacheEntity();

	virtual
	void ReportPartitionStatus();

private:
  static LogComponent 	g_log; ///< @brief Logging variable
};

///////////////////////////////////////
//////////Implementation//////////////
//////////////////////////////////////


template<class Policy>
LogComponent
ContentStoreStreaming< Policy >::g_log = LogComponent (("ndn.cs.AdptiveVideo." + Policy::GetName ()).c_str ());


template<class Policy>
TypeId
ContentStoreStreaming< Policy >::GetTypeId ()
{
  static TypeId tid = TypeId (("ns3::ndn::cs::AdptiveVideo::"+Policy::GetName ()).c_str ())
    .SetGroupName ("Ndn")
    .SetParent<base_> ()
    .template AddConstructor< ContentStoreStreaming< Policy > > ()

    ;
  return tid;
}

template<class Policy>
void ContentStoreStreaming<Policy>::SetContentInCache(const std::vector<ns3::ndn::Name>& cs)
{
	ContentStore::SetContentInCache(cs);
	InstallCacheEntity();

}
template<class Policy>
void ContentStoreStreaming<Policy>::InstallCacheEntity()
{
	vector<ns3::ndn::Name>::iterator iter = (this->inCacheConfig).begin();
	Ptr<Node> thisnode = this->m_node;
	Ptr<NDNBitRate> ndnbr = thisnode->GetObject<NDNBitRate> ();
	for(; iter != (this->inCacheConfig).end(); iter++)
	{
		double vsegsize = ndnbr->GetChunkSize(this->ExtractBitrate(*iter));
		Ptr<Data> dataPacket =
				Create< Data >(Create<Packet>(vsegsize * 1e3));
		dataPacket->SetName(Create<Name>(*iter));

		Ptr<typename base_::entry> newEntry = Create<typename base_::entry>(this, dataPacket);
		std::pair<typename base_::super::iterator, bool> result = base_::super::insert(dataPacket->GetName(), newEntry);
		if(!result.second)
			NS_LOG_DEBUG("Critical Error: in ContentStoreStreaming<Policy>::InstallCacheEntity(): fail to insert entry into content store!");
		else
		{
			newEntry->SetTrie(result.first);
			this->m_didAddEntry(newEntry);
		}

	}
}
template<class Policy>
void ContentStoreStreaming<Policy>::ClearCachedContent()
{
	ContentStore::ClearCachedContent();
	this->inCacheRun.clear();
	base_::super::clear();
}

template<class Policy>
void ContentStoreStreaming<Policy>::FillinCacheRun()
{
	//this->inCacheRun = this->inCacheConfig;
	this->inCacheRun.clear();
	base_::super::GetCachedName(this->inCacheRun);
}

template<class Policy>
void ContentStoreStreaming<Policy>::FillinCacheRun(std::vector<ns3::ndn::Name>& v)
{
	base_::super::GetCachedName(v);
}

template<class Policy>
void ContentStoreStreaming<Policy>::ReportPartitionStatus()
{

}

/*
template<class Policy>
bool ContentStoreStreaming<Policy>::Add(Ptr<const Data> data) {
	NS_LOG_FUNCTION(this << data->GetName ());

	Ptr<base_::entry> newEntry = Create<base_::entry>(this, data);
	//Check whether this entry should be cached according to pre-determined configuration

	if (Policy::GetName() == "DASCache") {
		return true;
	}
	else if (Policy::GetName() == "Lru" || Policy::GetName() == "Lfu"){
		bool go_on = true;
		if(m_useProbCache)
		{
			//double avgn = ((data->GetTSI() - 1) * m_nonedgesize + m_edgesize) / static_cast<double>(data->GetTSI());
			double avgn = static_cast<double>(GetMaxSize());
			double p = (static_cast<double>(data->GetAcc()) / (10*avgn)) * (static_cast<double>(data->GetTSB()) / data->GetTSI());
			UniformVariable prob(0.0, 1.0);
			if(prob.GetValue() > p)
				go_on = false;
		}
		if(go_on)
		{
			std::pair<typename base_::super::iterator, bool> result = base_::super::insert(
					data->GetName(), newEntry, data->GetPayload()->GetSize());
			if (result.first != base_::super::end()) {
				if (result.second) {
					newEntry->SetTrie(result.first);

					m_didAddEntry(newEntry);
					return true;
				}
			}
		}
	}
	return false;
}
*/

template<class Policy>
bool ContentStoreStreaming<Policy>::Add(Ptr<const Data> data) {
	if(this->m_updateflag)
		return ContentStoreImpl<Policy>::Add(data);
	else
		return false;
}


}
}
}
#endif
