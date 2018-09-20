/*
 * Author: Wenjie Li
 * Date: 2016-12-05
 */
#include "ndn-marl-impl.h"

#define NS_OBJECT_ENSURE_REGISTERED_TEMPL(type, templ1, templ2)		\
  static struct X ## type ## templ1 ## templ2 ## RegistrationClass	\
  {                                                     			\
    X ## type ## templ1 ## templ2 ## RegistrationClass () {			\
      ns3::TypeId tid = type<templ1, templ2>::GetTypeId ();			\
      tid.GetParent ();                                 			\
    }                                                   			\
  } x_ ## type ## templ1 ## templ2 ## RegistrationVariable

namespace ns3 {
namespace ndn {

template class MARLnfaImpl<DependentMARLState, DependentMARLAction>;
template class MARLnfaImpl<DependentMARLState, AggregateMARLAction>;
template class MARLnfaImpl<AggregateMARLState, DependentMARLAction>;
template class MARLnfaImpl<AggregateMARLState, AggregateMARLAction>;

NS_OBJECT_ENSURE_REGISTERED_TEMPL(MARLnfaImpl, DependentMARLState, DependentMARLAction);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(MARLnfaImpl, DependentMARLState, AggregateMARLAction);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(MARLnfaImpl, AggregateMARLState, DependentMARLAction);
NS_OBJECT_ENSURE_REGISTERED_TEMPL(MARLnfaImpl, AggregateMARLState, AggregateMARLAction);

}
}

