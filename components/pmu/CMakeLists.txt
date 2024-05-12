
cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS ../../../XPowersLib)

idf_component_register (
  SRCS "pmu.cpp"

  INCLUDE_DIRS "." "../../main" 
  REQUIRES driver XPowersLib
)
