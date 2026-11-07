# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py prove: each client under test/prove named *_genmc.c, run by GenMC under each memory model.

GenMC explores each execution that the model allows. It reports a failed assertion, a data race, or a thread
blocked in a sleep that no wake ends (genmc_host.h). A client that passes under one model and fails under another
names the order that the code needs.

GenMC runs in WSL on a Windows host and directly on Linux. Its source is at ~/src/genmc, at the tag GENMC_TAG. The
first run that needs it builds it and installs it to ~/.local. Each run writes its logs to one folder under build/.
"""

import glob
import os
import shlex
import subprocess

# The models: sequential consistency, x86, C11, the hardware model under C11, and Armv8.
GENMC = "~/.local/bin/genmc"
GENMC_MODELS = ["--sc", "--tso", "--rc11", "--imm", "--arm8"]
# GenMC's load annotation, spin-assume and loop jump threading read each address in a loop's condition, and stop on
# a constant address other than NULL. With the three off, each loop runs to the unroll bound.
GENMC_FLAGS = ["--disable-estimation", "--unroll=12", "--disable-load-annotation", "--disable-spin-assume",
               "--disable-loop-jump-threading"]
GENMC_SOURCE = "https://github.com/MPI-SWS/genmc.git"
# The release that the clients use. Each host builds this tag. Thus, each host runs the same checker.
GENMC_TAG = "v0.20.0"
# The packages GenMC builds against, LLVM 20 among those its source supports
GENMC_PACKAGES = "cmake llvm-20-dev libffi-dev zlib1g-dev libedit-dev bc g++-14 libzstd-dev libxml2-dev"
INCLUDES = ["include", "test/prove"]


def linux(root, command):
    """A shell command run on Linux: through WSL on a Windows host, directly on Linux."""
    if os.name == "nt":
        return subprocess.run(["wsl", "-e", "bash", "-lc", command], cwd=root, capture_output=True, text=True)
    return subprocess.run(["bash", "-lc", command], cwd=root, capture_output=True, text=True)


def linux_root(root, command):
    """A shell command run as root on Linux: through WSL's root user on a Windows host, through sudo on Linux."""
    if os.name == "nt":
        return subprocess.run(["wsl", "-u", "root", "-e", "bash", "-lc", command], cwd=root, capture_output=True,
                              text=True)
    return subprocess.run(["bash", "-lc", "sudo -n " + command], cwd=root, capture_output=True, text=True)


def genmc_ready(root):
    """None where GenMC runs, after a build where it is missing. The reason where a step failed."""
    if linux(root, "test -x %s" % GENMC).returncode == 0:
        return None
    print("  genmc     building at ~/src/genmc, installing to ~/.local")
    r = linux_root(root, "env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq %s" % GENMC_PACKAGES)
    if r.returncode != 0:
        return "its packages did not install: " + (r.stdout + r.stderr).strip()[-200:]
    steps = [
        "mkdir -p ~/src",
        "test -d ~/src/genmc/.git || git clone --depth 1 --branch %s %s ~/src/genmc" % (GENMC_TAG, GENMC_SOURCE),
        "cd ~/src/genmc && CC=gcc-14 CXX=g++-14 cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo "
        "-DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm -DCMAKE_INSTALL_PREFIX=$HOME/.local -B RelWithDebInfo -S .",
        "cd ~/src/genmc && cmake --build RelWithDebInfo -j 8 && cmake --install RelWithDebInfo",
    ]
    for step in steps:
        r = linux(root, step)
        # GenMC's configure names the LLVM it found, and a build that fails needs that version to read
        for line in (r.stdout + r.stderr).splitlines():
            if "Using LLVM" in line:
                print("  genmc     " + line.strip().lstrip("-").strip())
        if r.returncode != 0:
            # The compiler's own error lines name the cause. The last lines are only make's summary of them.
            text = r.stdout + r.stderr
            errors = [line.strip() for line in text.splitlines() if "error:" in line][:5]
            return "its build stopped at `%s`: %s" % (step.split("&&")[-1].strip(),
                                                     " | ".join(errors) if errors else text.strip()[-600:])
    return None


def genmc(root, client, model, run_root, name):
    """One client under one memory model: proved with its count of executions, or the first fault GenMC names."""
    flags = GENMC_FLAGS + ["--"] + ["-I " + i for i in INCLUDES]
    r = linux(root, "%s %s %s %s" % (GENMC, model, " ".join(flags), shlex.quote(client)))
    text = r.stdout + r.stderr
    with open(os.path.join(run_root, "%s_genmc_%s.txt" % (name, model.lstrip("-"))), "w", encoding="utf-8") as log:
        log.write(text)
    explored = [line for line in text.splitlines() if line.startswith("Number of complete executions explored")]
    blocked = [line for line in text.splitlines() if "blocked" in line.lower() and any(c.isdigit() and c != "0"
                                                                                     for c in line.split(":")[-1])]
    if "No errors were detected" in text and not blocked:
        return "proved, %s executions" % (explored[-1].split(":")[-1].strip() if explored else "?")
    faults = [line.strip() for line in text.splitlines()
              if line.startswith(("Error", "Assertion violation", "INTERNAL", "LLVM ERROR"))] + blocked
    return "FAILED: " + (faults[0] if faults else text.strip()[-200:])


def prove(root, run_root, names):
    """Each client, or the clients named, under each model. 0 where each one proved, 1 where any did not."""
    os.makedirs(run_root, exist_ok=True)
    failures = []
    clients = sorted(glob.glob(os.path.join(root, "test", "prove", "*_genmc.c")))
    clients = [c for c in clients if not names or os.path.basename(c)[:-len("_genmc.c")] in names]
    reason = genmc_ready(root) if clients else None
    if reason:
        failures.append("genmc: " + reason)
        clients = []
    for path in clients:
        name = os.path.basename(path)[:-len("_genmc.c")]
        client = os.path.relpath(path, root).replace("\\", "/")
        print("== %s" % name)
        for model in GENMC_MODELS:
            answer = genmc(root, client, model, run_root, name)
            print("  genmc %-6s %s" % (model.lstrip("-"), answer))
            if not answer.startswith("proved"):
                failures.append("%s genmc %s: %s" % (name, model.lstrip("-"), answer))
    print("prove: run in %s" % run_root)
    if failures:
        print("%d failures:" % len(failures))
        for failure in failures:
            print("  " + failure)
        return 1
    return 0
