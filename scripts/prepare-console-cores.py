#!/usr/bin/env python3
"""Reproducible integration changes to isolated sources; upstream stays intact."""
from pathlib import Path
import subprocess
import sys

root, build = map(Path, sys.argv[1:])
assert build.resolve().parent == (root / 'build').resolve()

def upstream(core, name):
    return subprocess.check_output(['git', '-C', str(root / 'external' / core), 'show', 'HEAD:' + name], text=True)

def replace_once(source, old, new):
    assert source.count(old) == 1, old[:100]
    return source.replace(old, new)

def save(core, name, text):
    path = build / (core + '-source') / name
    if path.read_text() != text:
        path.write_text(text)

source = upstream('fceumm', 'Makefile.common')
# A standalone static frontend does not provide RetroArch's helper functions.
source = replace_once(source, 'ifneq ($(STATIC_LINKING), 1)', 'ifeq (1, 1) # R2N64: link namespaced libretro-common helpers')
save('fceumm', 'Makefile.common', source)
source = upstream('fceumm', 'src/drivers/libretro/libretro.c')
source = replace_once(source, 'info->need_fullpath    = true;', 'info->need_fullpath    = false; /* R2N64 owns content memory. */')
source = replace_once(source, '''      if (!info || string_is_empty(info->path))
         return false;

      strlcpy(content_path, info->path,
            sizeof(content_path));''', '''      /* R2N64: no ROM-adjacent files; consume the checked frontend buffer. */
      if (!info || !info->data || !info->size)
         return false;
      content_data = (const uint8_t *)info->data;
      content_size = info->size;
      strlcpy(content_path, "R2N64-memory.nes", sizeof(content_path));''')
save('fceumm', 'src/drivers/libretro/libretro.c', source)
source = upstream('fceumm', 'src/palette.c')
source = replace_once(source, '''		int ssize = filestream_get_size(fp);
		int nEntries = ssize / 3;
		filestream_read(fp, ptmp, ssize);
		filestream_close(fp);''', '''		/* R2N64: the automatic system/nes.pal loader must not overflow
		 * its fixed buffer or use unread bytes from a truncated file. */
		int64_t ssize = filestream_get_size(fp);
		int nEntries;
		if ((ssize != 64 * 3 && ssize != sizeof(ptmp)) ||
			filestream_read(fp, ptmp, ssize) != ssize) {
			filestream_close(fp);
			free(fn);
			FCEU_printf(" Ignoring nes.pal: expected a complete 64- or 512-color palette.\\n");
			return;
		}
		nEntries = (int)(ssize / 3);
		filestream_close(fp);''')
save('fceumm', 'src/palette.c', source)
source = upstream('bsnes-mercury', 'target-libretro/libretro.cpp')
source = replace_once(source, '''bool retro_load_game(const struct retro_game_info *info) {
  // Support loading a manifest directly.''', '''bool retro_load_game(const struct retro_game_info *info) {
  // R2N64: a failed memory-only load must not poison the next cartridge.
  core_bind.load_request_error = false;
  core_bind.basename = "";
  core_bind.rom_filename = "";
  // Support loading a manifest directly.''')
source = replace_once(source, 'if(manifest || file::exists(load_path)) {',
                      'if(manifest) { // R2N64: memory-only content uses configured system directory.')
source = replace_once(source, '''size_t retro_serialize_size(void) {
  return SuperFamicom::system.serialize_size();
}''', '''// R2N64: upstream omits DSP1-4 HLE state in system/serialization.cpp.
// Do not report successful states which silently lose active chip state.
static bool r2n64_complete_state_supported() {
  return !SuperFamicom::cartridge.has_dsp1()
      && !SuperFamicom::cartridge.has_dsp2()
      && !SuperFamicom::cartridge.has_dsp3()
      && !SuperFamicom::cartridge.has_dsp4();
}

size_t retro_serialize_size(void) {
  return r2n64_complete_state_supported() ? SuperFamicom::system.serialize_size() : 0;
}''')
source = replace_once(source, '''bool retro_serialize(void *data, size_t size) {
  SuperFamicom::system.runtosave();''', '''bool retro_serialize(void *data, size_t size) {
  if (!r2n64_complete_state_supported()) return false;
  SuperFamicom::system.runtosave();''')
source = replace_once(source, '''bool retro_unserialize(const void *data, size_t size) {
  serializer s((const uint8_t*)data, size);''', '''bool retro_unserialize(const void *data, size_t size) {
  if (!r2n64_complete_state_supported()) return false;
  serializer s((const uint8_t*)data, size);''')
save('bsnes-mercury', 'target-libretro/libretro.cpp', source)
source = upstream('bsnes-mercury', 'sfc/cartridge/markup.cpp')
source = replace_once(source, '''void Cartridge::parse_markup_necdsp(Markup::Node root) {
  if(root.exists() == false) return;
  if(interface->bind->altImplementation(Alt::ForDSP)==Alt::DSP::HLE)''', '''void Cartridge::parse_markup_necdsp(Markup::Node root) {
  if(root.exists() == false) return;
  // R2N64: ST0011 shares the uPD96050 model, but has no ST0010 HLE.
  // Unknown programs use existing LLE and its required-firmware failure path.
  string hleProgram = root["rom[0]/name"].data;
  bool hleSupported = root["model"].data == "uPD7725"
      ? (hleProgram == "dsp1.program.rom" || hleProgram == "dsp1b.program.rom"
         || hleProgram == "dsp2.program.rom" || hleProgram == "dsp3.program.rom"
         || hleProgram == "dsp4.program.rom")
      : (root["model"].data == "uPD96050" && hleProgram == "st010.program.rom");
  if(hleSupported && interface->bind->altImplementation(Alt::ForDSP)==Alt::DSP::HLE)''')
source = replace_once(source, '''  } else {
    Mapping m({&ST0010::read, &st0010}, {&ST0010::write, &st0010});''', '''  } else {
    // R2N64: enable the existing reset, power and serialization hooks.
    has_st0010 = true;
    Mapping m({&ST0010::read, &st0010}, {&ST0010::write, &st0010});''')
save('bsnes-mercury', 'sfc/cartridge/markup.cpp', source)
# Upstream's unused standalone frontend profile contains boot-ROM files; they
# are neither build inputs nor assets for our libretro integration.
for relative in ('profile/Super Famicom.sys/ipl.rom', 'profile/Game Boy.sys/boot.rom',
                 'profile/Game Boy Color.sys/boot.rom'):
    path = build / 'bsnes-mercury-source' / relative
    if path.exists():
        assert path.resolve().is_relative_to(build.resolve())
        path.unlink()
