#ifndef CRETE_CLUSTER_SVM_NODE_FSM_H
#define CRETE_CLUSTER_SVM_NODE_FSM_H

#include <crete/cluster/node.h>
#include <crete/test_case.h>
#include <crete/asio/server.h>
#include <crete/dll.h>

#include <vector>
#include <memory>

#include <boost/msm/back/state_machine.hpp> // back-end

namespace crete
{
namespace cluster
{
namespace node
{
namespace svm
{

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
    struct tests_ready {};
    struct next_trace {};
    struct error {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
namespace ev
{
    struct start;
    struct poll {};
    struct next_trace;
    struct tests_queued {};
    struct error {};
}



namespace fsm
{

class KleeFSM;

} // namespace fsm
} // namespace svm
} // namespace node
} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_SVM_NODE_FSM_H
