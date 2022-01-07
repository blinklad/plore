typedef struct plore_path_ref {
	char *Absolute;
	char *FilePart;
} plore_path_ref;

typedef struct plore_file_listing_desc {
	plore_file_node Type;
	plore_path_ref Path;
	plore_time LastModification;
	u64 Bytes;
} plore_file_listing_desc;

typedef struct plore_file_listing_info_get_or_create_result {
	plore_file_listing_info *Info;
	b64 DidAlreadyExist;
} plore_file_listing_info_get_or_create_result;

internal plore_file_listing_info_get_or_create_result
GetOrCreateFileInfo(plore_file_context *Context, plore_path *Path) {
	plore_file_listing_info_get_or_create_result Result = {
		.Info = MapGet(&Context->FileInfo, Path).Value,	
	};
	
	if (Result.Info) {
		Result.DidAlreadyExist = true;
	} else {
		plore_map_insert_result InfoResult = MapInsert(&Context->FileInfo, Path, &ClearStruct(plore_file_listing_info));
		Assert(!InfoResult.DidAlreadyExist);
		Result.Info = InfoResult.Value;
	}
	return(Result);
}

internal plore_file_listing_desc
ListingFromFile(plore_file *File) {
	plore_file_listing_desc Result = {
		.Type = File->Type,
		.Path = {
			.Absolute = File->Path.Absolute,
			.FilePart = File->Path.FilePart,
		},
		.LastModification = File->LastModification,
		.Bytes = File->Bytes,
	};
	
	return(Result);
}

// NOTE(Evan): Is this needed?
internal plore_file_listing_desc
ListingFromDirectoryPath(plore_path *Path) {
	plore_file_listing_desc Result = {
		.Type = PloreFileNode_Directory,
		.Path = {
			.Absolute = Path->Absolute,
			.FilePart = Path->FilePart,
		},
	};
	
	return(Result);
}
