# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS ../../../XPowersLib)

idf_component_register (
  SRCS "pmu.cpp"

  INCLUDE_DIRS "."
  REQUIRES driver XPowersLib
)
