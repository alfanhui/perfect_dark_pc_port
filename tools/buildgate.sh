#!/bin/bash
# L0 build gate for the PC port (docs/perfecthead-testplan.md, BLD-03): build
# with PerfectHead off and on, for each architecture and region, and check
# that only the "on" build contains PerfectHead. Needs no ROM.
#
# Usage: tools/buildgate.sh
#   ARCHS   Windows (MSYS2): x86_64 and/or i686, built with /mingw64 and
#           /mingw32 (default: both). Elsewhere: native only.
#   ROMIDS  default: ntsc-final (add pal-final, jpn-final for every region)
#   JOBS    build jobs (default 8)
#   CLEAN=1 delete the build-gate folders first
#
# Builds go to build-gate/<arch>-<romid>-<off|on>. Exit status is 0 only when
# no gate failed. Linux and macOS are also covered by .github/workflows/c-cpp.yml.
cd "$(dirname "$0")/.." || exit 1
ROMIDS=${ROMIDS:-ntsc-final}
JOBS=${JOBS:-8}
RES=build-gate/results.txt
[ "$CLEAN" = 1 ] && rm -rf build-gate
mkdir -p build-gate
: > "$RES"

case $(uname -s) in
MINGW*|MSYS*) WIN=1; ARCHS=${ARCHS:-x86_64 i686} ;;
*) WIN=0; ARCHS=native ;;
esac

# Symbols that exist only when PERFECTHEAD is on (macOS prefixes an underscore)
MARKERS="pheadPaintHead g_PheadRootMenuDialog"

result() { echo "$1 BLD-03 $2" | tee -a "$RES"; }

for arch in $ARCHS; do
	path=$PATH
	if [ $WIN = 1 ]; then
		case $arch in
		x86_64) root=/mingw64 ;;
		i686) root=/mingw32 ;;
		*) result FAIL "$arch: unknown architecture"; continue ;;
		esac
		# MSYS2's /mingw64 is C:/msys64/mingw64 when run from Git Bash
		[ -x $root/bin/cmake ] || root=/c/msys64$root
		if [ ! -x $root/bin/cmake ]; then
			result SKIP "$arch: no cmake in $root (install mingw-w64-$arch-toolchain, -cmake, -SDL2, -zlib)"
			continue
		fi
		# Only this architecture's toolchain on PATH, or CMake finds the other's
		# libraries (an i686 link against x86_64 zlib)
		path=$root/bin:/c/msys64/usr/bin:/usr/bin:/c/Windows/System32:/c/Windows
		export MSYSTEM=$(echo ${root##*/} | tr a-z A-Z)
	fi
	for romid in $ROMIDS; do
		for ph in OFF ON; do
			name=$arch-$romid-$(echo $ph | tr A-Z a-z)
			dir=build-gate/$name
			log=$dir.log
			if ! PATH=$path cmake -G"Unix Makefiles" -B"$dir" -DROMID="$romid" -DPERFECTHEAD=$ph . > "$log" 2>&1 \
				|| ! PATH=$path cmake --build "$dir" -j"$JOBS" >> "$log" 2>&1; then
				result FAIL "$name: build failed: $(grep -m1 -E 'error|Error [0-9]' "$log" | cut -c1-120) (see $log)"
				continue
			fi
			exe=$(ls "$dir"/pd*.exe "$dir"/pd*.x86_64 "$dir"/pd*.i686 "$dir"/pd*.arm64 2>/dev/null | head -1)
			[ -n "$exe" ] || exe=$(find "$dir" -maxdepth 1 -type f -name 'pd*' ! -name '*.*' | head -1)
			if [ -z "$exe" ]; then
				result FAIL "$name: built, but no executable found in $dir"
				continue
			fi
			nm=nm
			[ $WIN = 1 ] && nm=$root/bin/nm
			found=0
			for s in $MARKERS; do
				PATH=$path $nm "$exe" 2>/dev/null | grep -qE " _?$s\$" && found=$((found + 1))
			done
			want=0
			[ $ph = ON ] && want=$(echo $MARKERS | wc -w)
			if [ $found -ne $want ]; then
				result FAIL "$name: $found of the PerfectHead marker symbols, want $want ($exe)"
			else
				result PASS "$name: $exe"
			fi
		done
	done
done

echo
echo "== Results ($RES)"
cat "$RES"
! grep -q '^FAIL' "$RES"
