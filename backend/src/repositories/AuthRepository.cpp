#include "repositories/AuthRepository.h"

namespace campusfix {
namespace {
AuthUser readUser(SqliteStatement &q) {
    if (!q.step()) return {};
    return {q.columnInt(0),q.columnText(1),q.columnText(2),q.columnText(3),
            q.columnText(4),q.columnText(5)};
}
}
AuthUser AuthRepository::userByName(const std::string &name) {
    auto q=db_.prepare("SELECT id,username,password_hash,display_name,role,status FROM users WHERE username=?1");
    q.bindText(1,name); return readUser(q);
}
AuthUser AuthRepository::userByTokenHash(const std::string &hash) {
    auto q=db_.prepare("SELECT u.id,u.username,u.password_hash,u.display_name,u.role,u.status,"
       "s.token_hash,s.expires_at<=strftime('%Y-%m-%dT%H:%M:%fZ','now'),s.revoked_at IS NOT NULL "
       "FROM auth_sessions s JOIN users u ON u.id=s.user_id WHERE s.token_hash=?1");
    q.bindText(1,hash);
    if(!q.step()) return {};
    AuthUser user{q.columnInt(0),q.columnText(1),q.columnText(2),q.columnText(3),
                  q.columnText(4),q.columnText(5)};
    user.tokenHash=q.columnText(6); user.expired=q.columnInt(7)!=0; user.revoked=q.columnInt(8)!=0;
    return user;
}
std::vector<Grant> AuthRepository::grants(std::int64_t id) {
    auto q=db_.prepare("SELECT capability,scope_location_id FROM management_grants "
                       "WHERE user_id=?1 AND revoked_at IS NULL ORDER BY capability,scope_location_id");
    q.bindInt(1,id); std::vector<Grant> out;
    while(q.step()) out.push_back({q.columnText(0),q.columnIsNull(1),q.columnInt(1)});
    return out;
}
std::int64_t AuthRepository::workerId(std::int64_t id) {
    auto q=db_.prepare("SELECT id FROM workers WHERE user_id=?1"); q.bindInt(1,id);
    return q.step()?q.columnInt(0):0;
}
void AuthRepository::createSession(const std::string &id,std::int64_t userId,
                                   const std::string &hash,const std::string &expires) {
    auto q=db_.prepare("INSERT INTO auth_sessions(id,user_id,token_hash,expires_at) VALUES(?1,?2,?3,?4)");
    q.bindText(1,id); q.bindInt(2,userId); q.bindText(3,hash); q.bindText(4,expires); q.step();
    auto u=db_.prepare("UPDATE users SET last_login_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id=?1");
    u.bindInt(1,userId); u.step();
}
bool AuthRepository::revokeSession(const std::string &hash) {
    auto q=db_.prepare("UPDATE auth_sessions SET revoked_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') "
       "WHERE token_hash=?1 AND revoked_at IS NULL AND expires_at>strftime('%Y-%m-%dT%H:%M:%fZ','now') "
       "AND user_id IN (SELECT id FROM users WHERE status='ACTIVE')");
    q.bindText(1,hash); q.step(); return sqlite3_changes(db_.nativeHandle())==1;
}
}
