# Asserts that the cdylib backend DEFINES every module-impl C ABI export
# logos-protocol DECLARES.
#
# Why this is not implied by anything else we build. logos-protocol only
# declares the ABI (the LOGOS_MODULE_IMPL_EXPORT functions in
# cpp/logos_module_impl.h); this repo's generator writes the definitions. Those
# are independent facts and the gap has shipped twice — grant_host_services at
# protocol 0.3, the teardown pair at 0.5 — each time with the header and the
# generator in perfect agreement about the protocol VERSION and in silent
# disagreement about the symbol list.
#
# It hides because nothing on the build path looks at it. An undefined symbol in
# an ELF shared object is legal at link time, so a module links clean; nixpkgs
# hardens with -Wl,-z,now, which makes resolution eager and therefore fatal at
# dlopen(). macOS links plugins with -undefined dynamic_lookup and never binds
# at all, so a green Darwin build proves nothing — and this check is the only
# thing in the repo that would notice on a Mac. The runtime then reports the
# module as LOADED and what an operator sees is other modules timing out.
#
# The declared list and the protocol version both come from ONE logos-protocol
# input, so there is no version arithmetic here and nothing hardcoded: "what
# this protocol requires" is just "what the header at this pin declares".
{ pkgs, common, src, generator, module-impl-abi }:

pkgs.runCommand "${common.pname}-module-impl-abi-tests"
  {
    nativeBuildInputs = [ generator module-impl-abi pkgs.unifdef ];
    meta = common.meta;
  }
  ''
    # pipefail stated here rather than inherited: stdenv sets it today, the one
    # pipeline below that may legitimately match nothing turns it back off
    # around itself, and neither of those should depend on the ambient shell.
    set -eu -o pipefail
    fail() { echo "FAIL: $*" >&2; exit 1; }

    fixtures=${src}/tests/experimental/fixtures
    declared=${module-impl-abi}/exports.txt
    hint='cpp-generator/experimental/lidl_gen_cdylib.cpp, in lidlMakeModuleImplExports()'

    # The MINOR the generated `#if LOGOS_PROTOCOL_VERSION_MINOR >= N` guards are
    # resolved at. It comes from the SAME logos-protocol output as the export
    # list, so the two can never be read from different revisions.
    #
    # Parsed strictly: an unparseable version would hand unifdef an empty -D
    # value, every guard would evaluate false, and the check would then measure
    # a protocol nobody pinned.
    minor=$(cut -d. -f2 < ${module-impl-abi}/version)
    major=$(cut -d. -f1 < ${module-impl-abi}/version)
    case "x$minor" in
      x|x*[!0-9]*) fail "protocol version '$(cat ${module-impl-abi}/version)' has no numeric MINOR" ;;
    esac
    case "x$major" in
      x|x*[!0-9]*) fail "protocol version '$(cat ${module-impl-abi}/version)' has no numeric MAJOR" ;;
    esac
    echo "logos-protocol $(cat ${module-impl-abi}/version) declares $(wc -l < "$declared" | tr -d ' ') module-impl exports;" \
         "resolving generated code at LOGOS_PROTOCOL_VERSION_MINOR=$minor"

    # ── Resolve, then extract ────────────────────────────────────────────────
    #
    # The emitter writes the version guards as TEXT — they are resolved when the
    # MODULE compiles, not when the generator runs — so a plain grep of the
    # generated .cpp finds all ten exports at every protocol version and would
    # pass vacuously. unifdef is what turns the text into the symbol set a
    # module built against THIS protocol would actually export.
    #
    # unifdef's own failure mode is the second vacuity path: given an expression
    # it cannot evaluate it exits 0 and leaves the text alone, which looks
    # exactly like "nothing was conditional". Hence the residual-conditional
    # assertion below. (rc 0 = unchanged, 1 = changed, >=2 = error.)
    # $cfg_label is set by assert_config before each call: A and B both emit a
    # file called universal_mod_module_impl.cpp, so naming the file alone leaves
    # the reader unable to tell which configuration failed.
    cfg_label="(no configuration)"
    resolved_symbols() {   # <out-dir> <major> <minor> <src.cpp>...
      outdir="$1"; M="$2"; m="$3"; shift 3
      rm -rf "$outdir"; mkdir -p "$outdir"
      for f in "$@"; do
        set +e
        unifdef -DLOGOS_PROTOCOL_VERSION_MAJOR="$M" \
                -DLOGOS_PROTOCOL_VERSION_MINOR="$m" "$f" > "$outdir/$(basename "$f")"
        rc=$?
        set -e
        [ "$rc" -le 1 ] || fail "[$cfg_label] unifdef exited $rc on $f"
        if grep -nE '^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef|else|elif|endif)' \
             "$outdir/$(basename "$f")" >&2; then
          fail "[$cfg_label] unifdef left the conditionals above unresolved in $(basename "$f")"
        fi
      done
      # A DEFINITION only: column zero (the emitter writes them at file scope
      # inside `extern "C"`), a name immediately followed by "(", and no
      # trailing ";". Deliberately strict rather than generous — `logos_module_
      # emit_cb g_emitCb` and `#include "logos_module_impl.h"` are both in this
      # file, and counting a mention as a definition is how this check would go
      # green over a missing export. If the emitter ever reflows its output past
      # this pattern the extraction UNDER-reports, which fails the diff loudly;
      # there is no shape of generated code that makes it over-report.
      #
      # pipefail is off for the pipe itself: matching nothing is a legitimate
      # outcome (the events sidecar defines no export, by design), and stdenv
      # runs builders with `set -o pipefail`, which turns that grep's exit 1
      # into a build failure with no diagnostic at all. Emptiness is judged by
      # the callers below and by logos-module-impl-diff, never by the pipe.
      set +o pipefail
      cat "$outdir"/*.cpp < /dev/null \
        | grep -E '^[^[:space:]/#].*[[:space:]*&]logos_module_[a-z0-9_]+[[:space:]]*\(' \
        | grep -vE ';[[:space:]]*$' \
        | grep -oE 'logos_module_[a-z0-9_]+[[:space:]]*\(' \
        | sed 's/[[:space:]]*($//' \
        | sort -u > "$outdir.txt"
      set -o pipefail
    }

    assert_config() {   # <label> <generated-dir>
      label="$1"; dir="$2"; cfg_label="$label"
      # nixpkgs builders run with `shopt -s nullglob`, so a non-matching
      # "$dir"/*.cpp expands to NOTHING rather than staying literal: `ls` would
      # then list the cwd and exit 0, and this guard would never fire. Count the
      # expansion itself instead.
      set -- "$dir"/*.cpp
      [ "$#" -gt 0 ] || fail "[$label] the generator emitted no .cpp at all"

      # Every .cpp the generator wrote, because that is what a module compiles.
      resolved_symbols "$dir/at-$minor"  "$major" "$minor" "$dir"/*.cpp
      resolved_symbols "$dir/at-0"       "$major" 0        "$dir"/*.cpp
      # A hypothetical NEXT major. Nothing pins this to 1 in particular — it is
      # simply "one past whatever we are on", which is the version where a
      # MINOR-only guard breaks.
      resolved_symbols "$dir/at-nextmaj" "$((major + 1))" 0 "$dir"/*.cpp
      n=$(wc -l < "$dir/at-$minor.txt" | tr -d ' ')
      n0=$(wc -l < "$dir/at-0.txt" | tr -d ' ')

      # THE anti-vacuity assertion, and the reason the pipeline is run twice.
      # Resolving the same sources with every guard forced false must yield a
      # strictly smaller set. If unifdef silently no-opped, or the extraction
      # were matching mentions rather than definitions, or the version never
      # reached unifdef at all, the two runs would agree and this fires. Note
      # what it does NOT do: name a symbol or a version. It only insists the
      # conditionals are live, which is what makes the diff below meaningful.
      if [ "$minor" -ge 3 ]; then
        [ -z "$(comm -13 "$dir/at-$minor.txt" "$dir/at-0.txt")" ] \
          || fail "[$label] MINOR=0 defines exports MINOR=$minor does not"
        [ "$n0" -lt "$n" ] \
          || fail "[$label] $n0 exports at MINOR=0 and $n at MINOR=$minor: the guards are inert"
      else
        # Below 0.3 nothing is gated, so the live assertion is that the two
        # resolutions are IDENTICAL. Never a skip: a skipped probe is the same
        # unexamined green this whole file exists to prevent.
        cmp -s "$dir/at-0.txt" "$dir/at-$minor.txt" \
          || fail "[$label] protocol 0.$minor gates no export, yet the two resolutions differ"
      fi
      echo "  [$label] version probe: $n0 exports at MINOR=0, $n at MINOR=$minor"

      # THE MAJOR PROBE. Every conditional surface here appeared at a MINOR, so
      # the natural guard is `LOGOS_PROTOCOL_VERSION_MINOR >= N` — and that is
      # wrong, because at the next MAJOR the MINOR resets to 0 and every such
      # guard silently goes false.
      #
      # It is worth being precise about why that is worse than it sounds. It is
      # not a link error and not a dlopen failure: the definitions and the calls
      # that reach them are guarded the same way, so they vanish TOGETHER and
      # everything still builds and loads. The only symptom is modules quietly
      # losing teardown and grantability, with no diagnostic anywhere. Nothing
      # else in this file would notice, because every other probe resolves at
      # the CURRENT major.
      #
      # So: at one major up, the export set must be complete. This assertion
      # fails against a MINOR-only guard and passes against a MAJOR-aware one,
      # which is the whole reason it exists.
      if ! cmp -s "$dir/at-nextmaj.txt" <(sort -u "$declared"); then
        {
          echo "FAIL: [$label] the export set is not complete at protocol $((major + 1)).0."
          echo
          echo "  DECLARED but NOT emitted once the MAJOR advances:"
          comm -13 "$dir/at-nextmaj.txt" <(sort -u "$declared") | sed 's/^/      - /'
          echo
          echo "  This is a version guard testing LOGOS_PROTOCOL_VERSION_MINOR without"
          echo "  LOGOS_PROTOCOL_VERSION_MAJOR. At $((major + 1)).0 the MINOR is 0, so the"
          echo "  guard goes false and the surface disappears — silently, because the"
          echo "  matching calls are guarded the same way and disappear with it."
          echo
          echo "  Emit the arithmetic expanded, not behind a macro (unifdef must be"
          echo "  able to evaluate it):"
          echo "    #if defined(LOGOS_PROTOCOL_VERSION_MINOR) && (LOGOS_PROTOCOL_VERSION_MAJOR > 0 || (LOGOS_PROTOCOL_VERSION_MAJOR == 0 && LOGOS_PROTOCOL_VERSION_MINOR >= N))"
          echo "  In: $hint"
        } >&2
        exit 1
      fi

      # The events sidecar is a second TU emitted next to the exports one. It
      # must carry no module-impl symbol of its own: two TUs defining the same
      # export is a duplicate-symbol link error, and an export that MOVED there
      # would still be found by the diff above, so only this says where it lives.
      for ev in "$dir"/*_events_cdylib.cpp; do
        [ -e "$ev" ] || continue
        resolved_symbols "$dir/events" "$minor" "$ev"
        [ ! -s "$dir/events.txt" ] \
          || { cat "$dir/events.txt" >&2
               fail "[$label] the events sidecar defines module-impl exports (above)"; }
        echo "  [$label] the events sidecar defines no module-impl export"
      done

      # Refuses an empty file on either side, and allows extra symbols on ours:
      # declared must be a SUBSET of defined, not equal to it.
      logos-module-impl-diff "$declared" "$dir/at-$minor.txt" "$label" "$hint"
    }

    mkdir -p work && cd work

    # ── A. Header-first ──────────────────────────────────────────────────────
    logos-cpp-generator --from-header "$fixtures/universal_impl.h" \
      --impl-class UniversalImpl --metadata "$fixtures/universal_metadata.json" \
      --backend cdylib --output-dir ./A >/dev/null \
      || fail "A: --from-header was refused"
    assert_config "A: --from-header, header-first" ./A

    # ── B. Contract-first ────────────────────────────────────────────────────
    # A SEPARATE branch of main.cpp, reached only by --lidl, and the one a
    # module whose contract is committed as .lidl actually takes. It shares the
    # emitter with A today; nothing but this check says it still does. Fed the
    # .lidl A itself wrote, so the two configurations describe one module.
    logos-cpp-generator --lidl ./A/universal_mod.lidl --backend cdylib \
      --impl-class UniversalImpl --impl-header universal_impl.h --output-dir ./B >/dev/null \
      || fail "B: --lidl --backend cdylib was refused (it needs --impl-class/--impl-header)"
    assert_config "B: --lidl, contract-first" ./B

    # ── C. Zero methods ──────────────────────────────────────────────────────
    # The exports are a fixed surface, not something accumulated per method. A
    # module with an empty impl class must still define the whole set — it is
    # the shape most likely to lose one to an emitter that only writes what it
    # thinks it needs. (The generator warns about the empty class; expected.)
    logos-cpp-generator --from-header "$fixtures/empty_class_impl.h" \
      --impl-class EmptyClassImpl --metadata "$fixtures/empty_metadata.json" \
      --backend cdylib --output-dir ./C >/dev/null 2>&1 \
      || fail "C: the zero-method fixture was refused"
    assert_config "C: zero-method module" ./C

    # ── D. Records + events ──────────────────────────────────────────────────
    # Records pull in the generated codec and events add the second TU, which
    # together are what change the emitted file set. The metadata is written
    # here rather than added to tests/experimental/fixtures because it exists
    # only to give a records fixture an event; the impl header is the shared one.
    cat > d_metadata.json <<'EOF'
    {
      "name": "records_events_mod",
      "version": "1.0.0",
      "type": "core",
      "dependencies": [],
      "events": [
        { "name": "blobStored", "params": [ { "name": "id", "type": "std::string" } ] }
      ]
    }
    EOF
    logos-cpp-generator --from-header "$fixtures/records_impl.h" \
      --impl-class RecordsImpl --metadata ./d_metadata.json \
      --backend cdylib --output-dir ./D >/dev/null \
      || fail "D: the records+events fixture was refused"
    assert_config "D: records + events" ./D

    mkdir -p "$out"
    echo "the cdylib backend defines every module-impl export logos-protocol $(cat ${module-impl-abi}/version) declares" \
      > "$out/result.txt"
  ''
