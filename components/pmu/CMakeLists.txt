cmake_minimum_required(VERSION 3.30)
set(EXTRA_COMPONENT_DIRS ../../../XPowersLib)

idf_component_register (
  SRCS "pmu.cpp"
  REQUIRES driver XPowersLib main
  INCLUDE_DIRS "." "../../main" 
)
