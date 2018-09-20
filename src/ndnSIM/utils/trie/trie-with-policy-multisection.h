/*
 * Author: Wenjie Li (liwenjie@cs.queensu.ca)
 * Create a policy class to handle cache multisection for adaptive video bitrates.
 * Contents with different bitrates are managed by multiple intrusive lists.
 * Each list has its own size limit
 *
 * Created date: 2016-07-12
 */

#ifndef TRIE_WITH_POLICY_MULTISECTION_H
#define TRIE_WITH_POLICY_MULTISECTION_H

#include "trie-with-policy.h"
#include <string>
#include <map>

namespace ns3 {
namespace ndn {
namespace ndnSIM {

template<typename FullKey,
         typename PayloadTraits,
         typename PolicyTraits
         >
class trie_with_policy_multisection
{
public:

	  typedef trie< FullKey,
	                PayloadTraits,
	                typename PolicyTraits::policy_hook_type > parent_trie;

	  typedef typename parent_trie::iterator iterator;
	  typedef typename parent_trie::const_iterator const_iterator;

	  typedef typename PolicyTraits::template policy<
			  trie_with_policy_multisection<FullKey, PayloadTraits, PolicyTraits>,
			  parent_trie,
			  typename PolicyTraits::template container_hook<parent_trie>::type >::type policy_container;

	inline
	trie_with_policy_multisection (size_t bucketSize = 1, size_t bucketIncrement = 1)
		:trie_ (name::Component (), bucketSize, bucketIncrement)
	{

	}

	inline void
	initMaintenanceLists (const std::string BR[], size_t numBitrates)
	{
		for(uint32_t i = 0; i < numBitrates; i++)
		{
			policys_.insert(std::make_pair(BR[i], std::move(*new policy_container(*this, BR[i]))));
		}
	}

	inline std::pair< iterator, bool >
	insert (const FullKey &key, typename PayloadTraits::insert_type payload, std::string br, uint32_t len)
	{
		std::pair<iterator, bool> item =
			this->trie_.insert (key, payload);

		if (item.second) // real insert
		{
			typename std::map<std::string, policy_container>::iterator it = policys_.find(br);
			if(it != policys_.end())
			{
				bool ok = it->second.insert(s_iterator_to (item.first), len);
				if (!ok)
				{
					item.first->erase (); // cannot insert
					return std::make_pair(this->end(), false);
				}
			}
		}
		else
		{
			return std::make_pair(s_iterator_to (item.first), false);
		}
		return item;
	}

	inline void
	GetCachedName(std::vector<ns3::ndn::Name>& result)
	{
		Ptr<FullKey> nameforroot = Create<FullKey>();
		trie_.traversal(result, nameforroot);
	}

	inline void
	erase (const FullKey &key)
	{
		iterator foundItem, lastItem;
	    bool reachLast;
	    boost::tie (foundItem, reachLast, lastItem) = trie_.find (key);

	    if (!reachLast || lastItem->payload () == PayloadTraits::empty_payload)
	      return; // nothing to invalidate

	    erase (lastItem);

	}

	inline void
	erase (iterator node, const std::string& name)
	{
		if (node == this->end()) return;
		auto iter = policys_.find(name);
		if(iter != policys_.end())
			iter->second.erase(s_iterator_to (node));
		node->erase ();
	}

	inline void
	clear ()
	{
		for(auto iter = policys_.begin(); iter != policys_.end(); iter++)
			iter->second.clear();
		this->trie_.clear ();
	}

	inline iterator
	find_exact (const FullKey &key)
	{
		iterator foundItem, lastItem;
	    bool reachLast;
	    boost::tie (foundItem, reachLast, lastItem) = trie_.find (key);

	    if (!reachLast || lastItem->payload () == PayloadTraits::empty_payload)
	      return end ();

	    return lastItem;
	}
  /**
   * @brief Find a node that has prefix at least as the key (cache lookup)
   */
	inline iterator
	deepest_prefix_match (const FullKey &key, std::string br)
	{
		iterator foundItem, lastItem;
		bool reachLast;
		boost::tie (foundItem, reachLast, lastItem) = this->trie_.find (key);

		// guard in case we don't have anything in the trie
		if (lastItem == this->trie_.end ())
		return this->trie_.end ();

		if (reachLast)
		{
			if (foundItem == this->trie_.end ())
			{
				foundItem = lastItem->find (); // should be something
			}
			typename std::map<std::string, policy_container>::iterator it = policys_.find(br);
			if(it != policys_.end())
				it->second.lookup(s_iterator_to (foundItem));
			return foundItem;
		}
		else
		{ // couldn't find a node that has prefix at least as key
			return this->trie_.end ();
		}
	}



  /**
   * @brief Find a node that has prefix at least as the key
   *
   * This version of find checks predicate for the next level and if
   * predicate is True, returns first deepest match available
   */
  template<class Predicate>
  inline iterator
  deepest_prefix_match_if_next_level (const FullKey &key, Predicate pred, std::string br)
  {
    iterator foundItem, lastItem;
    bool reachLast;
    boost::tie (foundItem, reachLast, lastItem) = this->trie_.find (key);

    // guard in case we don't have anything in the trie
    if (lastItem == this->trie_.end ())
      return this->trie_.end ();

    if (reachLast)
      {
        foundItem = lastItem->find_if_next_level (pred); // may or may not find something
        if (foundItem == this->trie_.end ())
          {
            return this->trie_.end ();
          }
		typename std::map<std::string, policy_container>::iterator it = policys_.find(br);
		if(it != policys_.end())
			it->second.lookup(s_iterator_to (foundItem));
        return foundItem;
      }
    else
      { // couldn't find a node that has prefix at least as key
        return this->trie_.end ();
      }
  }

  const policy_container&
  getPolicy (std::string br) const { return policys_.at(br); }


  policy_container&
  getPolicy (std::string br) {return policys_.at(br); }


  iterator end () const
  {
    return 0;
  }

  const parent_trie &
  getTrie () const { return trie_; }

  parent_trie &
  getTrie () { return trie_; }


  static inline iterator
  s_iterator_to (typename parent_trie::iterator item)
  {
    if (item == 0)
      return 0;
    else
      return &(*item);
  }


private:
  parent_trie      trie_;
  std::map<std::string, policy_container> policys_;

};

} // ndnSIM
} // ndn
} // ns3

#endif // TRIE_WITH_POLICY_H_
