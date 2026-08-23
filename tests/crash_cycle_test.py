#!/usr/bin/env python3
"""
crash_cycle_test.py   *** NEW FILE - added during the audit pass ***

Repeated SIGKILL crash-and-recover cycles against a PERSISTENT database.

Unlike chaos_test.py / final_stress_test.py, the database is created once and
then survives every cycle, so from cycle 2 onward tree_open() has to recover a
non-empty WAL left behind by the previous kill.  That is the path where
wal_recover() + wal_clear() actually run.

After every kill the tree must still contain every key the workload
acknowledged in the tracker file, and validate_tree() must pass.

Usage: python3 tests/crash_cycle_test.py [iterations] [seed]
"""

import os
import random
import subprocess
import sys
import time

BIN = "./tests/bin/crash_cycle.exe" if os.name == "nt" else "./tests/bin/crash_cycle"


def main():
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 20260823
    random.seed(seed)

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(root)

    if not os.path.exists(BIN):
        print("missing %s - run: make %s" % (BIN, BIN))
        return 2

    print("Crash-recovery cycles against a persistent DB "
          "(%d iterations, seed %d)\n" % (iterations, seed))

    r = subprocess.run([BIN, "init"], capture_output=True, text=True)
    if r.returncode != 0:
        print("init failed:", r.stdout, r.stderr)
        return 2

    failures = []
    for i in range(1, iterations + 1):
        kill_after = random.uniform(0.01, 0.30)
        p = subprocess.Popen([BIN, "run"],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(kill_after)
        p.kill()
        p.wait()

        r = subprocess.run([BIN, "verify"], capture_output=True, text=True)
        out = (r.stdout or "").strip()
        err = (r.stderr or "").strip()

        if r.returncode == 0:
            print("cycle %2d  kill@%.3fs  OK    %s" % (i, kill_after, out))
        else:
            failures.append(i)
            print("cycle %2d  kill@%.3fs  FAIL  %s" % (i, kill_after, out))
            if err:
                print("          stderr: %s" % err[:900])

    print("\n%d/%d cycles passed, %d failed" %
          (iterations - len(failures), iterations, len(failures)))
    if failures:
        print("failed cycles: %s" % failures)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
