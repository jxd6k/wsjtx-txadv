#!/usr/bin/env bash
# Build a local development app with its subordinate programs and shipped data.
# Uses existing Homebrew dependencies; does not install or change system settings.
set -euo pipefail

source_dir="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="${1:-${source_dir}/cmake-build-release}"
brew_prefix="$(brew --prefix)"
qt_prefix="$(brew --prefix qt@5)"

cmake -S "${source_dir}" -B "${build_dir}" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="${qt_prefix};${brew_prefix}" \
  -DWSJT_SKIP_MANPAGES=ON -DWSJT_GENERATE_DOCS=OFF -DWSJT_SKIP_MAP65=ON
cmake --build "${build_dir}" --target \
  wsjtx jt9 wsprd fmtave fcal fmeasure test_tx_timing -j 8
ctest --test-dir "${build_dir}" -R '^test_tx_timing$' --output-on-failure

bundle="${build_dir}/wsjtx.app/Contents"
mkdir -p "${bundle}/Resources/wsjtx"
for helper in jt9 wsprd fmtave fcal fmeasure; do
  cp "${build_dir}/${helper}" "${bundle}/MacOS/"
done
for data in cty.dat cty.dat_copyright.txt grid.dat sat.dat eclipse.txt ALLCALL7.TXT; do
  cp "${source_dir}/${data}" "${bundle}/Resources/wsjtx/"
done
cp "${source_dir}/contrib/Ephemeris/JPLEPH" "${bundle}/Resources/wsjtx/"
# Copy the contents so repeated builds do not nest a second sounds directory.
mkdir -p "${bundle}/Resources/sounds"
cp -R "${source_dir}/sounds/." "${bundle}/Resources/sounds/"
"${bundle}/MacOS/jt9" --help > /dev/null
printf 'Local development app: %s/wsjtx.app\n' "${build_dir}"
