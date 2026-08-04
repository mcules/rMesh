#pragma once
// Native unit-test stub for the build-generated src/version.h. The real file is
// produced by extra_scripts/get_version.py at firmware build time and is
// .gitignore'd, so it is absent on a fresh CI checkout. This mock is only found
// when the real src/version.h does not exist (quote-include searches src/ first).
#define VERSION "native-test"
