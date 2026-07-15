#!/usr/bin/env python3
"""Step 5: static dependency structure of the sealed zone-interface headers.

Parses the #include "..." graph among the 16 Phase 0 sealed contract headers
(ZONES.md "Phase 0 -- Owns"), runs Kahn's topological sort, explicitly attempts
cycle detection, and reports node/edge counts, max dependency depth (longest path),
and the topological order. Emits bench_results/dependency_graph.json.
"""
import json
import os
import re
from collections import defaultdict, deque

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The 16 sealed Phase 0 interface headers, exactly as listed in ZONES.md Phase 0 Owns.
SEALED = [
    "src/backbone/event_router.hpp",
    "src/backbone/scenario_events.hpp",
    "src/backbone/screen_state.hpp",
    "src/backbone/modal_state.hpp",
    "src/backbone/animation_clock.hpp",
    "src/backbone/focus_manager.hpp",
    "src/persistence/sync_state.hpp",
    "src/engine/scenario_id.hpp",
    "src/engine/rng_seed.hpp",
    "src/settings/settings.hpp",
    "src/assets/asset_paths.hpp",
    "src/assets/tier_config.hpp",
    "src/audio/audio_paths.hpp",
    "src/persistence/persistence_schema.hpp",
    "src/persistence/auth0_config.hpp",
    "src/theme/theme_tokens.hpp",
]

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def project_includes(path):
    """Return the set of project-relative headers this file #includes (quoted form)."""
    with open(os.path.join(REPO, path), "r") as f:
        text = f.read()
    out = set()
    for inc in INCLUDE_RE.findall(text):
        # Includes in these files are project-root-relative (e.g. "backbone/foo.hpp").
        out.add("src/" + inc)
    return out


def main():
    node_set = set(SEALED)
    edges = []            # (A depends-on B)  == A includes B
    out_of_set = defaultdict(list)
    adj = defaultdict(set)      # A -> {B it includes, within set}
    indeg = {n: 0 for n in SEALED}

    for a in SEALED:
        incs = project_includes(a)
        for b in sorted(incs):
            if b in node_set:
                if b not in adj[a]:
                    adj[a].add(b)
                    edges.append((a, b))
                    indeg[b] += 0  # ensure key
            else:
                out_of_set[a].append(b)
    # recompute indegree over the "depends-on" edges: edge A->B means A needs B,
    # so for a build/topo order B must come first. Treat B as prerequisite.
    indeg = {n: 0 for n in SEALED}
    for a in SEALED:
        for b in adj[a]:
            indeg[a] += 1   # a has an incoming dependency requirement on b

    # Kahn's algorithm on the prerequisite graph: nodes with no unmet deps first.
    # Build reverse adjacency: for each b, which a's depend on it.
    dependents = defaultdict(set)
    for a in SEALED:
        for b in adj[a]:
            dependents[b].add(a)

    remaining_deps = {n: len(adj[n]) for n in SEALED}
    ready = deque(sorted([n for n in SEALED if remaining_deps[n] == 0]))
    topo = []
    while ready:
        n = ready.popleft()
        topo.append(n)
        for d in sorted(dependents[n]):
            remaining_deps[d] -= 1
            if remaining_deps[d] == 0:
                ready.append(d)
    # stable-ish ordering
    cycle = len(topo) != len(SEALED)
    cyclic_nodes = [n for n in SEALED if n not in topo] if cycle else []

    # Longest path (max dependency depth) via DFS memo on the "depends-on" DAG.
    depth_memo = {}

    def depth(n, stack):
        if n in depth_memo:
            return depth_memo[n]
        if n in stack:
            return -1  # cycle guard
        stack.add(n)
        best = 0
        for b in adj[n]:
            d = depth(b, stack)
            best = max(best, 1 + d)
        stack.discard(n)
        depth_memo[n] = best
        return best

    max_depth = 0 if cycle else max(depth(n, set()) for n in SEALED)

    result = {
        "node_count": len(SEALED),
        "edge_count": len(edges),
        "is_acyclic": not cycle,
        "cycle_detected": cycle,
        "cyclic_nodes": cyclic_nodes,
        "max_dependency_depth": max_depth,
        "topological_order_deps_first": topo,
        "edges_depends_on": [{"from": a, "to": b} for (a, b) in edges],
        "out_of_set_includes": {k: v for k, v in out_of_set.items()},
    }
    with open(os.path.join(REPO, "bench_results/dependency_graph.json"), "w") as f:
        json.dump(result, f, indent=2)

    print(f"nodes (sealed interface headers): {len(SEALED)}")
    print(f"edges (A includes B, both in set): {len(edges)}")
    print(f"acyclic (verified via Kahn's, all nodes emitted): {not cycle}")
    print(f"max dependency depth (longest include chain): {max_depth}")
    print("\nedges (A depends-on B):")
    for a, b in edges:
        print(f"  {a.split('/')[-1]:26s} -> {b.split('/')[-1]}")
    print("\ntopological order (prerequisites first):")
    for i, n in enumerate(topo):
        print(f"  {i+1:2d}. {n}")
    print("\nout-of-set project includes (interface header -> non-sealed header):")
    for a, v in out_of_set.items():
        print(f"  {a.split('/')[-1]}: {[x.split('/')[-1] for x in v]}")
    print("\nWrote bench_results/dependency_graph.json")


if __name__ == "__main__":
    main()
