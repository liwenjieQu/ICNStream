#include "content-store-with-AdaptiveVideo.h"
#include "../../utils/trie/fixed-policy.h"
#include "../../utils/trie/lru-policy.h"
#include "../../utils/trie/lfu-policy.h"

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

/*
 * By Wenjie Li
 * Cache Configuration determined by ILP solver
 */
template class ContentStoreStreaming<fixed_policy_traits>;

template class ContentStoreStreaming<lru_policy_traits>;

template class ContentStoreStreaming<lfu_policy_traits>;

NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreStreaming, fixed_policy_traits);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreStreaming, lru_policy_traits);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreStreaming, lfu_policy_traits);
/*
typedef multi_policy_traits< boost::mpl::vector2< fixed_policy_traits,
                                                  aggregate_stats_policy_traits > > FixedWithCountsTraits;

template class ContentStoreStreaming<FixedWithCountsTraits>;
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreStreaming, FixedWithCountsTraits);
*/

} // namespace cs
} // namespace ndn
} // namespace ns3
