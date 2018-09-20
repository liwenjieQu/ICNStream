#ifndef NDN_CONTENT_STORE_WITH_TRANSCODE_H
#define NDN_CONTENT_STORE_WITH_TRANSCODE_H

#include "content-store-impl.h"
#include "ns3/object.h"
#include "ns3/node.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/name.h"
#include <map>

namespace ns3 {
namespace ndn {
namespace cs {

template<class Policy>
class ContentStoreTranscoding : public ContentStoreImpl<Policy>
{
public:

	typedef ContentStoreImpl<Policy> base_;
	ContentStoreTranscoding()
	{

	}
	virtual ~ContentStoreTranscoding()
	{

	}
	static TypeId
	GetTypeId ();

	virtual inline bool
	Add(Ptr<const Data> data);

	virtual Ptr<Data>
	LookForTranscoding (Ptr<Interest> interest, uint32_t& delay);

	virtual double
	GetTranscodeRatio();

	virtual double
	GetTranscodeCost(bool useweight, const std::string& br);


public:

	virtual
	void ClearCachedContent();

	virtual
	void SetContentInCache(const std::vector<ns3::ndn::Name>& cs);

	virtual
	void InstallCacheEntity();

private:
	void
	UpdateCostDict(Ptr<const Data> data);

private:
	double m_transratio;
	double m_weight;
	std::map<std::string, std::pair<uint32_t, uint32_t> > m_transcode_cost;

private:
	static LogComponent 	g_log; ///< @brief Logging variable
};

///////////////////////////////////////
//////////Implementation//////////////
//////////////////////////////////////
template<class Policy>
LogComponent
ContentStoreTranscoding< Policy >::g_log = LogComponent (("ndn.cs.Transcode." + Policy::GetName ()).c_str ());


template<class Policy>
TypeId
ContentStoreTranscoding< Policy >::GetTypeId ()
{
  static TypeId tid = TypeId (("ns3::ndn::cs::Transcode::"+Policy::GetName ()).c_str ())
    .SetGroupName ("Ndn")
    .SetParent<base_> ()
    .template AddConstructor< ContentStoreTranscoding< Policy > > ()

	.AddAttribute("TranscodeRatio",
		"The percentage of cache capacity where content would be transcoded before serve",
		StringValue("0.5"),
		MakeDoubleAccessor(&ContentStoreTranscoding<Policy>::m_transratio),
		MakeDoubleChecker<double>())

	.AddAttribute("TransformWeight",
		"The weight of transformation on transcoding cost",
		StringValue("0"),
		MakeDoubleAccessor(&ContentStoreTranscoding<Policy>::m_weight),
		MakeDoubleChecker<double>())
    ;
  return tid;
}
template<class Policy>
void ContentStoreTranscoding<Policy>::SetContentInCache(const std::vector<ns3::ndn::Name>& cs)
{
	ContentStore::SetContentInCache(cs);
	InstallCacheEntity();

}
template<class Policy>
void ContentStoreTranscoding<Policy>::InstallCacheEntity()
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
			NS_LOG_DEBUG("Critical Error: in ContentStoreTranscoding<Policy>::InstallCacheEntity(): fail to insert entry into content store!");
		else
		{
			newEntry->SetTrie(result.first);
			this->m_didAddEntry(newEntry);
		}

	}
}
template<class Policy>
void ContentStoreTranscoding<Policy>::ClearCachedContent()
{
	ContentStore::ClearCachedContent();
	this->inCacheRun.clear();
	base_::super::clear();
}

template<class Policy>
bool ContentStoreTranscoding<Policy>::Add(Ptr<const Data> data) {
	if(this->m_updateflag)
	{
		UpdateCostDict(data);
		return ContentStoreImpl<Policy>::Add(data);
	}
	else
		return false;
}

template<class Policy>
void ContentStoreTranscoding<Policy>::UpdateCostDict(Ptr<const Data> data) {
	std::string br = data->GetName().get(-3).toUri();

	auto iter = m_transcode_cost.find(br);
	if(iter == m_transcode_cost.end())
	{
		std::pair<uint32_t, uint32_t> count(0,0);
		m_transcode_cost.insert(std::make_pair(br, count));
		iter = m_transcode_cost.find(br);
	}

	double totaldelay = 0;
	Time lasthopdelay = Simulator::Now() - data->GetTimestamp();
	totaldelay += lasthopdelay.ToInteger(Time::MS);

	for(uint32_t hop = 1; hop <= data->GetHops(); hop++)
	{
		totaldelay += data->GetDelay(hop - 1);
	}

	iter->second.first += 1;
	iter->second.second += totaldelay;
}

template<class Policy>
Ptr<Data> ContentStoreTranscoding<Policy>::LookForTranscoding (Ptr<Interest> interest, uint32_t& delay)
{
	if(!(this->m_updateflag))//safe guard
	{
		Ptr<Node> nodeptr = this->m_node;
		Ptr<NDNBitRate> ndnbr = nodeptr->GetObject<NDNBitRate> ();
		uint32_t totalBR = ndnbr->GetTableSize();
		std::string highestBR = ndnbr->GetBRFromRank(totalBR);

		Ptr<Name> searchname = Create<Name>(interest->GetName().begin(), interest->GetName().begin()+1); //Copy the prefix from original interest
		searchname->append("br" + highestBR);
		searchname->appendNumber(interest->GetName().get(-2).toNumber());
		searchname->appendNumber(interest->GetName().get(-1).toNumber());

		typename base_::super::const_iterator node;
		if (interest->GetExclude() == 0) {
			node = this->deepest_prefix_match(*searchname);
		} else {
			node = this->deepest_prefix_match_if_next_level(*searchname,
					isNotExcluded(*interest->GetExclude()));
		}

		if (node != this->end()) //Transcode is feasible
		{
			//Find the transcoding delay
			std::string originBR = interest->GetName().get(-3).toUri();
			auto iter = m_transcode_cost.find(originBR);
			if(iter != m_transcode_cost.end())
				delay = (m_weight * iter->second.second) / iter->second.first;

			Ptr<Data> copy = Create<Data>(Create<Packet>(ndnbr->GetChunkSize(this->ExtractBitrate(interest->GetName())) *1e3));
			copy->SetName(interest->GetName());

			return copy;
		}
	}
	return 0;
}

template<class Policy>
double ContentStoreTranscoding<Policy>::GetTranscodeRatio()
{
	return m_transratio;
}

template<class Policy>
double ContentStoreTranscoding<Policy>::GetTranscodeCost(bool useweight, const std::string& br)
{
	auto iter = m_transcode_cost.find(br);
	if(iter != m_transcode_cost.end())
	{
		double r = static_cast<double>(iter->second.second) / iter->second.first;
		return (useweight) ? (r * m_weight) : r;
	}
	else
		return 0;
}

}
}
}

#endif
