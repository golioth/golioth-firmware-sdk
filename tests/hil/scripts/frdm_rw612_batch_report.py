#!/usr/bin/env python3
"""Report frdm_rw612 test flakiness per branch for the "Test: frdm_rw612 batch" workflow.

Fetches every run of the workflow on the selected branches with the gh CLI (job
list, job logs, test artifacts), classifies each failed test configuration by how
far the device got, and prints failure rates and Ethernet bring-up timing per
branch, compared against a baseline branch with Fisher's exact test.

Failure layers:
  A net:*           Ethernet/Wi-Fi bring-up (PHY autonegotiation, link, DHCP)
  B shell:*         device shell never answered the harness
  C post-connect:*  device was connected, a cloud interaction timed out or failed
  D infra:*         no test result (artifacts, job timeout, wrong runner)

Everything fetched is kept in --cache, and --offline re-runs the analysis on it.
"""

import argparse
import io
import json
import math
import re
import statistics
import subprocess
import sys
import time
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

ANSI = re.compile(r"\x1b\[[0-9;]*[a-zA-Z]")
TS = re.compile(r"\[(\d\d):(\d\d):(\d\d)\.(\d{3}),(\d{3})\]")
JOB_NAME = re.compile(r"^(int|smp) (\S+) \((\d+)\)$")


def clean(text):
    return ANSI.sub("", text).replace("\r", "")


class Fetcher:
    def __init__(self, repo, cache, offline):
        self.repo = repo
        self.cache = cache
        self.offline = offline

    def _gh(self, path, binary=False):
        for attempt in range(8):
            proc = subprocess.run(["gh", "api", f"repos/{self.repo}/{path}"]
                                  + (["--allow-escape-sequences"] if "/logs" in path else []),
                                  capture_output=True)
            if proc.returncode == 0:
                return proc.stdout if binary else json.loads(proc.stdout)
            err = (proc.stdout + proc.stderr).decode(errors="ignore")
            if "HTTP 404" in err or "HTTP 410" in err:
                return None
            time.sleep(5 + 10 * attempt)
        sys.exit(f"gh api {path} kept failing")

    def _paged(self, path, key):
        items = []
        page = 1
        while True:
            sep = "&" if "?" in path else "?"
            data = self._gh(f"{path}{sep}per_page=100&page={page}")
            if not data:
                break
            items += data[key]
            if len(data[key]) < 100:
                break
            page += 1
        return items

    def cached_json(self, name, path, key):
        fn = self.cache / name
        if fn.exists():
            return json.loads(fn.read_text())
        if self.offline:
            return []
        items = self._paged(path, key)
        fn.write_text(json.dumps(items))
        return items

    def cached_bytes(self, name, path):
        fn = self.cache / name
        if fn.exists():
            return fn.read_bytes()
        if self.offline:
            return None
        data = self._gh(path, binary=True)
        if data is not None:
            fn.write_bytes(data)
        return data

    def runs(self, workflow_file, branches):
        runs = []
        for branch in branches:
            name = f"runs-{branch.replace('/', '_')}.json"
            fn = self.cache / name
            if fn.exists() and self.offline:
                runs += json.loads(fn.read_text())
                continue
            if self.offline:
                continue
            items = self._paged(f"actions/workflows/{workflow_file}/runs?branch={branch}",
                                "workflow_runs")
            items = [{k: r[k] for k in ("id", "head_branch", "head_sha", "run_attempt",
                                        "status", "conclusion", "created_at")} for r in items]
            fn.write_text(json.dumps(items))
            runs += items
        return runs


def phy_bucket(seg):
    if "Failed to get phy@" in seg or "Failed to read from" in seg or "Failed to write to" in seg:
        return "phy-state-error"
    if "autonegotiation timed out" in seg and "Link is up" not in seg:
        return "autoneg-timeout"
    if "is entering autonegotiation sequence" in seg and "Link is up" not in seg:
        return "autoneg-no-result"
    if "Waiting for link to be up" in seg and "Link is up" not in seg:
        return "link-never-up"
    if "Starting DHCP" in seg and "net_dhcpv4: Received" not in seg:
        return "dhcp-no-lease"
    return None


def last_boot(con):
    return con.split("*** Booting Zephyr OS")[-1]


def first_failing_stmt(text):
    m = re.search(r"\n_{3,} (?:ERROR at setup of )?test_\w+ _{3,}\n(.*?)(?=\n_{3,} |\n={5,} )", text, re.S)
    if not m:
        return ""
    candidate = ""
    for line in m.group(1).splitlines():
        if line.startswith(">"):
            candidate = line[1:].strip()
        elif re.match(r"^tests/\S*test_\w+\.py:\d+: ", line) and candidate:
            return candidate
    return ""


def ethernet_boots(con):
    """Per-boot Ethernet bring-up measurements from one console capture."""
    boots = []
    for seg in con.split("*** Booting Zephyr OS"):
        if "is entering autonegotiation sequence" not in seg:
            continue
        ev = {"timeouts": 0}
        for line in seg.splitlines():
            m = TS.search(line)
            if not m:
                continue
            h, mi, s, ms, us = map(int, m.groups())
            t = h * 3600 + mi * 60 + s + ms / 1e3 + us / 1e6
            if "is entering autonegotiation sequence" in line:
                ev.setdefault("an_start", t)
            elif "autonegotiation timed out" in line:
                ev["timeouts"] += 1
            elif "eth_nxp_enet_mac: Link is up" in line:
                ev.setdefault("link_up", t)
            elif "Failed to get phy@" in line or "Failed to read from" in line:
                ev["phy_error"] = True
            elif "net_dhcpv4: Received" in line:
                ev.setdefault("dhcp", t)
        boots.append(ev)
    return boots


def int_console(zip_bytes):
    if not zip_bytes:
        return ""
    z = zipfile.ZipFile(io.BytesIO(zip_bytes))
    results = []
    for n in z.namelist():
        if n.endswith("-result.json"):
            d = json.loads(z.read(n))
            results.append((d.get("start", 0), [a["source"] for a in d.get("attachments", [])]))
    out = []
    for _, atts in sorted(results):
        for a in atts:
            if a in z.namelist():
                out.append(clean(z.read(a).decode(errors="ignore")))
    return "\n".join(out)


def classify_int(text, con):
    stmt = first_failing_stmt(text)
    summary = re.findall(r"^(?:FAILED|ERROR) \S+::(\w+) - (.*)$", text, re.M)
    if not summary:
        if "exceeded the maximum execution time" in text or "has timed out" in text:
            return "infra:job-timeout"
        return "infra:no-pytest-result"
    if "set_golioth_psk_credentials" in stmt or "send_cmd" in stmt or "wait_for_prompt" in stmt:
        return "shell:no-prompt" if "uart:~$" not in con else "shell:cmd-timeout"
    pb = phy_bucket(last_boot(con))
    if pb:
        return "net:" + pb
    if any("ReadTimeout" in e or "RPCTimeout" in e for _, e in summary):
        return "post-connect:rpc-http-timeout"
    return "post-connect:" + summary[0][1].split(":")[0].strip()[:40]


def classify_smp_config(status, con, harness, wifi):
    if status == "passed":
        return "pass"
    if status in ("skipped", "filtered", "not run"):
        return "not-run"
    if not con.strip():
        return "infra:no-console"
    if "uart:~$" not in con and "Booting Zephyr" not in con:
        return "shell:no-prompt"
    seg = last_boot(con)
    if wifi:
        if "Golioth CoAP client connected" not in seg:
            if "net_dhcpv4: Received" in seg:
                return "net:wifi-ip-no-connect"
            if "Starting DHCP" in seg:
                return "net:wifi-dhcp-no-lease"
            return "net:wifi-not-associated"
    else:
        pb = phy_bucket(seg)
        if pb:
            return "net:" + pb
    m = re.search(r"^E\s+(\w+(?:\.\w+)*(?:Error|Exception|Timeout)\w*)", harness, re.M)
    return "post-connect:" + (m.group(1).split(".")[-1] if m else "assert/other")


def sample_configs(zip_bytes):
    if not zip_bytes:
        return {}
    z = zipfile.ZipFile(io.BytesIO(zip_bytes))
    names = z.namelist()
    statuses = {}
    for n in names:
        if n.endswith("twister-out/twister.json"):
            for ts in json.loads(z.read(n)).get("testsuites", []):
                statuses[ts["name"]] = ts.get("status")
    out = {}
    for name, status in statuses.items():
        con = harness = ""
        for n in names:
            if n.endswith(f"/{name}/handler.log"):
                con = clean(z.read(n).decode(errors="ignore"))
            elif n.endswith(f"/{name}/twister_harness.log"):
                harness = clean(z.read(n).decode(errors="ignore"))
        out[name] = (status, con, harness)
    return out


def fisher_two_sided(a, b, c, d):
    """p-value for the 2x2 table [[a, b], [c, d]]."""
    n = a + b + c + d
    row1, col1 = a + b, a + c

    def p(x):
        return math.comb(col1, x) * math.comb(n - col1, row1 - x) / math.comb(n, row1)

    observed = p(a)
    lo, hi = max(0, row1 - (n - col1)), min(row1, col1)
    return min(1.0, sum(p(x) for x in range(lo, hi + 1) if p(x) <= observed * (1 + 1e-9)))


def collect(fetcher, runs, expected_runner):
    rows = []
    boots = []
    for run in runs:
        rid = run["id"]
        jobs = fetcher.cached_json(f"jobs-{rid}.json",
                                   f"actions/runs/{rid}/jobs?filter=all", "jobs")
        arts = fetcher.cached_json(f"artifacts-{rid}.json",
                                   f"actions/runs/{rid}/artifacts", "artifacts")
        art_ids = {a["name"]: a["id"] for a in arts if not a.get("expired")}
        for job in jobs:
            m = JOB_NAME.match(job["name"])
            if not m or job.get("conclusion") not in ("success", "failure"):
                continue
            suite, test, iteration = m.groups()
            attempt = job.get("run_attempt", 1)
            base = dict(branch=run["head_branch"], run=rid, attempt=attempt, suite=suite,
                        test=test, iteration=int(iteration), runner=job.get("runner_name"),
                        job_conclusion=job["conclusion"])
            if job.get("runner_name") != expected_runner:
                rows.append(dict(base, config=f"{suite}.{test}", wifi=False, cat="infra:wrong-runner"))
                continue
            log = fetcher.cached_bytes(f"log-{job['id']}.txt", f"actions/jobs/{job['id']}/logs")
            text = "\n".join(ln[29:] if re.match(r"^\d{4}-\d\d-\d\dT", ln) else ln
                             for ln in clean((log or b"").decode(errors="ignore")).splitlines())
            if suite == "int":
                aname = f"allure-reports-hil-zephyr-frdm_rw612-{test}-{iteration}-{attempt}"
                zb = fetcher.cached_bytes(f"art-{rid}-{aname}.zip", f"actions/artifacts/{art_ids[aname]}/zip") \
                    if aname in art_ids else None
                con = int_console(zb)
                cat = "pass" if job["conclusion"] == "success" else classify_int(text, con)
                rows.append(dict(base, config=f"int.{test}", wifi=False, cat=cat))
                boots += [dict(b, branch=run["head_branch"], source=f"int.{test}") for b in ethernet_boots(con)]
            else:
                aname = f"twister-run-artifacts-frdm_rw612-{test}-{iteration}-{attempt}"
                zb = fetcher.cached_bytes(f"art-{rid}-{aname}.zip", f"actions/artifacts/{art_ids[aname]}/zip") \
                    if aname in art_ids else None
                configs = sample_configs(zb)
                if not configs:
                    cat = "pass" if job["conclusion"] == "success" else "infra:no-twister-result"
                    rows.append(dict(base, config=f"smp.{test}", wifi=False, cat=cat))
                    continue
                for cfg, (status, con, harness) in configs.items():
                    wifi = cfg.endswith(".wifi")
                    cat = classify_smp_config(status, con, harness, wifi)
                    if cat == "not-run":
                        continue
                    rows.append(dict(base, config=cfg, wifi=wifi, cat=cat))
                    if not wifi:
                        boots += [dict(b, branch=run["head_branch"], source=cfg) for b in ethernet_boots(con)]
    return rows, boots


def pct(n, d):
    return f"{100 * n / d:5.1f}%" if d else "    -"


def report(rows, boots, baseline):
    branches = sorted({r["branch"] for r in rows})
    print("## Jobs and configurations\n")
    print("| branch | jobs | failed jobs | Ethernet configs | A net | B shell | C post-connect | D infra | Wi-Fi configs | Wi-Fi failed |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    stats = {}
    for br in branches:
        rs = [r for r in rows if r["branch"] == br and r["cat"] != "infra:wrong-runner"]
        jobs = {(r["run"], r["attempt"], r["suite"], r["test"], r["iteration"]): r["job_conclusion"] for r in rs}
        jf = sum(1 for c in jobs.values() if c == "failure")
        eth = [r for r in rs if not r["wifi"]]
        wifi = [r for r in rs if r["wifi"]]
        layer = Counter(r["cat"].split(":")[0] for r in eth if r["cat"] != "pass")
        int_jobs = [r for r in rs if r["suite"] == "int"]
        stats[br] = dict(jobs=len(jobs), jobs_failed=jf, eth=len(eth), eth_a=layer["net"],
                         int_jobs=len(int_jobs), int_b=sum(1 for r in int_jobs if r["cat"].startswith("shell:")),
                         eth_failed=sum(layer.values()))
        wf = sum(1 for r in wifi if r["cat"] != "pass")
        print(f"| {br} | {len(jobs)} | {jf} ({pct(jf, len(jobs)).strip()}) | {len(eth)} | "
              f"{layer['net']} ({pct(layer['net'], len(eth)).strip()}) | {layer['shell']} | "
              f"{layer['post-connect']} | {layer['infra']} | {len(wifi)} | {wf} |")
    wrong = Counter(r["runner"] for r in rows if r["cat"] == "infra:wrong-runner")
    if wrong:
        print(f"\nJobs excluded because they ran on another runner: {dict(wrong)}")

    print("\n## Failure categories\n")
    for br in branches:
        c = Counter(r["cat"] for r in rows if r["branch"] == br and r["cat"] not in ("pass", "infra:wrong-runner"))
        print(f"- {br}: " + (", ".join(f"{k} {v}" for k, v in c.most_common()) or "none"))

    print("\n## Ethernet bring-up per boot\n")
    print("| branch | boots | link up | autoneg→link s (min/median/p90/max) | boots with timeout | of those recovered | PHY errors |")
    print("|---|---|---|---|---|---|---|")
    for br in branches:
        bs = [b for b in boots if b["branch"] == br]
        dur = sorted(b["link_up"] - b["an_start"] for b in bs if "link_up" in b and "an_start" in b)
        up = sum(1 for b in bs if "link_up" in b)
        to = [b for b in bs if b["timeouts"]]
        rec = sum(1 for b in to if "link_up" in b)
        q = (f"{dur[0]:.1f} / {statistics.median(dur):.1f} / {dur[int(0.9 * (len(dur) - 1))]:.1f} / {dur[-1]:.1f}"
             if dur else "-")
        print(f"| {br} | {len(bs)} | {up} | {q} | {len(to)} | {rec} | {sum(1 for b in bs if b.get('phy_error'))} |")

    if baseline in stats:
        print(f"\n## Compared with {baseline} (Fisher's exact test, two-sided)\n")
        b = stats[baseline]
        for br in branches:
            if br == baseline:
                continue
            s = stats[br]
            p_a = fisher_two_sided(s["eth_a"], s["eth"] - s["eth_a"], b["eth_a"], b["eth"] - b["eth_a"])
            p_b = fisher_two_sided(s["int_b"], s["int_jobs"] - s["int_b"], b["int_b"], b["int_jobs"] - b["int_b"])
            p_j = fisher_two_sided(s["jobs_failed"], s["jobs"] - s["jobs_failed"],
                                   b["jobs_failed"], b["jobs"] - b["jobs_failed"])
            print(f"- {br}: layer A {s['eth_a']}/{s['eth']} vs {b['eth_a']}/{b['eth']} (p={p_a:.3f}); "
                  f"layer B {s['int_b']}/{s['int_jobs']} vs {b['int_b']}/{b['int_jobs']} (p={p_b:.3f}); "
                  f"failed jobs {s['jobs_failed']}/{s['jobs']} vs {b['jobs_failed']}/{b['jobs']} (p={p_j:.3f})")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default="golioth/golioth-firmware-sdk")
    ap.add_argument("--workflow-file", default="test-frdm-rw612-batch.yml")
    ap.add_argument("--branches", nargs="+", default=["rw612-batch/v0-baseline", "rw612-batch/v1-zephyr-patches",
                                                      "rw612-batch/v2-console", "rw612-batch/v3-autoneg-timeout"])
    ap.add_argument("--baseline", default="rw612-batch/v0-baseline")
    ap.add_argument("--expected-runner", default="jons_raspberry_pi")
    ap.add_argument("--cache", type=Path, default=Path("rw612-batch-cache"))
    ap.add_argument("--offline", action="store_true", help="only use what is already in --cache")
    ap.add_argument("--json", type=Path, help="also write per-configuration rows and per-boot data here")
    args = ap.parse_args()

    args.cache.mkdir(parents=True, exist_ok=True)
    fetcher = Fetcher(args.repo, args.cache, args.offline)
    runs = fetcher.runs(args.workflow_file, args.branches)
    if not runs:
        sys.exit("no runs found")
    rows, boots = collect(fetcher, runs, args.expected_runner)
    print(f"# frdm_rw612 batch report: {len(runs)} runs on {len({r['head_branch'] for r in runs})} branches\n")
    report(rows, boots, args.baseline)
    if args.json:
        args.json.write_text(json.dumps({"rows": rows, "boots": boots}, indent=1))


if __name__ == "__main__":
    main()
