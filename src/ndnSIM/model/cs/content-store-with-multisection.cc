
#include "content-store-with-multisection.h"

#include "../../utils/trie/userdefined-policy.h"

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

template class ContentStoreMulSec<user_defined_policy_traits>;


NS_OBJECT_ENSURE_REGISTERED_TEMPL(ContentStoreMulSec, user_defined_policy_traits);




} // namespace cs
} // namespace ndn
} // namespace ns3
