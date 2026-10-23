#pragma once
#include "models/ManagementGrant.h"
#include <cstdint>
#include <string>
#include <vector>

namespace campusfix {
enum class Action {
    Create,Read,Edit,Submit,Cancel,Review,Recommend,Assign,Accept,Refuse,Transfer,
    Progress,Complete,VerifyPass,VerifyFail,Evaluate,Close,InternalNotes,
    WorkerRead,WorkerManage,Statistics
};
struct Subject {
    std::int64_t id=0;
    std::string role;
    bool active=false;
    std::vector<Grant> grants;
};
struct OrderScope {
    bool exists=false;
    std::int64_t reporterId=0,version=0;
    std::string status;
    bool currentWorker=false, historicalWorker=false, evaluated=false;
    std::vector<std::int64_t> ancestors;
};
struct AccessDecision {
    int status=200;
    std::string code;
    bool allowed() const {return status==200;}
};
class AuthorizationPolicy {
public:
    static bool capability(const Subject &,const std::string &,const std::vector<std::int64_t> &);
    static bool originalReporter(const Subject &,const OrderScope &);
    static bool mayRead(const Subject &,const OrderScope &);
    static AccessDecision order(const Subject &,Action,const OrderScope &);
    static AccessDecision operation(const Subject &,Action,const OrderScope &,std::int64_t expectedVersion);
    static AccessDecision location(const Subject &,Action,const std::vector<std::int64_t> &,
                                   bool ownWorker=false);
};
// A projection must opt in to each field. Unknown/raw database fields fail closed.
struct FieldVisibility {
    bool publicInfo=false,contact=false,internal=false;
    bool allows(const std::string &field) const;
};
}
