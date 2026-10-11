import hashlib
import json
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INSTALLER_SCRIPT = ROOT / "installer" / "InstallTruckSimPlugin.ps1"
POWERSHELL = shutil.which("powershell.exe")
DLL64 = (
    ROOT / "includes" / "trucksim-gps-plugin" / "win_x64" / "plugins"
    / "trucksim-gps-telemetry.dll"
)
DLL32 = (
    ROOT / "includes" / "trucksim-gps-plugin" / "win_x86" / "plugins"
    / "trucksim-gps-telemetry.dll"
)


def pe_machine(path: Path) -> int:
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
class TruckSimPluginInstallerTests(unittest.TestCase):
    def invoke_installer(
        self,
        app_root: Path,
        library_root: Path | list[Path],
        remove: bool = False,
        dll64: Path = DLL64,
        dll32: Path = DLL32,
    ) -> subprocess.CompletedProcess[str]:
        libraries = library_root if isinstance(library_root, list) else [library_root]
        if len(libraries) > 1:
            quote = lambda value: "'" + str(value).replace("'", "''") + "'"
            arguments = " ".join(
                [
                    "-DllPath64", quote(dll64),
                    "-DllPath32", quote(dll32),
                    "-ManifestPath", quote(app_root / "tools" / "installed-trucksim-plugins.json"),
                    "-StatusPath", quote(app_root / "logs" / "trucksim-plugin-status.txt"),
                    "-SteamLibraryRoots @(" + ",".join(quote(path) for path in libraries) + ")",
                ]
            )
            if remove:
                arguments += " -RemoveManagedPlugin"
            return subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-Command",
                    f"& {{ & {quote(INSTALLER_SCRIPT)} {arguments} }}",
                ],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
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
            str(app_root / "tools" / "installed-trucksim-plugins.json"),
            "-StatusPath",
            str(app_root / "logs" / "trucksim-plugin-status.txt"),
            "-SteamLibraryRoots",
        ]
        command.append(str(libraries[0]))
        if remove:
            command.append("-RemoveManagedPlugin")
        return subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )

    def test_clean_install_upgrade_and_uninstall_verify_destination_hashes(self) -> None:
        self.assertTrue(DLL64.is_file(), str(DLL64))
        self.assertTrue(DLL32.is_file(), str(DLL32))
        self.assertEqual(0x8664, pe_machine(DLL64))
        self.assertEqual(0x014C, pe_machine(DLL32))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            library_root = root / "SteamLibrary"
            destinations = {
                "win_x64": (
                    library_root / "steamapps" / "common" / "Euro Truck Simulator 2"
                    / "bin" / "win_x64" / "plugins" / "trucksim-gps-telemetry.dll"
                ),
                "win_x86": (
                    library_root / "steamapps" / "common" / "Euro Truck Simulator 2"
                    / "bin" / "win_x86" / "plugins" / "trucksim-gps-telemetry.dll"
                ),
            }
            for path in destinations.values():
                path.parent.mkdir(parents=True)

            installed = self.invoke_installer(app_root, library_root)
            self.assertEqual(0, installed.returncode, installed.stderr)
            for architecture, destination in destinations.items():
                source = DLL64 if architecture == "win_x64" else DLL32
                self.assertTrue(destination.is_file())
                self.assertEqual(
                    hashlib.sha256(source.read_bytes()).hexdigest().upper(),
                    hashlib.sha256(destination.read_bytes()).hexdigest().upper(),
                )
                self.assertEqual(pe_machine(source), pe_machine(destination))

            manifest_path = app_root / "tools" / "installed-trucksim-plugins.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
            for architecture, destination in destinations.items():
                old_image = bytearray(destination.read_bytes())
                old_image[-1] ^= 0x01
                destination.write_bytes(old_image)
                manifest[str(destination)] = hashlib.sha256(old_image).hexdigest().upper()
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            upgraded = self.invoke_installer(app_root, library_root)
            self.assertEqual(0, upgraded.returncode, upgraded.stderr)
            for architecture, destination in destinations.items():
                source = DLL64 if architecture == "win_x64" else DLL32
                self.assertEqual(source.read_bytes(), destination.read_bytes())

            removed = self.invoke_installer(app_root, library_root, remove=True)
            self.assertEqual(0, removed.returncode, removed.stderr)
            self.assertTrue(all(not path.exists() for path in destinations.values()))

    def test_wrong_architecture_source_fails_before_installing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            library_root = root / "SteamLibrary"
            destination = (
                library_root / "steamapps" / "common" / "Euro Truck Simulator 2"
                / "bin" / "win_x86" / "plugins" / "trucksim-gps-telemetry.dll"
            )
            destination.parent.mkdir(parents=True)
            result = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(INSTALLER_SCRIPT),
                    "-DllPath64",
                    str(DLL64),
                    "-DllPath32",
                    str(DLL64),
                    "-ManifestPath",
                    str(root / "app" / "tools" / "manifest.json"),
                    "-StatusPath",
                    str(root / "app" / "logs" / "status.txt"),
                    "-SteamLibraryRoots",
                    str(library_root),
                ],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertFalse(destination.exists())

    def test_source_hash_mismatch_fails_and_is_logged(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            invalid_source = root / "modified-x64.dll"
            image = bytearray(DLL64.read_bytes())
            image[-1] ^= 0x01
            invalid_source.write_bytes(image)

            result = self.invoke_installer(
                root / "app", root / "missing-library", dll64=invalid_source
            )

            self.assertNotEqual(0, result.returncode)
            status = (root / "app" / "logs" / "trucksim-plugin-status.txt").read_text(
                encoding="utf-8-sig"
            )
            self.assertIn("hash mismatch", status)
            self.assertFalse((root / "missing-library" / "steamapps").exists())

    def test_multiple_libraries_preserve_unmanaged_plugins_and_user_data(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            libraries = [root / "SteamLibraryA", root / "SteamLibraryB"]
            destinations = [
                library / "steamapps" / "common" / "Euro Truck Simulator 2"
                / "bin" / "win_x64" / "plugins" / "trucksim-gps-telemetry.dll"
                for library in libraries
            ]
            for destination in destinations:
                destination.parent.mkdir(parents=True)
            unmanaged = b"unrelated third-party plugin"
            destinations[1].write_bytes(unmanaged)
            user_data = app_root / "LocalAppData" / "NLSI" / "Exclusive Logbook" / "history.jsonl"
            user_data.parent.mkdir(parents=True)
            user_data.write_bytes(b"preserve this user history")

            result = self.invoke_installer(app_root, libraries)

            self.assertEqual(0, result.returncode, result.stderr)
            self.assertEqual(DLL64.read_bytes(), destinations[0].read_bytes())
            self.assertEqual(unmanaged, destinations[1].read_bytes())
            self.assertEqual(b"preserve this user history", user_data.read_bytes())

    def test_ets2_and_ats_each_get_matching_architecture_payloads(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            library_root = root / "SteamLibrary"
            destinations = []
            for game in ("Euro Truck Simulator 2", "American Truck Simulator"):
                for architecture in ("win_x64", "win_x86"):
                    destination = (
                        library_root / "steamapps" / "common" / game / "bin"
                        / architecture / "plugins" / "trucksim-gps-telemetry.dll"
                    )
                    destination.parent.mkdir(parents=True)
                    destinations.append((
                        destination,
                        DLL64 if architecture == "win_x64" else DLL32,
                    ))

            result = self.invoke_installer(app_root, library_root)

            self.assertEqual(0, result.returncode, result.stderr)
            for destination, source in destinations:
                self.assertEqual(source.read_bytes(), destination.read_bytes())
                self.assertEqual(
                    hashlib.sha256(source.read_bytes()).hexdigest().upper(),
                    hashlib.sha256(destination.read_bytes()).hexdigest().upper(),
                )
                self.assertEqual(pe_machine(source), pe_machine(destination))

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

            result = self.invoke_installer(app_root, library_root)

            destination = (
                game_arch / "plugins" / "trucksim-gps-telemetry.dll"
            )
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertEqual(DLL64.read_bytes(), destination.read_bytes())
            self.assertEqual(0x8664, pe_machine(destination))

    def test_locked_managed_destination_fails_and_is_logged(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app_root = root / "app"
            library_root = root / "SteamLibrary"
            destination = (
                library_root / "steamapps" / "common" / "Euro Truck Simulator 2"
                / "bin" / "win_x64" / "plugins" / "trucksim-gps-telemetry.dll"
            )
            destination.parent.mkdir(parents=True)
            self.assertEqual(0, self.invoke_installer(app_root, library_root).returncode)
            old_image = bytearray(destination.read_bytes())
            old_image[-1] ^= 0x01
            destination.write_bytes(old_image)
            manifest_path = app_root / "tools" / "installed-trucksim-plugins.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
            manifest[str(destination)] = hashlib.sha256(old_image).hexdigest().upper()
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            lock = lock_for_read(destination, root / "lock.ps1")
            try:
                result = self.invoke_installer(app_root, library_root)
            finally:
                lock.terminate()
                lock.communicate(timeout=5)

            self.assertNotEqual(0, result.returncode)
            self.assertEqual(bytes(old_image), destination.read_bytes())
            status = (app_root / "logs" / "trucksim-plugin-status.txt").read_text(
                encoding="utf-8-sig"
            )
            self.assertIn("failed to install", status)


if __name__ == "__main__":
    unittest.main()
