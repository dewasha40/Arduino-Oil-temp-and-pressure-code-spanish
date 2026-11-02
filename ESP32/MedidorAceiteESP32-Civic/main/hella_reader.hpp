#pragma once
namespace hella { struct Reading {float oil_c=-40,pressure_bar=0,auxiliary=0,battery_v=0;bool oil_valid=false,pressure_valid=false,auxiliary_valid=false;}; bool begin(); Reading read(); }
