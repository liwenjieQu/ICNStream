/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
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
 */

#ifndef USER_DEFINED_POLICY_H_
#define USER_DEFINED_POLICY_H_

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/list.hpp>
#include "ns3/name.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <map>
#include <string>

namespace ns3 {
namespace ndn {
namespace ndnSIM {

/**
 * @brief Traits for Least Recently Used replacement policy
 */
struct user_defined_policy_traits
{
  /// @brief Name that can be used to identify the policy (for NS-3 object model and logging)
  static std::string GetName () { return "OLru"; }

  struct policy_hook_type : public boost::intrusive::list_member_hook<> {};

  template<class Container>
  struct container_hook
  {
    typedef boost::intrusive::member_hook< Container,
                                           policy_hook_type,
                                           &Container::policy_hook_ > type;
  };

  template<class Base,
           class Container,
           class Hook>
  struct policy
  {
    typedef typename boost::intrusive::list< Container, Hook > policy_container;

    // could be just typedef
    class type : public policy_container
    {
    public:
      typedef Container parent_trie;

      type (Base &base, const std::string& name)
        : base_ (base)
      	, BRidx (name)
        , max_size_ (100)
      	, now_size_ (0)
      {
      }

      inline void
      update (typename parent_trie::iterator item)
      {
        // do relocation
        policy_container::splice (policy_container::end (),
                                  *this,
                                  policy_container::s_iterator_to (*item));
      }

      inline bool
      insert (typename parent_trie::iterator item)
      {
        if (max_size_ != 0 && policy_container::size () >= max_size_)
          {
            base_.erase (&(*policy_container::begin ()));
          }

        policy_container::push_back (*item);
        return true;
      }
      inline bool
      insert (typename parent_trie::iterator item, uint32_t s)
      {
    	  while(max_size_ < s + now_size_)
    	  {
    		  //The size of the single chunk (item) exceeds the cache capacity
			  if(now_size_ == 0)
				  return false;

	        	typename parent_trie::iterator musttry = &(*policy_container::begin ());
	            std::string replace_piece;
	            while(musttry != 0)
	            {
	            	replace_piece = musttry->key().toUri() + replace_piece;
	            	musttry = musttry->Getparent();
	            }
	            std::map<std::string, size_t>::iterator iter = track_itemsize_.find(replace_piece);
	            if(iter != track_itemsize_.end())
	            {
	                now_size_ -= iter->second;
	                track_itemsize_.erase(iter);
	            }
	            else
	            {
	            	std::cout << "WTFFFFFFFFFFFFFFFFFF!!!!"<<std::endl;
	            }
	            /*
	            typename parent_trie::iterator todelete = &(*policy_container::begin ());
	            if(todelete != 0)
	            {
	            	erase(Base::s_iterator_to(todelete));
		        	base_.erase (todelete);
	            }
	            */
	            base_.erase (&(*policy_container::begin ()), BRidx);
    	  }
	        typename parent_trie::iterator whatname = item;
	        std::string piece;
	        while(whatname != 0)
	        {
	        	piece = whatname->key().toUri() + piece;
	        	whatname = whatname->Getparent();
	        }

	        now_size_ += s;
	        policy_container::push_back (*item);
	        track_itemsize_[piece] = s;
	        return true;
      }

      inline void
      lookup (typename parent_trie::iterator item)
      {
        // do relocation
        policy_container::splice (policy_container::end (),
                                  *this,
                                  policy_container::s_iterator_to (*item));
      }

      inline void
      erase (typename parent_trie::iterator item)
      {
        policy_container::erase (policy_container::s_iterator_to (*item));
      }

      inline void
      clear ()
      {
        policy_container::clear ();
        now_size_ = 0;
        track_itemsize_.clear();
      }

      inline void
      set_max_size (size_t max_size)
      {
        max_size_ = max_size;
      }

      inline size_t
      get_max_size () const
      {
        return max_size_;
      }
      inline size_t
	  get_current_size() const
      {
    	  return now_size_;
      }
    private:
      type () : base_(*((Base*)0)) { };

    private:
      Base &base_;
      std::string	BRidx;
      size_t max_size_;
      size_t now_size_;
      std::map<std::string, size_t> track_itemsize_;
    };
  };
};
} // ndnSIM
} // ndn
} // ns3

#endif
