#!/usr/bin/env python3
"""
Parallel test framework for YoctoCC.

Usage:
    python3 run_tests_parallel.sh              # Run all tests
    python3 run_tests_parallel.sh arith        # Run tests matching "arith"
    python3 run_tests_parallel.sh arith pointer # Multiple filters (OR)

Environment:
    FORMAT=md       Output in Markdown format (default: simple, matches the
                     original bash script's terminal output 1:1)
"""

import os
import platform
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Tuple

# --- Configuration ---

TEST_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", "10"))
PARALLEL_JOBS = int(os.environ.get("PARALLEL_JOBS", os.cpu_count() or 4))
OUTPUT_MD = os.environ.get("FORMAT", "simple") == "md"
FILTERS = sys.argv[1:]


class C:
    GREEN, RED, YELLOW, BLUE, NC = "\033[0;32m", "\033[0;31m", "\033[1;33m", "\033[0;34m", "\033[0m"


def color(text: str, code: str) -> str:
    """Colorize for terminal output. No-op in Markdown mode (colors don't render there)."""
    return text if OUTPUT_MD else f"{code}{text}{C.NC}"


# --- Project paths ---

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
CASES_DIR = SCRIPT_DIR / "cases"
COMPILER = PROJECT_ROOT / "build" / "yoctocc"
TEST_HELPER_O = PROJECT_ROOT / "build" / "test_helper.o"

# --- Architecture detection: arm64 needs an x86-64 cross compiler + QEMU ---

UNAME_M = platform.machine()
if UNAME_M == "aarch64":
    X86_64_CC, QEMU_EXEC, MAKE_X86_FLAG = "x86_64-linux-gnu-gcc", "qemu-x86_64", "X86_64_CC=x86_64-linux-gnu-gcc"
else:
    X86_64_CC, QEMU_EXEC, MAKE_X86_FLAG = "gcc", "", ""

# Matches `ASSERT(expected, actual);` and captures both source expressions.
ASSERT_CALL_RE = re.compile(r"^\s*ASSERT\(([^,]+),\s*(.+)\)\s*;\s*$")
ASSERT_RESULT_RE = re.compile(r"ASSERT_RESULT\s+(PASS|FAIL)\s+#(\d+)\s+expected\s+(-?\d+)\s+actual\s+(-?\d+)")


# --- Data structures ---

@dataclass
class TestCase:
    num: int
    file: Path
    name: str
    expected_exit: int = 0


@dataclass
class AssertResult:
    index: int
    code: str       # the `actual` expression under test, from source
    expected: str   # expected value (runtime value if the assert ran, else the source literal)
    actual: str     # actual value if the assert ran, else "—"
    status: str     # PASS / FAIL / SKIP


@dataclass
class TestResult:
    name: str
    file: Path
    status: str = "PASS"   # PASS / FAIL
    reason: str = ""       # compile / assemble / link / timeout / result / noresult
    expected_exit: int = 0
    actual_exit: int = 0
    error_log: str = ""
    stderr_log: str = ""
    asserts: List[AssertResult] = field(default_factory=list)


# --- Build ---

def make(target: str, extra_flag: str = "") -> bool:
    cmd = ["make", "-s"] + ([extra_flag] if extra_flag else []) + [target]
    return subprocess.run(cmd, cwd=PROJECT_ROOT, capture_output=True).returncode == 0


# --- Test execution ---

def run_step(cmd: List[str]) -> Tuple[bool, str, Optional[int]]:
    """Run one subprocess step. Returns (ok, combined_output, returncode).
    returncode is None on timeout."""
    try:
        cp = subprocess.run(cmd, capture_output=True, text=True, timeout=TEST_TIMEOUT)
        return cp.returncode == 0, cp.stdout + cp.stderr, cp.returncode
    except subprocess.TimeoutExpired:
        return False, "", None


def run_single_test(tc: TestCase, work_dir: Path) -> TestResult:
    d = work_dir / f"t{tc.num}"
    d.mkdir(parents=True, exist_ok=True)
    asm, obj, binf = d / "a.s", d / "a.o", d / "a"
    r = TestResult(name=tc.name, file=tc.file, expected_exit=tc.expected_exit)

    build_steps = [
        ("compile", [str(COMPILER), str(tc.file), str(asm)]),
        ("assemble", [X86_64_CC, "-c", "-o", str(obj), str(asm)]),
        ("link", [X86_64_CC, "-no-pie", "-o", str(binf), str(obj), str(TEST_HELPER_O)]),
    ]
    for reason, cmd in build_steps:
        ok, out, rc = run_step(cmd)
        if rc is None:
            r.status, r.reason = "FAIL", "timeout"
            return r
        if not ok:
            r.status, r.reason, r.error_log = "FAIL", reason, out
            return r

    run_cmd = ([QEMU_EXEC] if QEMU_EXEC else []) + [str(binf)]
    _, out, rc = run_step(run_cmd)
    if rc is None:
        r.status, r.reason = "FAIL", "timeout"
        return r
    r.actual_exit, r.stderr_log = rc, out
    if rc != tc.expected_exit:
        r.status, r.reason = "FAIL", "result"
    return r


# --- ASSERT parsing ---

def extract_asserts(test_file: Path) -> List[Tuple[str, str]]:
    """Pull (expected_src, actual_src) out of every ASSERT() call, skipping
    comments, preprocessor lines, and the `void ASSERT(...)` prototype."""
    out = []
    for line in test_file.read_text().splitlines():
        s = line.strip()
        if not s or s.startswith(("//", "#", "void ")) or "ASSERT(" not in s:
            continue
        m = ASSERT_CALL_RE.match(s)
        if m:
            out.append((m.group(1).strip(), m.group(2).strip()))
    return out


def count_asserts(test_file: Path) -> int:
    return len(extract_asserts(test_file))


def parse_asserts(r: TestResult) -> List[AssertResult]:
    by_idx = {}
    for line in r.stderr_log.splitlines():
        m = ASSERT_RESULT_RE.search(line)
        if m:
            status, idx, expected, actual = m.groups()
            by_idx[int(idx)] = (status, expected, actual)

    out = []
    for i, (expected_src, actual_src) in enumerate(extract_asserts(r.file), 1):
        if i in by_idx:
            status, expected, actual = by_idx[i]
        else:
            status, expected, actual = "SKIP", expected_src, "—"
        out.append(AssertResult(i, actual_src, expected, actual, status))
    return out


# --- Test collection ---

def collect_tests(filters: List[str]) -> List[TestCase]:
    tests = []
    if not CASES_DIR.is_dir():
        return tests
    num = 0
    for f in sorted(CASES_DIR.glob("*.c")):
        if "ASSERT(" not in f.read_text():
            print(color(f"警告: {f.name} に ASSERT() がありません。スキップします", C.YELLOW))
            continue
        if filters and not any(flt in str(f) for flt in filters):
            continue
        num += 1
        tests.append(TestCase(num, f, f.name))
    return tests


# --- Failure description (shared by both output formats) ---

def signal_name(n: int) -> str:
    try:
        return signal.Signals(n).name
    except ValueError:
        return f"signal {n}"


def failure_info(r: TestResult) -> Tuple[str, Optional[str]]:
    """Returns (short title for the file-level line, skip_reason for SKIPed asserts)."""
    if r.reason in ("compile", "assemble", "link"):
        return f"{r.reason} error", f"{r.reason} failed"
    if r.reason == "timeout":
        return f"timeout: {TEST_TIMEOUT}s", "program timed out"
    if r.reason == "result":
        actual = r.actual_exit
        if actual > 128:
            reason = f"program crashed with {signal_name(actual - 128)}"
        else:
            reason = f"program exited with code {actual}"
        return f"exit code: expected={r.expected_exit}, actual={actual}", reason
    return "no result", "no result"


# --- Rendering ---

def header_lines() -> List[str]:
    if OUTPUT_MD:
        lines = [
            "# 🧪 YoctoCC テストスイート (並列)", "",
            f"- **アーキテクチャ**: {UNAME_M}",
            f"- **並列数**: {PARALLEL_JOBS}",
            f"- **タイムアウト**: {TEST_TIMEOUT}s",
        ]
        if FILTERS:
            lines.append(f"- **フィルタ**: {' '.join(FILTERS)}")
        return lines + [""]

    bar = color("=" * 40, C.BLUE)
    lines = [bar, color("      Yoctocc テストスイート (並列)", C.BLUE), bar,
             f"アーキテクチャ: {UNAME_M}", f"並列数: {PARALLEL_JOBS}", f"タイムアウト: {TEST_TIMEOUT}s"]
    if FILTERS:
        lines.append(f"フィルタ: {' '.join(FILTERS)}")
    return lines + [""]


STATUS_ICON = {"PASS": "✅", "FAIL": "❌", "SKIP": "⏭️"}


def md_escape(s: str) -> str:
    """Escape pipes so a `code` value with `|` in it doesn't break the table row."""
    return s.replace("|", "\\|")


def assert_table(asserts: List[AssertResult], skip_reason: Optional[str]) -> List[str]:
    """# | Code | Expected | Actual | Note"""
    if not asserts:
        return []
    lines = ["| # | Code | Expected | Actual | Note |", "|---|---|---|---|---|"]
    for a in asserts:
        note = (skip_reason or "skipped") if a.status == "SKIP" else ""
        lines.append(f"| {STATUS_ICON[a.status]} {a.index} "
                      f"| `{md_escape(a.code)}` | {md_escape(a.expected)} | {md_escape(a.actual)} | {note} |")
    return lines


def assert_lines_simple(asserts: List[AssertResult], skip_reason: Optional[str]) -> List[str]:
    lines = []
    for a in asserts:
        if a.status == "SKIP":
            detail = f"skipped: {skip_reason}" if skip_reason else "skipped"
            sep = f" => ({detail})"
        else:
            sep = f" => expected: {a.expected}, actual: {a.actual}"
        code = C.GREEN if a.status == "PASS" else C.RED if a.status == "FAIL" else C.YELLOW
        lines.append(color(f"    #{a.index} {a.code}{sep}", code))
    return lines


def render_result(r: TestResult) -> List[str]:
    lines = []
    if r.status == "PASS":
        lines.append(f"### ✅ `{r.name}`" if OUTPUT_MD else color(f"[{r.name}]", C.GREEN))
        skip_reason = None
    else:
        title, skip_reason = failure_info(r)
        if OUTPUT_MD:
            lines.append(f"### ❌ `{r.name}` — **{title}**")
            if r.error_log:
                lines += ["", "```", r.error_log.rstrip("\n"), "```"]
        else:
            lines.append(color(f"[{r.name}] FAILED ({title})", C.RED))
            if r.error_log:
                lines.append(color("  Error details:", C.YELLOW))
                lines += [f"    {ln}" for ln in r.error_log.rstrip("\n").splitlines()]

    if OUTPUT_MD:
        table = assert_table(r.asserts, skip_reason)
        if table:
            lines += [""] + table
        lines.append("")
    else:
        lines += assert_lines_simple(r.asserts, skip_reason)
    return lines


def summarize(results: List[TestResult]):
    total_cases = passed = failed = skipped = failed_files = 0
    for r in results:
        p = sum(a.status == "PASS" for a in r.asserts)
        f = sum(a.status == "FAIL" for a in r.asserts)
        s = sum(a.status == "SKIP" for a in r.asserts)
        if r.status == "FAIL" and not r.asserts:
            f = max(f, count_asserts(r.file))
        total_cases += p + f + s
        passed += p
        failed += f
        skipped += s
        if r.status == "FAIL" or f > 0:
            failed_files += 1
    return total_cases, passed, failed, skipped, failed_files


def summary_lines(results: List[TestResult], elapsed: float) -> Tuple[List[str], int]:
    total_cases, passed, failed, skipped, failed_files = summarize(results)
    total_files = len(results)

    if OUTPUT_MD:
        lines = ["---", "", "## 📊 テスト結果サマリー", "",
                 "| 項目 | 値 |", "|---|---|",
                 f"| ファイル数 | {total_files} (失敗: {failed_files}) |",
                 f"| 総ケース数 | {total_cases} |",
                 f"| ✅ 成功 | {passed} |"]
        if failed:
            lines.append(f"| ❌ 失敗 | {failed} |")
        if skipped:
            lines.append(f"| ⏭️ スキップ | {skipped} |")
        lines.append(f"| ⏱️ 経過時間 | {elapsed:.3f}s |")
        lines.append("")
        if failed_files == 0:
            lines.append(f"**✅ すべてのテストが成功しました!** ({passed}/{total_cases})")
        else:
            lines.append(f"**❌ 一部のテストが失敗しました** "
                          f"(ファイル: {failed_files}/{total_files}, ケース: ✅ {passed} / ❌ {failed} / ⏭️ {skipped})")
        return lines, failed_files

    bar = color("=" * 40, C.BLUE)
    lines = ["", bar, color("      テスト結果サマリー", C.BLUE), bar,
             f"ファイル数:   {total_files} (失敗: {failed_files})",
             f"総ケース数:   {total_cases}",
             color(f"成功:         {passed}", C.GREEN)]
    if failed:
        lines.append(color(f"失敗:         {failed}", C.RED))
    if skipped:
        lines.append(color(f"スキップ:     {skipped}", C.YELLOW))
    lines.append("")
    lines.append(f"経過時間:     {elapsed:.3f}s")
    lines.append("")
    if failed_files == 0:
        lines.append(color(f"すべてのテストが成功しました! ({passed}/{total_cases})", C.GREEN))
    else:
        lines.append(color(f"一部のテストが失敗しました "
                            f"(ファイル: {failed_files}/{total_files}, "
                            f"ケース: 成功 {passed} / 失敗 {failed} / スキップ {skipped})", C.RED))
    return lines, failed_files


# --- Main ---

def main():
    start = time.time()
    print("\n".join(header_lines()))

    print(color("コンパイラをビルド中...", C.YELLOW))
    if not make("build/yoctocc"):
        print(color("コンパイラのビルドに失敗しました", C.RED))
        sys.exit(1)
    print(color("コンパイラ本体のビルドが完了しました", C.GREEN))

    print(color("テストヘルパーをビルド中...", C.YELLOW))
    if not make("build/test_helper.o", MAKE_X86_FLAG):
        print(color("テストヘルパーのビルドに失敗しました", C.RED))
        sys.exit(1)
    print(color("テストヘルパーのビルドが完了しました", C.GREEN))
    print()

    tests = collect_tests(FILTERS)
    if not tests:
        print(color("テストケースが見つかりませんでした。", C.RED))
        sys.exit(1)

    est_cases = sum(count_asserts(t.file) for t in tests)
    print(color(f"テスト実行中... ({est_cases} ケース / {len(tests)} ファイル)", C.YELLOW))
    print()

    work_dir = Path(tempfile.mkdtemp())
    results: List[TestResult] = []
    try:
        with ThreadPoolExecutor(max_workers=PARALLEL_JOBS) as executor:
            futures = {executor.submit(run_single_test, t, work_dir): t for t in tests}
            for future in as_completed(futures):
                tc = futures[future]
                try:
                    results.append(future.result())
                except Exception as e:
                    results.append(TestResult(name=tc.name, file=tc.file, status="FAIL",
                                               reason="noresult", error_log=str(e)))
    finally:
        shutil.rmtree(work_dir, ignore_errors=True)

    results.sort(key=lambda r: r.name)
    for r in results:
        r.asserts = parse_asserts(r)

    for r in results:
        print("\n".join(render_result(r)))

    elapsed = time.time() - start
    lines, failed_files = summary_lines(results, elapsed)
    print("\n".join(lines))
    sys.exit(0 if failed_files == 0 else 1)


if __name__ == "__main__":
    main()
