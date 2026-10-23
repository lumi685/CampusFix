#include "services/DemoAccountActivation.h"
#include "infrastructure/AuthCrypto.h"
#include "infrastructure/SqliteConnection.h"
#include <conio.h>
#include <iostream>
#include <stdexcept>

namespace campusfix {
namespace {
void validateName(const std::string &name) {
    if(name.rfind("demo_",0)!=0 || name.size()>64)
        throw std::runtime_error("Only named fictional demo accounts may be activated");
}
}
std::string readSecretFromConsole() {
    std::cerr << "Password (hidden): "; std::string password;
    for(;;) {
        const int key=_getch();
        if(key=='\r') break;
        if(key==3) throw std::runtime_error("Cancelled");
        if(key==0 || key==224) { _getch(); continue; }
        if(key=='\b') { if(!password.empty()) password.pop_back(); continue; }
        if(key>=32 && key<=126 && password.size()<128) password.push_back(static_cast<char>(key));
    }
    std::cerr << '\n'; return password;
}
void setDemoPassword(const std::filesystem::path &path,const std::string &name,
                     const std::string &password) {
    validateName(name);
    if(password.size()<12 || password.size()>128) throw std::runtime_error("Password must be 12-128 characters");
    const auto hash=hashPassword(password);
    SqliteConnection db(path); SqliteTransaction tx(db);
    auto q=db.prepare("UPDATE users SET password_hash=?1,updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') "
       "WHERE username=?2 AND status='DISABLED' AND password_hash='!UNSET_DEMO_CREDENTIAL!'");
    q.bindText(1,hash); q.bindText(2,name); q.step();
    if(sqlite3_changes(db.nativeHandle())!=1) throw std::runtime_error("Demo account unavailable or password already set");
    tx.commit();
}
void enableDemoAccount(const std::filesystem::path &path,const std::string &name) {
    validateName(name);
    SqliteConnection db(path); SqliteTransaction tx(db);
    auto q=db.prepare("UPDATE users SET status='ACTIVE',updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') "
       "WHERE username=?1 AND status='DISABLED' AND password_hash LIKE '$argon2id$%'");
    q.bindText(1,name); q.step();
    if(sqlite3_changes(db.nativeHandle())!=1) throw std::runtime_error("Demo account unavailable or secure password not set");
    tx.commit();
}
}
