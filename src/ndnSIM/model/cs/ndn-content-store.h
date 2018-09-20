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
 */

#ifndef NDN_CONTENT_STORE_H
#define	NDN_CONTENT_STORE_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/node.h"
#include "ns3/ndn-data.h"
#include "ns3/name.h"
#include "ns3/ndn-bitrate.h"

#include <boost/tuple/tuple.hpp>
#include <vector>
#include <memory>

namespace ns3 {

class Packet;

namespace ndn {

class Interest;
class Name;
class ContentStore;

/**
 * @ingroup ndn
 * @defgroup ndn-cs Content Store
 */

/**
 * @ingroup ndn-cs
 * @brief Namespace for ContentStore operations
 */
namespace cs {

/**
 * @ingroup ndn-cs
 * @brief NDN content store entry
 */
class Entry : public SimpleRefCount<Entry>
{
public:
  /**
   * \brief Construct content store entry
   *
   * \param header Parsed Data header
   * \param packet Original Ndn packet
   *
   * The constructor will make a copy of the supplied packet and calls
   * RemoveHeader and RemoveTail on the copy.
   */
  Entry (Ptr<ContentStore> cs, Ptr<const Data> data);

  /**
   * \brief Get prefix of the stored entry
   * \returns prefix of the stored entry
   */
  const Name&
  GetName () const;

  /**
   * \brief Get Data of the stored entry
   * \returns Data of the stored entry
   */
  Ptr<const Data>
  GetData () const;

  inline
  uint32_t GetSize()
  {
	  return m_data->GetPayload()->GetSize();
  };

  /**
   * @brief Get pointer to access store, to which this entry is added
   */
  Ptr<ContentStore>
  GetContentStore ();

private:
  Ptr<ContentStore> m_cs; ///< \brief content store to which entry is added
  Ptr<const Data> m_data; ///< \brief non-modifiable Data
};

} // namespace cs


/**
 * @ingroup ndn-cs
 * \brief Base class for NDN content store
 *
 * Particular implementations should implement Lookup, Add, and Print methods
 */
class ContentStore : public Object
{
public:
  /**
   * \brief Interface ID
   *
   * \return interface ID
   */
  static
  TypeId GetTypeId ();

  ContentStore();

  /**
   * @brief Virtual destructor
   */
  virtual
  ~ContentStore ();

  /**
   * \brief Find corresponding CS entry for the given interest
   *
   * \param interest Interest for which matching content store entry
   * will be searched
   *
   * If an entry is found, it is promoted to the top of most recent
   * used entries index, \see m_contentStore
   */
  virtual Ptr<Data>
  Lookup (Ptr<const Interest> interest) = 0;

  virtual Ptr<Data>
  Lookup (Ptr<const Interest> interest, double& r);

  virtual Ptr<Data>
  LookForTranscoding (Ptr<Interest> interest, uint32_t& delay);

  /**
   * \brief Add a new content to the content store.
   *
   * \param header Fully parsed Data
   * \param packet Fully formed Ndn packet to add to content store
   * (will be copied and stripped down of headers)
   * @returns true if an existing entry was updated, false otherwise
   */
  virtual bool
  Add (Ptr<const Data> data) = 0;

  // /*
  //  * \brief Add a new content to the content store.
  //  *
  //  * \param header Interest header for which an entry should be removed
  //  * @returns true if an existing entry was removed, false otherwise
  //  */
  // virtual bool
  // Remove (Ptr<Interest> header) = 0;

  /**
   * \brief Print out content store entries
   */
  virtual void
  Print (std::ostream &os) const = 0;


  /**
   * @brief Get number of entries in content store
   */
  virtual uint32_t
  GetSize () const = 0;

  /**
   * @brief Return first element of content store (no order guaranteed)
   */
  virtual Ptr<cs::Entry>
  Begin () = 0;

  /**
   * @brief Return item next after last (no order guaranteed)
   */
  virtual Ptr<cs::Entry>
  End () = 0;

  /**
   * @brief Advance the iterator
   */
  virtual Ptr<cs::Entry>
  Next (Ptr<cs::Entry>) = 0;

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////

  /**
   * @brief Static call to cheat python bindings
   */
  static inline Ptr<ContentStore>
  GetContentStore (Ptr<Object> node);

  //APIs added to implement functionality of DASCache
  //////////////////////////////////////////////////////////////////////////////
  //Fill the ContentStore with the Optimization result achieved by DASCache
  virtual inline
  void SetContentInCache(const std::vector<ns3::ndn::Name>& cs)
  {
	  inCacheConfig = cs;
  }
  //Return the current ContentStore
  inline
  std::vector<ns3::ndn::Name>& GetContentInCache()
  {
	  return inCacheRun;
  }
  //Assign values to inCacheRun vector (the contents in current ContentStore)
  virtual
  void FillinCacheRun();

  virtual
  void FillinCacheRun(std::vector<ns3::ndn::Name>&);
  //////////////////////////////////////////////////////////////////////////////

  //APIs added to implement functionality of StreamCache
  //////////////////////////////////////////////////////////////////////////////
  //Put the contents in inCacheConfig vector into ContentStore
  virtual
  void InstallCacheEntity();

  //Clear ContentStore and inCacheConfig and inCacheRun vectors
  virtual
  void ClearCachedContent();
  //////////////////////////////////////////////////////////////////////////////

  virtual uint32_t GetCapacity()
  {
	  return 0;
  }
  virtual uint32_t GetCapacity(const std::string& s)
  {
	  return 0;
  }
  std::vector<ns3::ndn::Name> 	inCacheConfig;	//Only used for DASCache. Use to store the optimization result from offline computing
  std::vector<ns3::ndn::Name> 	inCacheRun;		//For installing FIB entries based on current cache status

  //Added to implement DFA Transition (Deserted)
public:
    virtual uint32_t GetCurrentSize() const;
    virtual inline bool RemoveByMDP(Ptr<Name> name)
    {
    	return true;
    }
    virtual inline bool AddByMDP(Ptr<Name> name)
    {
    	return true;
    }

    //Added to implement cache multisection
public:
	virtual void
	AdjustSectionRatio(const std::string&, const std::string&, double = 0.05);

	virtual bool
	SetSectionRatio(const std::string[], double[], size_t);

	virtual bool
	SetSectionRatio(double[], size_t){ return true; };

	virtual void
	InitContentStore(const std::string[], size_t);

	virtual inline double
	GetReward()
	{
		return 0;
	}

	inline void
	AddReward(double r)
	{
		m_reward += r;
		m_numreq++;
	}

	inline void
	ResetReward()
	{
		m_reward = 0;
		m_numreq = 0;
	}

	virtual double
	GetSectionRatio(const std::string&);

	std::string
	ExtractBitrate(const Name& n);

	inline void EnableRecord() { m_enableRecord = true; };

	virtual void ReportPartitionStatus();
	virtual void ClearCachePartition();

	inline void DisableCSUpdate()
	{
		m_updateflag = false;
	}

	inline void EnableCSUpdate()
	{
		m_updateflag = true;
	}

	virtual double
	GetTranscodeRatio();

	virtual double
	GetTranscodeCost(bool useweight, const std::string& br);

protected:

	bool				m_enableRecord = false;
	bool				m_noCachePartition;
	bool				m_updateflag = true; // Update Flag indicates whether CS can insert/delete as normal. When m_updateflag=false, Add() is disabled;
	uint32_t			m_transition = 1;


	double				m_reward;
	uint32_t 			m_numreq;

	Time 				m_period;
	uint32_t			m_design;
	double				m_unit;
	double				m_timein;
protected:

	double
	CalReward(Ptr<Interest> interest, bool cachehit);

	double
	RewardRule(uint32_t refBR, uint32_t realBR, bool cachehit);

	TracedCallback<Ptr<const Interest>,
                 Ptr<const Data> > m_cacheHitsTrace; ///< @brief trace of cache hits

	TracedCallback<Ptr<const Interest> > m_cacheMissesTrace; ///< @brief trace of cache misses

	TracedCallback<uint32_t, const std::string&, double> m_rewardTrace; /// Trace of local cache reward (different from m_reward)

	TracedCallback<uint32_t, double, std::shared_ptr<double> > m_cachePartitionTrace; /// Trace of cache partition status

	virtual void NotifyNewAggregate (); ///< @brief Even when object is aggregated to another Object
	virtual void DoDispose (); ///< @brief Do cleanup

	Ptr<Node> m_node;
	Ptr<NDNBitRate> m_BRinfo;
};

inline std::ostream&
operator<< (std::ostream &os, const ContentStore &cs)
{
  cs.Print (os);
  return os;
}

inline Ptr<ContentStore>
ContentStore::GetContentStore (Ptr<Object> node)
{
  return node->GetObject<ContentStore> ();
}


} // namespace ndn
} // namespace ns3

#endif // NDN_CONTENT_STORE_H
