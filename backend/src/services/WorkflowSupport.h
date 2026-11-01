#pragma once
#include "services/AuthService.h"
#include "services/AuthorizationService.h"
#include "domain/WorkOrderValidation.h"
#include "models/WorkOrder.h"
namespace campusfix::workflow {
inline void authenticate(SqliteConnection &db,std::int64_t user,const std::string &token) {
    try {if(AuthService::me(db,token).user.id!=user)throw std::invalid_argument("AUTH_INVALID");}
    catch(const std::invalid_argument &e){throw WorkOrderError(401,std::string(e.what())=="AUTH_EXPIRED"?"AUTH_EXPIRED":"AUTH_INVALID","Authentication failed");}
}
inline void require(const AccessDecision &decision) {
    if(!decision.allowed())throw WorkOrderError(decision.status,decision.code,"Operation unavailable");
}
inline void version(const WorkOrder &order,std::int64_t expected) {
    if(order.version!=expected){WorkOrderError e(409,"ORDER_VERSION_CONFLICT","Order version changed");e.currentVersion=order.version;throw e;}
}
inline void note(const std::string &value,std::size_t max,const char *field,bool optional=false) {
    validateOrderText(value,max,field,true);
    if(optional)return;
    // Decode only after the common validator has established valid UTF-8.
    for(std::size_t i=0;i<value.size();) {
        const auto b=static_cast<unsigned char>(value[i++]);std::uint32_t cp=b;int n=0;
        if(b>=0xF0){cp=b&7;n=3;}else if(b>=0xE0){cp=b&15;n=2;}else if(b>=0xC0){cp=b&31;n=1;}
        while(n-->0)cp=(cp<<6)|(static_cast<unsigned char>(value[i++])&63);
        if(!((cp>=9&&cp<=13)||cp==32||cp==0x85||cp==0xA0||cp==0x1680||(cp>=0x2000&&cp<=0x200A)||cp==0x2028||cp==0x2029||cp==0x202F||cp==0x205F||cp==0x3000||cp==0xFEFF))return;
    }
    throw WorkOrderError(422,"VALIDATION_ERROR","Non-blank text required",field);
}
}
