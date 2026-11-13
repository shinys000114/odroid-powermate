# SPDX-FileCopyrightText: 2025 Hardkernel Co., Ltd.
# SPDX-License-Identifier: GPL-3.0-only

# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# PROJECT_VER becomes the firmware version (esp_app_desc, /api/version, the merged
# bin name). It follows git: commits and tags trigger a reconfigure by themselves.
include(cmake/version.cmake)
powermate_git_version(PROJECT_VER)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(odroid-power-mate)
add_subdirectory(example/pmtui)
include(cmake/gen_powermate_bin.cmake)
