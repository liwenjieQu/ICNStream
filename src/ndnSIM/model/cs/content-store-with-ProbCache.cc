#include "content-store-with-ProbCache.h"

#include "../../utils/trie/random-policy.h"
#include "../../utils/trie/lru-policy.h"
#include "../../utils/trie/fifo-policy.h"
#include "../../utils/trie/lfu-policy.h"
#include "../../utils/trie/multi-policy.h"
#include "../../utils/trie/aggregate-stats-policy.h"

#define NS_OBJECT_ENSURE_REGISTERED_TEMPL(type, templ)  \
  static struct X ## type ## templ ## RegistrationClass \
  {                                                     \
    X ## type ## templ ## RegistrationClass () {        \
      ns3::TypeId tid = type<templ>::GetTypeId ();      \
      tid.GetParent ();                                 \
    }                                                   \
  } x_ ## type ## templ ## RegistrationVariable

namespace ns3 {
namespace ndn {

using namespace ndnSIM;

namespace cs {

// explicit instantiation and registering
/**
 * @brief ContentStore with LRU cache replacement policy
 **/
template class ContentStoreWithProbCache<lru_policy_traits>;

/**
 * @brief ContentStore with random cache replacement policy
 **/
template class ContentStoreWithProbCache<random_policy_traits>;

/**
 * @brief ContentStore with FIFO cache replacement policy
 **/
template class ContentStoreWithProbCache<fifo_policy_traits>;

/**
 * @brief ContentStore with Least Frequently Used (LFU) cache replacement policy
 **/
template class ContentStoreWithProbCache<lfu_policy_traits>;


NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreWithProbCache, lru_policy_traits);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreWithProbCache, random_policy_traits);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreWithProbCache, fifo_policy_traits);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreWithProbCache, lfu_policy_traits);


typedef multi_policy_traits< boost::mpl::vector2< lru_policy_traits,
                                                  aggregate_stats_policy_traits > > LruWithCountsTraits;
typedef multi_policy_traits< boost::mpl::vector2< random_policy_traits,
                                                  aggregate_stats_policy_traits > > RandomWithCountsTraits;
typedef multi_policy_traits< boost::mpl::vector2< fifo_policy_traits,
                                                  aggregate_stats_policy_traits > > FifoWithCountsTraits;
typedef multi_policy_traits< boost::mpl::vector2< lfu_policy_traits,
                                                  aggregate_stats_policy_traits > > LfuWithCountsTraits;

template class ContentStoreWithProbCache<LruWithCountsTraits>;
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreWithProbCache, LruWithCountsTraits);

template class ContentStoreWithProbCache<RandomWithCountsTraits>;
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreWithProbCache, RandomWithCountsTraits);

template class ContentStoreWithProbCache<FifoWithCountsTraits>;
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreWithProbCache, FifoWithCountsTraits);

template class ContentStoreWithProbCache<LfuWithCountsTraits>;
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreWithProbCache, LfuWithCountsTraits);


#ifdef DOXYGEN
// /**
//  * \brief Content Store implementing LRU cache replacement policy
//  */
class Lru : public ContentStoreWithProbCache<lru_policy_traits> { };

/**
 * \brief Content Store implementing FIFO cache replacement policy
 */
class Fifo : public ContentStoreWithProbCache<fifo_policy_traits> { };

/**
 * \brief Content Store implementing Random cache replacement policy
 */
class Random : public ContentStoreWithProbCache<random_policy_traits> { };

/**
 * \brief Content Store implementing Least Frequently Used cache replacement policy
 */
class Lfu : public ContentStoreWithProbCache<lfu_policy_traits> { };
#endif


} // namespace cs
} // namespace ndn
} // namespace ns3
