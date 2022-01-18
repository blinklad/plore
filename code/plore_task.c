PLATFORM_TASK(MoveFiles) {
	plore_move_files_header *Header = (plore_move_files_header *)Param;
	if (!Header || !Header->Count) return;
	for (u64 F = 0; F < Header->Count; F++) Platform->MoveFile(Header->sAbsolutePaths[F], Header->dAbsolutePaths[F]);
}