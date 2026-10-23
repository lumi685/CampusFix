#pragma once
#include <cstdint>
#include <string>
namespace campusfix {
struct Grant { std::string capability; bool global=false; std::int64_t locationId=0; };
}
