#!/usr/bin/env python3
"""Static safety guards for the isolated QDTech Mega Drive prototype."""

from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
PREPARE = ROOT / "tools/prepare_md_test_pack.ps1"
PLAN = ROOT / "docs/V1819_MD_PROTOTYPE_PLAN.md"


def main() -> None:
    script = PREPARE.read_text(encoding="utf-8-sig")
    plan = PLAN.read_text(encoding="utf-8")

    ids = re.findall(r"@\{ Id='(\d{2})';", script)
    assert ids == [f"{number:02d}" for number in range(1, 21)]
    assert "..\\tmp\\md-test-pack" in script
    assert "32X" not in "\n".join(
        line for line in script.splitlines() if line.lstrip().startswith("@{")
    )

    # The collection contains many misleading .smd suffixes. Keep one real
    # interleaved sample and normalize known linear images to .bin.
    assert script.count("Out='shining_force_1_zh.smd'") == 1
    assert "$header[1] -eq 0x03" in script
    assert "$header[8] -eq 0xAA" in script
    assert "$header[9] -eq 0xBB" in script
    assert "GetString($header, 0x100, 4) -eq 'SEGA'" in script
    assert "Out='road_rash_2.bin'" in script
    assert "Out='super_street_fighter_2.bin'" in script

    assert script.count("Tier='memory-3m'") == 2
    assert script.count("Tier='memory-5m'") == 1
    assert script.count("Controls='6-button'") == 1
    assert "不得提交或上传 GitHub" in plan
    assert "不得把 Gwenesis 源码或组合固件推送" in plan
    assert "不修改正式固件" in plan

    tracked = subprocess.run(
        ["git", "ls-files"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    ).stdout.splitlines()
    tracked_test_pack = [
        path for path in tracked
        if "md-test-pack" in path.lower() or path.lower().startswith("roms/md/")
    ]
    assert not tracked_test_pack, (
        f"MD test-pack files must not be tracked: {tracked_test_pack}"
    )

    print("QDTech MD prototype safety guards passed")


if __name__ == "__main__":
    main()
