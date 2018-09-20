#ifndef FIXED_POLICY_H_
#define FIXED_POLICY_H_

/*
 * author: Wenjie Li
 * Use pre-determined cache configuration
 * Configuration is stored in global variable
 */
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/list.hpp>

namespace ns3 {
namespace ndn {
namespace ndnSIM {


struct fixed_policy_traits
{
  /// @brief Name that can be used to identify the policy (for NS-3 object model and logging)
  static std::string GetName () { return "DASCache"; }

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

      type (Base &base)
        : current_size_(0)
		, base_ (base)
        , max_size_ (100)
      {
      }

      inline void
      update (typename parent_trie::iterator item)
      {
    	  //Do nothing!
    	  //Extra Info: Seems there is no one calling this function
      }

      inline bool
      insert (typename parent_trie::iterator item)
      {
    	  policy_container::push_back (*item);
    	  return true;
      }

      inline bool
      insert (typename parent_trie::iterator item, uint32_t s)
      {
        return true;
      }

      inline void
      lookup (typename parent_trie::iterator item)
      {
    	  //Do nothing!
    	  //There is no need to change the order in the container
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
      inline
      void addsize(size_t s)
      {
    	  current_size_ += s;
      }
      inline size_t
	  get_current_size() const
      {
    	  return current_size_;
      }
    private:
      type () : base_(*((Base*)0)) { };

    public:
      size_t current_size_;

    private:
      Base &base_;
      size_t max_size_;
    };
  };
};

} // ndnSIM
} // ndn
} // ns3

#endif
