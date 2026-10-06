#!/usr/bin/env bash
# Sourced by the two core builds; never modifies the pinned submodule.
prepare_core() {
    local target="$1" source_dir="$1/source" patch_file
    local patch_digest
    patch_digest="$(find "$ROOT/external/patches" -maxdepth 1 -name '*.patch' -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum)"
    [[ "$(git -C "$UPSTREAM" rev-parse HEAD)" == "$REVISION" ]] || { echo 'Unexpected core revision.' >&2; return 1; }
    mkdir -p "$target"
    if [[ ! -f "$target/source-revision" || "$(cat "$target/source-revision")" != "$REVISION $patch_digest" ]]; then
        # git archive overwrites upstream files but cannot remove files added by
        # an earlier local patch. Recreate only our verified disposable source
        # copy, otherwise an updated /dev/null patch fails on its existing file.
        local resolved_source
        resolved_source="$(realpath -m "$source_dir")"
        [[ "$resolved_source" == "$(realpath "$ROOT")/build/core-lab/source" ||
           "$resolved_source" == "$(realpath "$ROOT")/build/core-ps4/source" ]] || return 1
        if [[ -e "$source_dir" || -L "$source_dir" ]]; then
            rm -rf -- "$source_dir"
        fi
        mkdir -p "$source_dir"
        git -C "$UPSTREAM" archive "$REVISION" | tar -x -C "$source_dir"
        for patch_file in "$ROOT"/external/patches/*.patch; do
            [[ -f "$patch_file" ]] || continue
            patch --batch --forward -d "$source_dir" -p1 -i "$patch_file"
        done
        printf '%s\n' "$REVISION $patch_digest" > "$target/source-revision"
    fi
}
