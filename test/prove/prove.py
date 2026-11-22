# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py prove: the proof harnesses and the thread clients under test/prove, each under its provers.

Two arms hold them:

  cbmc       each harness named *_prove.c, a C program of inputs, assumptions and assertions (prove.h). Each one states
             theorems of theory/embedded_types on the C functions. CBMC unwinds it to the bound of its entry in
             CBMC_BOUNDS, with unwinding assertions, so a bound too small for a loop fails. It runs on each set of defines
             of the entry, one word width or target to each set. A second run with PROVE_REACH must fail: it asserts
             the end of the harness unreachable. A harness whose assumptions leave no path passes each assertion and
             proves nothing.
  genmc      each client named *_threads.c, two or three threads on the real headers, under each memory model in
             GENMC_MODELS. GenMC explores each execution that the model allows. It reports a failed assertion, a data
             race, or a thread asleep in a wait of thread_host.h that no later wake ends. A client that passes under
             one model and fails under another names the order that the code needs.

CBMC does not run the thread clients. Each call passes a Cfg compound literal that holds pointers, and the thread
model of CBMC takes each such literal as a shared pointer. CBMC then stops with
`pointer handling for concurrency is unsound`.

Each prover run has RUN_SECONDS of wall clock. At the limit, coreutils timeout stops the prover, and the run fails with
the limit named.

The provers run in WSL on a Windows host and directly on Linux. CBMC comes from the
distribution. GenMC comes from its source at ~/src/genmc, at the tag GENMC_TAG. The first run that needs it builds it
and installs it to ~/.local. Each run writes its logs to one folder under build/.
"""

import glob
import os
import shlex
import subprocess
import time

# The wall clock of each prover run, in seconds
RUN_SECONDS = 900
# The exit status of coreutils timeout where it stopped the command
TIMED_OUT = 124
# The arms, in the order that a run takes them
ARMS = ["cbmc", "genmc"]

# Each harness's bound: the loop unwind that CBMC holds it to, 0 for none, and the sets of defines that it runs on
CBMC_BOUNDS = {
    "atomic": {"unwind": 3, "defines": [[]]},
    "backoff": {"unwind": 0, "defines": [[], ["EMBED_WORD_BITS=32"], ["EMBED_WORD_BITS=16"]]},
    "lane_word": {"unwind": 5, "defines": [[], ["EMBED_WORD_BITS=32"]]},
    "stored_count": {"unwind": 0, "defines": [[], ["EMBED_TARGET=EMBED_TARGET_EMBEDDED"]]},
    "ticket_flags": {"unwind": 0, "defines": [[], ["EMBED_WORD_BITS=32"]]},
    "ticket_stamp": {"unwind": 0, "defines": [[], ["EMBED_WORD_BITS=32"]]},
}
CBMC = "cbmc"

# The models: sequential consistency, x86, C11, the hardware model under C11, and Armv8.
GENMC = "~/.local/bin/genmc"
GENMC_MODELS = ["--sc", "--tso", "--rc11", "--imm", "--arm8"]
# There is no unroll bound: each loop of a client ends by its finite passes and by the wakes of thread_host.h. The wait
# of thread_host.h is a loop that reads only the count of wakes. GenMC's spin-assume takes it as a wait, and
# --check-liveness reports an execution where no later wake ends it. A blocked execution is a branch where a read
# took a value that an assume refused, which GenMC drops; it is not a fault.
GENMC_FLAGS = ["--disable-estimation", "--check-liveness"]
# No --nthreads: the parallel exploration of GenMC corrupts its heap under the models that track dependencies, and it aborts.
GENMC_SOURCE = "https://github.com/MPI-SWS/genmc.git"
# The release that the clients use. Each host builds this tag. Thus, each host runs the same checker.
GENMC_TAG = "v0.20.0"
# The packages GenMC builds against, LLVM 20 among those its source supports
GENMC_PACKAGES = "cmake llvm-20-dev clang-20 libffi-dev zlib1g-dev libedit-dev bc g++-14 libzstd-dev libxml2-dev"
# The clang that GenMC runs at each check. It is a package, so a host with a GenMC build from a cache can lack it.
GENMC_CLANG = "/usr/lib/llvm-20/bin/clang"
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


def bounded(root, command):
    """A prover command run as linux runs it, stopped at RUN_SECONDS. The result, whether the limit stopped it, and
    the seconds that it took."""
    started = time.monotonic()
    r = linux(root, "timeout --kill-after=30 %d %s" % (RUN_SECONDS, command))
    return r, r.returncode == TIMED_OUT, time.monotonic() - started


def genmc_ready(root):
    """None where GenMC runs, after its packages and its build where either is absent. The reason where a step failed."""
    built = linux(root, "test -x %s" % GENMC).returncode == 0
    if not built or linux(root, "test -x %s" % GENMC_CLANG).returncode != 0:
        print("  genmc     its packages install")
        r = linux_root(root, "env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq %s" % GENMC_PACKAGES)
        if r.returncode != 0:
            return "its packages did not install: " + (r.stdout + r.stderr).strip()[-200:]
    if built:
        return None
    print("  genmc     its build runs at ~/src/genmc, and installs to ~/.local")
    steps = [
        "mkdir -p ~/src",
        "test -d ~/src/genmc/.git || git clone --depth 1 --branch %s %s ~/src/genmc" % (GENMC_TAG, GENMC_SOURCE),
        "cd ~/src/genmc && CC=gcc-14 CXX=g++-14 cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo "
        "-DCMAKE_PREFIX_PATH=/usr/lib/llvm-20 -DCMAKE_INSTALL_PREFIX=$HOME/.local -B RelWithDebInfo -S .",
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


def cbmc_ready(root):
    """None where CBMC runs, and the reason where it does not."""
    if linux(root, "command -v %s" % CBMC).returncode == 0:
        return None
    return "no cbmc on the path: the distribution package cbmc gives it"


def cbmc(root, source, unwind, defines, reach):
    """One run of CBMC on source: whether it reported success, the properties that failed, its text, and its
    seconds."""
    flags = ["-D " + d for d in defines] + ["-I " + i for i in INCLUDES]
    if unwind:
        flags += ["--unwind %d" % unwind, "--unwinding-assertions"]
    if reach:
        flags.append("-D PROVE_REACH")
    r, stopped, seconds = bounded(root, "%s %s %s" % (CBMC, " ".join(flags), shlex.quote(source)))
    text = r.stdout + r.stderr
    failed = [line.strip() for line in text.splitlines() if line.rstrip().endswith("FAILURE")]
    if stopped:
        failed.append("timed out after %d s" % RUN_SECONDS)
    return "VERIFICATION SUCCESSFUL" in text and not stopped, failed, text, seconds


def log_name(run_root, name, arm, label):
    """The path of the log of one run."""
    return os.path.join(run_root, "%s_%s_%s.txt" % (name, arm, label.replace("=", "_") or "default"))


def prove_harness(root, run_root, path, failures):
    """One harness under CBMC on each set of defines of its bound, each with its reach run."""
    name = os.path.basename(path)[:-len("_prove.c")]
    bound = CBMC_BOUNDS.get(name)
    if bound is None:
        failures.append("%s: no bound in CBMC_BOUNDS of test/prove/prove.py" % name)
        return
    source = os.path.relpath(path, root).replace("\\", "/")
    print("== %s" % name, flush=True)
    for defines in bound["defines"]:
        label = " ".join(defines)
        held, failed, text, seconds = cbmc(root, source, bound["unwind"], defines, reach=False)
        reached, _, _, reach_seconds = cbmc(root, source, bound["unwind"], defines, reach=True)
        with open(log_name(run_root, name, "cbmc", label), "w", encoding="utf-8") as log:
            log.write(text)
        print("  cbmc  %-36s %s (%.0f s, the reach run %.0f s)%s" % (
            label or "the default", "proved" if held else "FAILED", seconds, reach_seconds,
            "" if not reached else "  (VACUOUS: the end is unreachable)"), flush=True)
        for line in failed[:6]:
            print("      " + line)
        if not held:
            failures.append("%s cbmc %s: %s" % (name, label, "; ".join(failed[:3]) or "not proved"))
        if reached:
            failures.append("%s cbmc %s: the end of the harness is unreachable" % (name, label))


def genmc(root, client, model, run_root, name):
    """One client under one memory model: proved with its count of executions, or the first fault GenMC names."""
    flags = GENMC_FLAGS + ["--"] + ["-I " + i for i in INCLUDES]
    r, stopped, seconds = bounded(root, "%s %s %s %s" % (GENMC, model, " ".join(flags), shlex.quote(client)))
    text = r.stdout + r.stderr
    with open(log_name(run_root, name, "genmc", model.lstrip("-")), "w", encoding="utf-8") as log:
        log.write(text)
    if stopped:
        return "FAILED: timed out after %d s" % RUN_SECONDS
    explored = [line for line in text.splitlines() if line.startswith("Number of complete executions explored")]
    if "No errors were detected" in text and r.returncode == 0:
        return "proved, %s executions (%.0f s)" % (explored[-1].split(":")[-1].strip() if explored else "?", seconds)
    faults = [line.strip() for line in text.splitlines()
              if line.startswith(("Error", "Assertion violation", "INTERNAL", "LLVM ERROR"))]
    return "FAILED: " + (faults[0] if faults else text.strip()[-200:])


def prove(root, run_root, names, wanted_models=None, wanted_arms=None):
    """Each harness and client, or those named, under each arm, or the arms named, and GenMC under each model, or the
    models named. 0 where each one proved, 1 where any did not."""
    os.makedirs(run_root, exist_ok=True)
    failures = ["no model named %s" % w for w in (wanted_models or []) if "--" + w not in GENMC_MODELS]
    failures += ["no arm named %s" % w for w in (wanted_arms or []) if w not in ARMS]
    arms = [a for a in ARMS if not wanted_arms or a in wanted_arms]
    models = [m for m in GENMC_MODELS if not wanted_models or m.lstrip("-") in wanted_models]
    harnesses = sorted(glob.glob(os.path.join(root, "test", "prove", "*_prove.c")))
    harnesses = [h for h in harnesses if not names or os.path.basename(h)[:-len("_prove.c")] in names]
    clients = sorted(glob.glob(os.path.join(root, "test", "prove", "*_threads.c")))
    clients = [c for c in clients if not names or os.path.basename(c)[:-len("_threads.c")] in names]
    if failures:
        arms = []
    if "cbmc" in arms and harnesses:
        reason = cbmc_ready(root)
        if reason:
            failures.append("cbmc: " + reason)
            arms.remove("cbmc")
    if "genmc" in arms and clients:
        reason = genmc_ready(root)
        if reason:
            failures.append("genmc: " + reason)
            arms.remove("genmc")
    if "cbmc" in arms:
        for path in harnesses:
            prove_harness(root, run_root, path, failures)
    for path in clients if "genmc" in arms else []:
        name = os.path.basename(path)[:-len("_threads.c")]
        client = os.path.relpath(path, root).replace("\\", "/")
        print("== %s" % name, flush=True)
        for model in models:
            answer = genmc(root, client, model, run_root, name)
            print("  genmc %-6s %s" % (model.lstrip("-"), answer), flush=True)
            if not answer.startswith("proved"):
                failures.append("%s genmc %s: %s" % (name, model.lstrip("-"), answer))
    print("prove: run in %s" % run_root)
    if failures:
        print("%d failures:" % len(failures))
        for failure in failures:
            print("  " + failure)
        return 1
    return 0
