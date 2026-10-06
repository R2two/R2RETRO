"""Regression checks for the GP4 directory tree sent to PkgTool."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

SPEC = importlib.util.spec_from_file_location(
    "r2n64_package", Path(__file__).resolve().parents[1] / "scripts/package.py")
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class Gp4DirectoriesTests(unittest.TestCase):
    def test_repeated_names_keep_full_parents_and_file_metadata(self):
        paths = ["eboot.bin", "licenses/common/audio/file.c.notice.txt",
                 "licenses/common/include/audio/file.h.notice.txt",
                 "licenses/common/streams/file.c.notice.txt",
                 "licenses/common/include/streams/file.h.notice.txt",
                 "licenses/other/include/audio/file.h.notice.txt"]
        root = ET.Element("psproject", fmt="gp4", version="1000")
        ET.SubElement(root, "volume", marker="unchanged")
        files = ET.SubElement(root, "files")
        for path in paths:
            ET.SubElement(files, "file", targ_path=path, orig_path=path,
                          pfs_compression="enable")
        directories = ET.SubElement(root, "rootdir")
        ET.SubElement(directories, "dir", targ_name="incorrect-old-tree")
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "test.gp4"
            ET.ElementTree(root).write(project)
            PACKAGE.rebuild_gp4_directories(project)
            actual = ET.parse(project).getroot()
            self.assertEqual(actual.find("volume").attrib, {"marker": "unchanged"})
            self.assertEqual([file.attrib for file in actual.find("files")],
                             [file.attrib for file in files])
            found = set()

            def walk(directory, parent=""):
                for child in directory:
                    target = (parent + "/" + child.attrib["targ_name"]).lstrip("/")
                    self.assertNotIn(target, found)
                    found.add(target)
                    walk(child, target)

            walk(actual.find("rootdir"))
            expected = {parent.as_posix() for path in paths
                        for parent in Path(path).parents if parent != Path(".")}
            self.assertEqual(found, expected)
            once = project.read_bytes()
            PACKAGE.rebuild_gp4_directories(project)
            self.assertEqual(project.read_bytes(), once)

    def test_parent_traversal_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "test.gp4"
            project.write_text('<psproject><files><file targ_path="../bad"/>'
                               '</files><rootdir/></psproject>')
            with self.assertRaises(RuntimeError):
                PACKAGE.rebuild_gp4_directories(project)


if __name__ == "__main__":
    unittest.main()
