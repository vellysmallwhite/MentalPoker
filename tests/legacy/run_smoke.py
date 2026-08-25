#!/usr/bin/env python3
"""Run the four-node legacy v1 flow and emit a deterministic pass/fail result."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time
from typing import Callable, Sequence


REPO_ROOT = Path(__file__).resolve().parents[2]
COMPOSE_FILE = REPO_ROOT / "docker-compose.yml"
PLAYERS = ("player1", "player2", "player3", "player4")
PLAYER_MARKERS = (
    "Successfully joined room",
    "All players are ready. Changing game phase to ENCRYPTION.",
    "Consensus on the deck achieved. Proceeding to decryption phase.",
    "My Hand:",
    "Final consensus achieved. The winner consensus value is:",
)
FATAL_MARKERS = (
    "AddressSanitizer",
    "UndefinedBehaviorSanitizer",
    "ERROR: LeakSanitizer",
    "runtime error:",
    "Failed to open key file",
    "Failed to open playerCount file",
    "terminate called",
)
LONG_INTEGER = re.compile(r"(?<![A-Za-z0-9])\d{20,}(?![A-Za-z0-9])")


class SmokeFailure(RuntimeError):
    """Raised when the baseline cannot produce a trustworthy pass signal."""


def run_command(
    command: Sequence[str],
    *,
    environment: dict[str, str],
    check: bool = True,
    timeout: float | None = None,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command,
        cwd=REPO_ROOT,
        env=environment,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )
    if check and result.returncode != 0:
        rendered = " ".join(command)
        raise SmokeFailure(
            f"command failed with exit code {result.returncode}: {rendered}\n"
            f"{result.stdout}"
        )
    return result


def run_streaming_command(
    command: Sequence[str],
    *,
    environment: dict[str, str],
    timeout: float,
) -> None:
    result = subprocess.run(
        command,
        cwd=REPO_ROOT,
        env=environment,
        check=False,
        timeout=timeout,
    )
    if result.returncode != 0:
        rendered = " ".join(command)
        raise SmokeFailure(
            f"command failed with exit code {result.returncode}: {rendered}"
        )


def compose_command(project: str, *arguments: str) -> list[str]:
    return [
        "docker",
        "compose",
        "--file",
        str(COMPOSE_FILE),
        "--project-name",
        project,
        *arguments,
    ]


def find_available_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


def service_logs(
    project: str, service: str, environment: dict[str, str]
) -> str:
    result = run_command(
        compose_command(project, "logs", "--no-color", service),
        environment=environment,
        check=False,
        timeout=15,
    )
    return result.stdout


def service_state(
    project: str, service: str, environment: dict[str, str]
) -> tuple[str, str]:
    container = run_command(
        compose_command(project, "ps", "--all", "--quiet", service),
        environment=environment,
        check=False,
        timeout=15,
    ).stdout.strip()
    if not container:
        return "missing", "none"

    result = run_command(
        [
            "docker",
            "inspect",
            "--format",
            "{{.State.Status}} {{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}",
            container,
        ],
        environment=environment,
        check=False,
        timeout=15,
    )
    fields = result.stdout.strip().split(maxsplit=1)
    if result.returncode != 0 or not fields:
        return "missing", "none"
    return fields[0], fields[1] if len(fields) == 2 else "none"


def evaluate_player_log(log: str) -> tuple[list[str], list[str]]:
    missing = [marker for marker in PLAYER_MARKERS if marker not in log]
    fatal = [marker for marker in FATAL_MARKERS if marker in log]
    return missing, fatal


def redact_log(log: str) -> str:
    redacted: list[str] = []
    redact_next_line = False
    for line in log.splitlines():
        if redact_next_line:
            prefix = line.split("|", maxsplit=1)[0]
            redacted.append(f"{prefix}| <redacted legacy private cards>")
            redact_next_line = False
            continue
        if "My Hand:" in line:
            redacted.append(line)
            redact_next_line = True
            continue
        if " hand value:" in line:
            prefix = line.split("hand value:", maxsplit=1)[0]
            redacted.append(f"{prefix}hand value: <redacted>")
            continue
        redacted.append(LONG_INTEGER.sub("<redacted-large-integer>", line))
    return "\n".join(redacted) + ("\n" if log else "")


def wait_until(
    description: str,
    predicate: Callable[[], bool],
    *,
    deadline: float,
) -> None:
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.5)
    raise SmokeFailure(f"timed out waiting for {description}")


def write_run_artifacts(
    artifact_dir: Path,
    project: str,
    environment: dict[str, str],
) -> dict[str, dict[str, str]]:
    states: dict[str, dict[str, str]] = {}
    for service in ("server", *PLAYERS):
        status, health = service_state(project, service, environment)
        states[service] = {"status": status, "health": health}
        log = service_logs(project, service, environment)
        (artifact_dir / f"{service}.log").write_text(
            redact_log(log), encoding="utf-8"
        )
    return states


def run_once(
    run_number: int,
    *,
    preset: str,
    timeout_seconds: int,
    artifact_root: Path,
    base_environment: dict[str, str],
) -> dict[str, object]:
    project = f"mentalpoker-legacy-{os.getpid()}-{run_number}"
    run_artifacts = artifact_root / f"run-{run_number}"
    run_artifacts.mkdir(parents=True, exist_ok=True)
    environment = dict(base_environment)
    environment["LEGACY_CMAKE_PRESET"] = preset
    environment["LEGACY_IMAGE_TAG"] = preset
    environment["LEGACY_SERVER_PORT"] = str(find_available_port())
    started_at = time.monotonic()
    deadline = started_at + timeout_seconds
    result: dict[str, object] = {
        "run": run_number,
        "preset": preset,
        "status": "failed",
    }

    try:
        run_command(
            compose_command(project, "up", "--detach", "server"),
            environment=environment,
            timeout=60,
        )

        def server_is_healthy() -> bool:
            status, health = service_state(project, "server", environment)
            if status in {"dead", "exited", "removing"}:
                raise SmokeFailure(f"server stopped before becoming healthy: {status}")
            return status == "running" and health == "healthy"

        wait_until("the lobby healthcheck", server_is_healthy, deadline=deadline)

        for player in PLAYERS:
            run_command(
                compose_command(project, "up", "--detach", "--no-deps", player),
                environment=environment,
                timeout=60,
            )

            def player_joined(service: str = player) -> bool:
                status, _ = service_state(project, service, environment)
                if status in {"dead", "exited", "removing"}:
                    raise SmokeFailure(f"{service} stopped while joining: {status}")
                return "Successfully joined room" in service_logs(
                    project, service, environment
                )

            wait_until(f"{player} to join", player_joined, deadline=deadline)

        def hand_completed() -> bool:
            all_complete = True
            for player in PLAYERS:
                status, _ = service_state(project, player, environment)
                if status in {"dead", "exited", "removing"}:
                    raise SmokeFailure(f"{player} stopped during the hand: {status}")
                missing, fatal = evaluate_player_log(
                    service_logs(project, player, environment)
                )
                if fatal:
                    raise SmokeFailure(
                        f"{player} emitted fatal diagnostics: {', '.join(fatal)}"
                    )
                all_complete = all_complete and not missing
            return all_complete

        wait_until(
            "all four players to complete the legacy hand",
            hand_completed,
            deadline=deadline,
        )
        result["status"] = "passed"
    except (SmokeFailure, subprocess.TimeoutExpired) as error:
        result["error"] = str(error)
    finally:
        result["duration_seconds"] = round(time.monotonic() - started_at, 3)
        try:
            result["services"] = write_run_artifacts(
                run_artifacts, project, environment
            )
        finally:
            run_command(
                compose_command(
                    project,
                    "down",
                    "--volumes",
                    "--remove-orphans",
                    "--timeout",
                    "2",
                ),
                environment=environment,
                check=False,
                timeout=30,
            )

    return result


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument(
        "--preset", choices=("ci-release", "asan-ubsan"), default="ci-release"
    )
    parser.add_argument(
        "--artifact-dir",
        type=Path,
        default=REPO_ROOT / "artifacts" / "legacy-smoke",
    )
    parser.add_argument("--skip-build", action="store_true")
    arguments = parser.parse_args()
    if arguments.repeat < 1:
        parser.error("--repeat must be at least 1")
    if arguments.timeout < 30:
        parser.error("--timeout must be at least 30 seconds")
    return arguments


def main() -> int:
    arguments = parse_arguments()
    artifacts = arguments.artifact_dir.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["LEGACY_CMAKE_PRESET"] = arguments.preset
    environment["LEGACY_IMAGE_TAG"] = arguments.preset

    summary: dict[str, object] = {
        "schema_version": 1,
        "preset": arguments.preset,
        "requested_runs": arguments.repeat,
        "runs": [],
    }
    exit_code = 1

    try:
        run_command(
            ["docker", "info", "--format", "{{.ServerVersion}}"],
            environment=environment,
            timeout=30,
        )
        run_command(
            ["docker", "compose", "version"],
            environment=environment,
            timeout=30,
        )

        if not arguments.skip_build:
            build_project = f"mentalpoker-legacy-build-{os.getpid()}"
            run_streaming_command(
                compose_command(build_project, "build", "server", "player1"),
                environment=environment,
                timeout=1800,
            )

        runs: list[dict[str, object]] = []
        for run_number in range(1, arguments.repeat + 1):
            result = run_once(
                run_number,
                preset=arguments.preset,
                timeout_seconds=arguments.timeout,
                artifact_root=artifacts,
                base_environment=environment,
            )
            runs.append(result)
            status = result["status"]
            duration = result["duration_seconds"]
            print(
                f"legacy smoke run {run_number}: {status} ({duration}s)",
                flush=True,
            )
            if status != "passed":
                print(result.get("error", "unknown error"), file=sys.stderr)
                break
        summary["runs"] = runs
        exit_code = 0 if len(runs) == arguments.repeat and all(
            run["status"] == "passed" for run in runs
        ) else 1
    except (FileNotFoundError, SmokeFailure, subprocess.TimeoutExpired) as error:
        summary["infrastructure_error"] = str(error)
        print(f"legacy smoke infrastructure failure: {error}", file=sys.stderr)
    except KeyboardInterrupt:
        summary["infrastructure_error"] = "interrupted by user"
        print("legacy smoke interrupted; container cleanup was attempted", file=sys.stderr)
        exit_code = 130
    finally:
        summary["status"] = "passed" if exit_code == 0 else "failed"
        (artifacts / "summary.json").write_text(
            json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
