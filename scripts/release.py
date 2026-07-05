import sys
import os
import json
import zipfile
import argparse
from pathlib import Path

os.chdir(Path(__file__).resolve().parent.parent)

def get_project_version() -> str:
    with Path("main/CMakeLists.txt").open(encoding='utf-8') as f:
        for line in f:
            if line.startswith("set(PROJECT_VER"):
                return line.split("\"")[1]
    return "1.0.0"

def clean_build_dir() -> None:
    build_dir = Path("build")
    if build_dir.exists():
        import shutil
        try:
            shutil.rmtree(build_dir)
            print(f"Cleaned build directory: {build_dir}")
        except Exception as e:
            print(f"Warning: Failed to clean build directory: {e}", file=sys.stderr)

def merge_bin() -> None:
    if os.system("idf.py merge-bin") != 0:
        print("merge-bin failed", file=sys.stderr)
        sys.exit(1)

def zip_bin(name: str, version: str) -> None:
    out_dir = Path("releases")
    out_dir.mkdir(exist_ok=True)
    output_path = out_dir / f"v{version}_{name}.zip"

    if output_path.exists():
        output_path.unlink()

    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zipf:
        zipf.write("build/merged-binary.bin", arcname="merged-binary.bin")
    print(f"zip bin to {output_path} done")

def release(board: str = None, config_filename: str = "config.json") -> None:
    boards_dir = Path("main/boards")
    
    if board:
        cfg_path = boards_dir / board / config_filename
    else:
        cfg_path = None
        for board_dir in boards_dir.iterdir():
            if board_dir.is_dir() and board_dir.name != "common":
                config_path = board_dir / config_filename
                if config_path.exists():
                    cfg_path = config_path
                    board = board_dir.name
                    break
        
        if not cfg_path:
            print(f"[ERROR] No board directory with {config_filename} found in {boards_dir}", file=sys.stderr)
            sys.exit(1)
    
    if not cfg_path.exists():
        print(f"[ERROR] {cfg_path} does not exist", file=sys.stderr)
        sys.exit(1)
    
    print(f"Board: {board}")

    project_version = get_project_version()
    print(f"Project Version: {project_version}")

    with cfg_path.open(encoding='utf-8') as f:
        cfg = json.load(f)
    
    target = cfg["target"]
    manufacturer = cfg.get("manufacturer", "")

    builds = cfg.get("builds", [])
    if not builds:
        print("[ERROR] No builds defined in config.json", file=sys.stderr)
        sys.exit(1)

    for build in builds:
        name = build["name"]
        final_name = f"{manufacturer}-{name}" if manufacturer else name
        
        print("-" * 80)
        print(f"name: {final_name}")
        print(f"target: {target}")
        
        sdkconfig_append = build.get("sdkconfig_append", [])
        for item in sdkconfig_append:
            print(f"sdkconfig_append: {item}")

        clean_build_dir()

        if os.system(f"idf.py set-target {target}") != 0:
            print("set-target failed", file=sys.stderr)
            sys.exit(1)

        with Path("sdkconfig").open("a", encoding='utf-8') as f:
            f.write("\n")
            f.write("# Append by release.py\n")
            for append in sdkconfig_append:
                f.write(f"{append}\n")

        if os.system(f"idf.py build") != 0:
            print("build failed")
            sys.exit(1)

        merge_bin()
        zip_bin(final_name, project_version)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Tianshu Firmware Release Build Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  python scripts/release.py                           # Auto-detect board
  python scripts/release.py -b tianshu-mesh           # Specify board
  python scripts/release.py -l                        # List available boards
"""
    )
    parser.add_argument(
        "-b", "--board",
        metavar="BOARD_NAME",
        help="Board directory name under main/boards/ (e.g., tianshu-mesh, tianshu-mesh-display)"
    )
    parser.add_argument(
        "-c", "--config",
        default="config.json",
        metavar="FILENAME",
        help="Board config filename (default: config.json)"
    )
    parser.add_argument(
        "-l", "--list",
        action="store_true",
        help="List available board directories"
    )
    
    args = parser.parse_args()
    
    if args.list:
        boards_dir = Path("main/boards")
        print("Available boards:")
        for board_dir in sorted(boards_dir.iterdir()):
            if board_dir.is_dir() and board_dir.name != "common":
                config_path = board_dir / args.config
                status = "[OK]" if config_path.exists() else "[no config]"
                print(f"  - {board_dir.name} {status}")
        sys.exit(0)
    
    release(board=args.board, config_filename=args.config)
