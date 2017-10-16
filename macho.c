#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/fat.h>

#define SWAP32(X)	\
	(				\
		((X & 0x000000ffUL) << 24) | \
		((X & 0x0000ff00UL) << 8)  | \
		((X & 0x00ff0000UL) >> 8)  | \
		((X & 0xff000000UL) >> 24) 	\
	)

int 
parseLC_SEGMENT(_buffer, _offset, si, ti)
	const unsigned char* _buffer;
	size_t _offset;
	unsigned int *si;
	unsigned int *ti;
{
    struct segment_command *segment;
	struct section *section;
	unsigned int i;

	segment = (struct segment_command *)(_buffer + _offset);
	if (0 != strncmp(segment->segname, "__TEXT", sizeof("__TEXT"))) {
		*si += segment->nsects;
		fprintf(stderr, "LC_SEGMENT: skip = %u(%s)\n", *si, segment->segname);
		return 0;
	}

	/* セクション */
	section = (struct section *)(_buffer + _offset + sizeof(*segment));
	for (i = 0; i < segment->nsects; i++, section++) {
		*si++;
		/* __textセクションが何番目のセクションかを記録 */
		//if (0 == strncmp(section->sectname, "__text", sizeof("__text"))) {
			*ti = *si;
			fprintf(stderr, "LC_SEGMENT: %08d _text = %u \n", si, *si);
		//}
	}
	return 0;
}

int 
parseLC_SYMTAB(_buffer, _offset, si, ti)
	const unsigned char* _buffer;
	size_t _offset;
	unsigned int *si;
	unsigned int *ti;
{

	struct symtab_command *table;
    struct nlist *symbol;
	unsigned char *string_table, *name;
	int i, string_offset;
	
	table = (struct symtab_command *)(_buffer +_offset);
	symbol = (struct nlist *)(_buffer + table->symoff);
	string_table = _buffer + table->stroff;
	
	for (i = 0; i < table->nsyms; i++, symbol++) {
		string_offset = symbol->n_un.n_strx;
		name = string_table + string_offset;
		fprintf(stdout, "LC_SYMTAB: 0x%08x: %s\n", symbol->n_value, name +1);
	}
	return 0;
}

int 
parsLC_LOAD_DYLIB(_buffer, _offset, si, ti)
	const unsigned char* _buffer;
	size_t _offset;
	unsigned int *si;
	unsigned int *ti;
{
	struct dylib_command *dyld;
	unsigned char *string_table, *name;
	
	/* _dl() 使用禁止なんで意味ない */
	return 0;
}

int
parseSegments(_buffer)
	const unsigned char* _buffer;
{
	struct mach_header *header;
	unsigned int section_index = 0, text_section_index = 0;
	struct load_command *load;
	size_t offset;
	unsigned int i, n_commands;
	unsigned char *content = (unsigned char*)_buffer;
	
	header = (struct mach_header*)_buffer;
	if(MH_MAGIC != header->magic) {
		return puts("not mach-0");
	}

	offset = sizeof(*header);
	n_commands =  header->ncmds;
	
	fprintf(stderr, "commands = %u\n", n_commands);

	for (i = 0; i < n_commands; ++i) {
		load = (struct load_command *)(content + offset);
		switch (load->cmd) {
			case LC_SEGMENT:
				fprintf(stderr, "command = LC_SEGMENT\n");
				parseLC_SEGMENT(content, offset, &section_index, &text_section_index);
				break;
			case LC_SYMTAB:
				fprintf(stderr, "command = LC_SYMTAB\n");
				parseLC_SYMTAB(content, offset, &section_index, &text_section_index);
				break;
			case LC_LOAD_DYLIB:
				puts("LC_LOAD_DYLIB\n");
				break;
			default:
				fprintf(stderr, "command = %x\n", load->cmd);
				break;
		}
		offset += load->cmdsize;
	}
	return 0;
}

int 
parseHeader(_buffer)
	const unsigned char* _buffer;
{
	struct fat_header fh;
	struct fat_arch fa;
	unsigned int magic, i;
	unsigned char *content = (unsigned char*)_buffer;

	magic = *(unsigned int*)content;
	if ((FAT_MAGIC == magic) || (FAT_CIGAM == magic)) {
		puts("fat");
	}
	else if (MH_MAGIC == magic) {
		puts("macho-O");
		return parseSegments(_buffer);
	}
	else {
		return puts("unknown");
	}
	
	memcpy(&fh, content, sizeof(struct fat_header));
	if (FAT_CIGAM == magic) {
		/* CIGAM の場合はビッグエンディアンなのでひっくり返す */
		 fh.nfat_arch = SWAP32(fh.nfat_arch);	
	}
	content += sizeof(struct fat_header);
	for (i = 0; i < fh.nfat_arch; i++) {
        memcpy(&fa, content, sizeof(struct fat_arch));
		if (magic == FAT_CIGAM) {
			fa.cputype = SWAP32(fa.cputype);
			fa.offset = SWAP32(fa.offset);
		}
		parseSegments(_buffer + fa.offset);

		content += sizeof(struct fat_arch);
	}

	return 0;
}


int 
main(argc, argv)
	int argc;
	char **argv;
{
	int fd;
	size_t fsize;
	unsigned char* contents;

	fd = open(argv[1], O_RDONLY, S_IREAD);
	if (-1 == fd) {
		return puts("invalid fd");
	}
	
	fsize = lseek(fd, 0LL, SEEK_END);
	fprintf(stderr, "size = %zu\n", fsize);
	
	contents = (unsigned char*)malloc(fsize + 4);
	if (NULL == contents) {
		return puts("alloc failed");
	}
	
	lseek(fd, 0LL, SEEK_SET);
	if (fsize != read(fd, contents, fsize)) {
		return puts("read err");
	}

	parseHeader(contents);
	
	free(contents);

	close(fd);
	return 0;
}


