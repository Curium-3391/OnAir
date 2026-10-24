# Shared compile cache and build batcher (Mac + build server)

Dev tooling for the many parallel agent worktrees. The scripts live outside the repo, in `~/.claude` on the Mac.

## 1. Shared ccache across worktrees

ccache only hits across trees when the preprocessed text is identical, and that text contains absolute paths
(`# 1 "/tmp/x/core/..."`). ccache turns paths under `base_dir` into paths relative to the compiler's cwd, so two trees hit each
other only when **source and build dir have the same relative position**:

| Machine | Layout | base_dir |
|---|---|---|
| Mac | `/private/tmp/NAME/src` (git worktree) + `/private/tmp/NAME/build` | `/private/tmp` |
| Server | `/root/work/FAMILY` (source) + `/root/work/FAMILY/build` | `/root/work` |

`/tmp` is a symlink to `/private/tmp` on macOS and cwd is always the real path, so the Mac trees must be configured with the
`/private/tmp/...` spelling (otherwise the relative path contains the tree name and nothing hits).

Settings (`ccache.conf`, also exported by the env files): `base_dir`, `hash_dir=false`, `direct_mode=false` (preprocessor mode:
`-I` is not hashed), `sloppiness=include_file_mtime,include_file_ctime,time_macros,pch_defines`, `compression=true`,
`ignore_options=-ffile-prefix-map=* -fdebug-prefix-map=* -fmacro-prefix-map=*` (CMakeLists adds `-ffile-prefix-map=<root>=onair`,
a per-tree absolute path that would otherwise change every hash; output is identical because the map is applied), cache 20 GB (Mac)
/ 15 GB (server). CMake: `CMAKE_{C,CXX,OBJCXX}_COMPILER_LAUNCHER=ccache` (the repo's `DECT2_CCACHE` does the same).
Rust: `RUSTC_WRAPPER=sccache CARGO_INCREMENTAL=0` when sccache is installed (Mac yes, server no).

Agent usage (Mac):

    source ~/.claude/onair-ccache.env
    zsh ~/.claude/onair-wt.sh NAME [REF] [NEW-BRANCH]      # worktree at /private/tmp/NAME/src
    onair_cmake NAME -DDECT2_BUILD_TESTS=OFF               # configures /private/tmp/NAME/build with Ninja
    zsh ~/.claude/onair-guard.sh cmake --build /private/tmp/NAME/build --target dect2

Server: `source /etc/profile.d/onair-ccache.sh` (non-login shells too) before `cmake`; keep the existing `/root/work/<family>/build`.

Measured (Mac, `dect2core`, 1190 compile steps, same commit in two worktrees `cct1`, `cct2`): first build 760 s, second build
118 s with 1210 hits / 68 misses (94.7 %). Before this, the cache hit rate across all builds was 49.8 % and different worktrees
never hit each other. Server (12 engine objects, two trees): 105 s cold, 5 s second (12/12 hits); server overall hit rate was 7 %.

## 2. Build batcher (Mac): `~/.claude/onair-build-batch.py`

    python3 ~/.claude/onair-build-batch.py submit --worktree PATH --target dect2 [--out BINARY_PATH]
    python3 ~/.claude/onair-build-batch.py status | stop

Use it where you now call guard + cmake. `submit` snapshots the worktree (committed + uncommitted + untracked vs its merge-base
with `pub/integ-31`, using a temporary index, so your index/tree are untouched), queues it and blocks until its result exists
(exit 0 = binary copied to `--out`; timeout 40 min). The daemon (started automatically) collects requests for 90 s; the batch
members are claimed only once `onair-guard.sh` grants a slot, so requests arriving while the queue is busy still join. Then, in one
throw-away tree `/private/tmp/onair-batch/src` at `pub/integ-31` and one persistent Ninja build dir `.../build`:
1. patches are applied in order with `git apply --3way`; conflicting ones are set aside,
2. the union of the targets is built once; on failure the set is bisected, so only the broken patch fails and the rest get their binary,
3. conflicting patches are built alone afterwards.
Results: `res/<id>.json` (status ok/failed/conflict/error, message, log tail); logs in `/private/tmp/onair-batch/log/`.

**The binary contains everyone's changes.** Pick your feature at run time with flags (`--shot`, `--style`, ... as before); do not
expect a binary that contains only your patch. Unit tests that need only your change should still run in your own tree.

Env overrides: `ONAIR_BATCH_DIR/REPO/BASE/HOLD/TIMEOUT/GUARD`.

Tested with 3 fake requests on a throw-away clone: two touching the same line (second one reported as conflict and built alone, binary
delivered), one with a syntax error (found by bisecting, log tail returned), all in one daemon run of about 3 minutes.
