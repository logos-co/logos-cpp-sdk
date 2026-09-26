# lib.mkClients: the client pair per contract and nothing else, typed only when
# asked for, and the same whether a contract is named as a file or a directory.
{ pkgs, common, plain, typed, fromDir }:

pkgs.runCommand "${common.pname}-mk-clients-tests" { meta = common.meta; } ''
  set -eu
  fail() { echo "FAIL: $*" >&2; exit 1; }

  for d in ${plain} ${typed}; do
    [ "$(ls $d | sort | tr '\n' ' ')" = "typed_probe_api.cpp typed_probe_api.h " ] \
      || { ls -la $d >&2; fail "$d holds more than the client pair"; }
    grep -q 'explicit TypedProbe(const std::string& origin);' $d/typed_probe_api.h \
      || fail "$d: the client is not built from an origin"
  done
  echo "OK: one client pair per contract"

  grep -q 'LogosList echoInts(const LogosList& values' ${plain}/typed_probe_api.h \
    || fail "the default clients typed a collection"
  grep -q 'std::vector<int64_t> echoInts(const std::vector<int64_t>& values' \
    ${typed}/typed_probe_api.h \
    || fail "typedCollections = true left [int] untyped"
  echo "OK: typedCollections selects the typed surface"

  diff -r ${plain} ${fromDir} || fail "a directory input generated different clients"
  echo "OK: a module's lidl directory works as its file does"

  mkdir -p $out
  echo "lib.mkClients tests passed" > $out/result.txt
''
