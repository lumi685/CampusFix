#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "infrastructure/AuthCrypto.h"
#include "services/DemoAccountActivation.h"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc,char **argv) {
    if(argc!=5) return 2;
    try {
        campusfix::initializeCrypto();
        campusfix::applyMigrations(argv[1],argv[2]);
        campusfix::applyDemoSeed(argv[1],argv[3]);
        const std::string password=campusfix::newToken();
        campusfix::setDemoPassword(argv[1],"demo_reporter01",password);
        campusfix::enableDemoAccount(argv[1],"demo_reporter01");
        std::ofstream secret(argv[4],std::ios::binary);
        secret<<password;
        if(!secret) throw std::runtime_error("Cannot write temporary test secret");
        std::cout<<"PASS temporary demo auth fixture created\n";
        return 0;
    } catch(const std::exception &error) { std::cerr<<error.what()<<'\n'; return 1; }
}
