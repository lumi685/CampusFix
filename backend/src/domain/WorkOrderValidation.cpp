#include "domain/WorkOrderValidation.h"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace campusfix {
namespace {
void invalid(const char *field) {throw WorkOrderError(422,"VALIDATION_ERROR","Invalid work order field",field);}
void text(const std::string &s,std::size_t max,const char *field,bool allowBlank=false) {
    // Count Unicode scalar values (SQLite length semantics), rejecting malformed UTF-8,
    // NUL and control characters instead of allowing truncation at the SQL boundary.
    std::size_t count=0;
    bool content=false;
    for(std::size_t i=0;i<s.size();) {
        auto lead=static_cast<unsigned char>(s[i++]);
        std::uint32_t cp=lead; int extra=0;
        if(lead>=0xC2 && lead<=0xDF) {cp=lead&31;extra=1;}
        else if(lead>=0xE0 && lead<=0xEF) {cp=lead&15;extra=2;}
        else if(lead>=0xF0 && lead<=0xF4) {cp=lead&7;extra=3;}
        else if(lead>=0x80) invalid(field);
        if(i+extra>s.size()) invalid(field);
        for(int n=0;n<extra;++n) {
            auto c=static_cast<unsigned char>(s[i++]);
            if((c&0xC0)!=0x80) invalid(field);
            cp=(cp<<6)|(c&63);
        }
        if((extra==1&&cp<0x80)||(extra==2&&cp<0x800)||(extra==3&&cp<0x10000)||
           cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF)||cp==0||
           (cp<32&&cp!=9&&cp!=10&&cp!=13)||cp==127) invalid(field);
        if(cp!=32&&cp!=9&&cp!=10&&cp!=13&&cp!=0x3000) content=true;
        if(++count>max) invalid(field);
    }
    if(!allowBlank&&(!count||!content)) invalid(field);
}
std::tm utc(std::time_t t) {
    std::tm result{};
#ifdef _WIN32
    if(gmtime_s(&result,&t)!=0) throw std::runtime_error("UTC conversion failed");
#else
    if(!gmtime_r(&t,&result)) throw std::runtime_error("UTC conversion failed");
#endif
    return result;
}
}
void validateOrderText(const std::string &value,std::size_t max,const char *field,bool allowBlank) {
    text(value,max,field,allowBlank);
}
bool validUtcDateTime(const std::string &s) {
    if(s.size()<20||s.size()>30||s[4]!='-'||s[7]!='-'||s[10]!='T'||s[13]!=':'||s[16]!=':'||s.back()!='Z') return false;
    if(s.size()!=20 && (s.size()<22||s[19]!='.')) return false;
    for(std::size_t i=0;i<s.size()-1;++i) {
        if(i==4||i==7||i==10||i==13||i==16||(i==19&&s.size()!=20)) continue;
        if(s[i]<'0'||s[i]>'9') return false;
    }
    const int year=std::stoi(s.substr(0,4)),month=std::stoi(s.substr(5,2)),day=std::stoi(s.substr(8,2));
    static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(year<1||month<1||month>12) return false;
    const bool leap=(year%4==0&&year%100!=0)||year%400==0;
    return day>=1&&day<=days[month-1]+(month==2&&leap?1:0)&&
        std::stoi(s.substr(11,2))<24&&std::stoi(s.substr(14,2))<60&&std::stoi(s.substr(17,2))<60;
}
void validateWorkOrderInput(const WorkOrderInput &i) {
    if(i.categoryId<=0) invalid("category_id");
    if(i.locationId<=0) invalid("location_id");
    text(i.title,120,"title");text(i.description,2000,"description");
    text(i.contactName,80,"contact_name");text(i.contactPhone,32,"contact_phone");
    if(i.urgency!="LOW"&&i.urgency!="NORMAL"&&i.urgency!="HIGH"&&i.urgency!="URGENT") invalid("requested_urgency");
    if(i.expectedVisitAt&&!validUtcDateTime(*i.expectedVisitAt)) invalid("expected_visit_at");
}
OrderTime orderTime(std::chrono::system_clock::time_point point) {
    const auto seconds=std::chrono::time_point_cast<std::chrono::seconds>(point);
    const auto millis=std::chrono::duration_cast<std::chrono::milliseconds>(point-seconds).count();
    const auto t=std::chrono::system_clock::to_time_t(seconds);
    const auto universal=utc(t),business=utc(t+8*60*60);
    std::ostringstream u,b;
    u<<std::put_time(&universal,"%Y-%m-%dT%H:%M:%S.")<<std::setfill('0')<<std::setw(3)<<millis<<'Z';
    b<<std::put_time(&business,"%Y%m%d");
    return {u.str(),b.str()};
}
}
