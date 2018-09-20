#include "content-store-with-transcode.h"
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

template class ContentStoreTranscoding<fixed_policy_traits>;

template class ContentStoreTranscoding<lru_policy_traits>;

template class ContentStoreTranscoding<lfu_policy_traits>;

NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreTranscoding, fixed_policy_traits);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreTranscoding, lru_policy_traits);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreTranscoding, lfu_policy_traits);


} // namespace cs
} // namespace ndn
} // namespace ns3
