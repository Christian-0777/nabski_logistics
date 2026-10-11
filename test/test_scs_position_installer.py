import shutil
import hashlib
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INSTALLER_SCRIPT = ROOT / "installer" / "InstallScsPositionPlugin.ps1"
POWERSHELL = shutil.which("powershell.exe")


def write_pe_plugin(path: Path, machine: int, marker: bytes) -> None:
    image = bytearray(0x90 + len(marker))
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3C, 0x80)
    image[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", image, 0x84, machine)
    image[0x90:] = marker
    path.write_bytes(image)


def read_pe_machine(path: Path) -> int:
    image = path.read_bytes()
    offset = struct.unpack_from("<I", image, 0x3C)[0]
    if image[offset : offset + 4] != b"PE\0\0":
        raise ValueError(f"Invalid PE file: {path}")
    return struct.unpack_from("<H", image, offset + 4)[0]


def lock_for_read(path: Path, script_path: Path) -> subprocess.Popen[str]:
    escaped_path = str(path).replace("'", "''")
    script_path.write_text(
        f"$stream = [System.IO.File]::Open('{escaped_path}', "
        "[System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, "
        "[System.IO.FileShare]::Read)\n"
        "Write-Output 'READY'\n"
        "Start-Sleep -Seconds 60\n"
        "$stream.Dispose()\n",
        encoding="utf-8",
    )
    process = subprocess.Popen(
        [
            POWERSHELL,
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(script_path),
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    assert process.stdout is not None
    if process.stdout.readline().strip() != "READY":
        output, error = process.communicate(timeout=5)
        raise AssertionError(f"Could not lock plugin: {output}\n{error}")
    return process


@unittest.skipUnless(POWERSHELL, "Windows PowerShell is required for installer integration tests")
class ScsPositionInstallerTests(unittest.TestCase):
    def invoke_installer(
        self,
        app_root: Path,
        library_root: Path,
        dll64: Path,
        dll32: Path,
        restore: bool = False,
    ) -> subprocess.CompletedProcess[str]:
        command = [
            POWERSHELL,
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(INSTALLER_SCRIPT),
            "-DllPath64",
            str(dll64),
            "-DllPath32",
            str(dll32),
            "-ManifestPath",
            str(app_root / "tools" / "installed-scs-position-plugins.json"),
            "-BackupDirectory",
            str(app_root / "plugin-backups" / "scs-position"),
            "-StatusPath",
            str(app_root / "logs" / "scs-position-plugin-status.txt"),
            "-SteamLibraryRoots",
            str(library_root),
        ]
        if restore:
            command.append("-RestoreManagedPlugin")
        return subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )

    def test_missing_game_is_reported_without_creating_plugin_folders(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            library_root = root / "SteamLibrary"
            dll64 = root / "nlsi-x64.dll"
            dll32 = root / "nlsi-x86.dll"
            library_root.mkdir()
            write_pe_plugin(dll64, 0x8664, b"x64 plugin")
            write_pe_plugin(dll32, 0x014C, b"x86 plugin")

            result = self.invoke_installer(app_root, library_root, dll64, dll32)

            self.assertEqual(0, result.returncode, result.stderr)
            self.assertIn(
                "no supported Steam ETS2/ATS game architecture",
                (app_root / "logs" / "scs-position-plugin-status.txt").read_text(
                    encoding="utf-8-sig"
                ),
            )
            self.assertFalse((library_root / "steamapps" / "common").exists())

    def test_missing_plugin_source_fails_explicitly(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            library_root = root / "SteamLibrary"
            library_root.mkdir()
            app_root = root / "app"
            missing = root / "missing.dll"

            result = self.invoke_installer(app_root, library_root, missing, missing)

            self.assertNotEqual(0, result.returncode)
            status = (app_root / "logs" / "scs-position-plugin-status.txt").read_text(
                encoding="utf-8-sig"
            )
            self.assertIn("source DLL is missing", status)
            self.assertFalse((app_root / "tools" / "installed-scs-position-plugins.json").exists())

    def test_release_binaries_install_to_matching_game_architecture_paths(self) -> None:
        release_root = ROOT / "build" / "plugins" / "v1.5.4-beta"
        dll64 = release_root / "win_x64" / "nlsi.dll"
        dll32 = release_root / "win_x86" / "nlsi.dll"
        self.assertTrue(dll64.is_file(), str(dll64))
        self.assertTrue(dll32.is_file(), str(dll32))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            library_root = root / "SteamLibrary"
            destinations = {
                "win_x64": (
                    library_root / "steamapps" / "common" / "Euro Truck Simulator 2"
                    / "bin" / "win_x64" / "plugins" / "nlsi.dll"
                ),
                "win_x86": (
                    library_root / "steamapps" / "common" / "Euro Truck Simulator 2"
                    / "bin" / "win_x86" / "plugins" / "nlsi.dll"
                ),
            }
            for path in destinations.values():
                path.parent.mkdir(parents=True)

            result = self.invoke_installer(app_root, library_root, dll64, dll32)

            self.assertEqual(0, result.returncode, result.stderr)
            for architecture, destination in destinations.items():
                source = dll64 if architecture == "win_x64" else dll32
                self.assertEqual(
                    hashlib.sha256(source.read_bytes()).hexdigest().upper(),
                    hashlib.sha256(destination.read_bytes()).hexdigest().upper(),
                )
                self.assertEqual(
                    0x8664 if architecture == "win_x64" else 0x014C,
                    read_pe_machine(destination),
                )

    def test_existing_nlsi_plugin_is_backed_up_then_restored(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            library_root = root / "SteamLibrary"
            game_root = library_root / "steamapps" / "common" / "Euro Truck Simulator 2"
            plugin_x64 = game_root / "bin" / "win_x64" / "plugins" / "nlsi.dll"
            plugin_x86 = game_root / "bin" / "win_x86" / "plugins" / "nlsi.dll"
            plugin_x64.parent.mkdir(parents=True)
            plugin_x86.parent.mkdir(parents=True)
            plugin_x64.write_bytes(b"preexisting x64 nlsi")
            plugin_x86.write_bytes(b"preexisting x86 nlsi")
            dll64 = root / "new-x64.dll"
            dll32 = root / "new-x86.dll"
            write_pe_plugin(dll64, 0x8664, b"initial x64 nlsi")
            write_pe_plugin(dll32, 0x014C, b"initial x86 nlsi")

            installed = self.invoke_installer(app_root, library_root, dll64, dll32)

            self.assertEqual(0, installed.returncode, installed.stderr)
            self.assertEqual(dll64.read_bytes(), plugin_x64.read_bytes())
            self.assertEqual(dll32.read_bytes(), plugin_x86.read_bytes())
            manifest = (app_root / "tools" / "installed-scs-position-plugins.json").read_text(
                encoding="utf-8-sig"
            )
            self.assertIn("backup_path", manifest)
            self.assertIn(hashlib.sha256(plugin_x64.read_bytes()).hexdigest().upper(), manifest)
            self.assertIn(hashlib.sha256(plugin_x86.read_bytes()).hexdigest().upper(), manifest)

            write_pe_plugin(dll64, 0x8664, b"upgraded x64 nlsi")
            write_pe_plugin(dll32, 0x014C, b"upgraded x86 nlsi")
            upgraded = self.invoke_installer(app_root, library_root, dll64, dll32)
            self.assertEqual(0, upgraded.returncode, upgraded.stderr)
            self.assertEqual(dll64.read_bytes(), plugin_x64.read_bytes())
            self.assertEqual(dll32.read_bytes(), plugin_x86.read_bytes())
            manifest = (app_root / "tools" / "installed-scs-position-plugins.json").read_text(
                encoding="utf-8-sig"
            )
            self.assertIn(hashlib.sha256(plugin_x64.read_bytes()).hexdigest().upper(), manifest)
            self.assertIn(hashlib.sha256(plugin_x86.read_bytes()).hexdigest().upper(), manifest)

            restored = self.invoke_installer(
                app_root, library_root, dll64, dll32, restore=True
            )

            self.assertEqual(0, restored.returncode, restored.stderr)
            self.assertEqual(b"preexisting x64 nlsi", plugin_x64.read_bytes())
            self.assertEqual(b"preexisting x86 nlsi", plugin_x86.read_bytes())
            self.assertFalse(
                (app_root / "tools" / "installed-scs-position-plugins.json").exists()
            )

    def test_locked_managed_destination_fails_and_is_logged(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            library_root = root / "SteamLibrary"
            destination = (
                library_root / "steamapps" / "common" / "Euro Truck Simulator 2"
                / "bin" / "win_x64" / "plugins" / "nlsi.dll"
            )
            destination.parent.mkdir(parents=True)
            dll64 = root / "new-x64.dll"
            dll32 = root / "new-x86.dll"
            write_pe_plugin(dll64, 0x8664, b"initial x64 nlsi")
            write_pe_plugin(dll32, 0x014C, b"initial x86 nlsi")
            self.assertEqual(
                0, self.invoke_installer(app_root, library_root, dll64, dll32).returncode
            )
            original = destination.read_bytes()
            write_pe_plugin(dll64, 0x8664, b"upgraded x64 nlsi")
            lock = lock_for_read(destination, root / "lock.ps1")
            try:
                result = self.invoke_installer(app_root, library_root, dll64, dll32)
            finally:
                lock.terminate()
                lock.communicate(timeout=5)

            self.assertNotEqual(0, result.returncode)
            self.assertEqual(original, destination.read_bytes())
            status = (app_root / "logs" / "scs-position-plugin-status.txt").read_text(
                encoding="utf-8-sig"
            )
            self.assertIn("Close ETS2/ATS and retry", status)

    def test_ats_and_both_games_receive_matching_architecture_payloads(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            library_root = root / "SteamLibrary"
            dll64 = root / "payload-x64.dll"
            dll32 = root / "payload-x86.dll"
            write_pe_plugin(dll64, 0x8664, b"1.5.4-beta x64")
            write_pe_plugin(dll32, 0x014C, b"1.5.4-beta x86")
            games = ("Euro Truck Simulator 2", "American Truck Simulator")
            destinations = []
            for game in games:
                for architecture in ("win_x64", "win_x86"):
                    game_root = (
                        library_root / "steamapps" / "common" / game
                        / "bin" / architecture
                    )
                    game_root.mkdir(parents=True)
                    destinations.append((
                        game_root / "plugins" / "nlsi.dll",
                        dll64 if architecture == "win_x64" else dll32,
                        0x8664 if architecture == "win_x64" else 0x014C,
                    ))

            result = self.invoke_installer(app_root, library_root, dll64, dll32)

            self.assertEqual(0, result.returncode, result.stderr)
            for destination, source, machine in destinations:
                self.assertEqual(source.read_bytes(), destination.read_bytes())
                self.assertEqual(
                    hashlib.sha256(source.read_bytes()).hexdigest(),
                    hashlib.sha256(destination.read_bytes()).hexdigest(),
                )
                self.assertEqual(machine, read_pe_machine(destination))

    def test_ats_only_install_uses_x64_destination_and_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            library_root = root / "SteamLibrary"
            game_arch = (
                library_root / "steamapps" / "common" / "American Truck Simulator"
                / "bin" / "win_x64"
            )
            game_arch.mkdir(parents=True)
            dll64 = root / "payload-x64.dll"
            dll32 = root / "payload-x86.dll"
            write_pe_plugin(dll64, 0x8664, b"ATS x64 plugin")
            write_pe_plugin(dll32, 0x014C, b"ATS x86 plugin")

            result = self.invoke_installer(app_root, library_root, dll64, dll32)

            destination = game_arch / "plugins" / "nlsi.dll"
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertEqual(dll64.read_bytes(), destination.read_bytes())
            self.assertEqual(0x8664, read_pe_machine(destination))


if __name__ == "__main__":
    unittest.main()
