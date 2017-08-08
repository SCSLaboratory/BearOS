/* disklabel.c -- utility for reading and writing disklabels to disks
 *
 * Copyright (c) 2011 Bear System. All rights reserved.
*/

/* FIXME: UUIDs are just all zero at the moment. */

#include <argp.h>
#include <error.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <disklabel.h>
#include <mbr_partition.h>

static char program[] = "disklabel";
static char doc[] = "disklabel -- a program for reading and writing disklabels";
static char args_doc[] = "slice";
// static char version[] = "0.1";
// static char bug_address[] = "morgon.j.kanter@dartmouth.edu";
static struct argp_option options[] = {
	{ "bootstrap", 'b', "FILE", 0, "Location of the bootstrap (stage2) code" },
	{ "stage1", '1', "FILE", 0, "Location of the stage1 code" },
	{ "dry-run", 'n', 0, 0, "Don't actually write anything, just print to stdout" },
	{ "mbr", 'm', 0, 0, "Determine slice to label from the MBR (labels first BSD slice)" },
	{ "read", 'r', 0, 0, "Read disklabel from slice" },
	{ "write", 'w', 0, 0, "Write disklabel to slice" },
	{ "update", 'u', 0, 0, "Update label when writing (leave bootstrap alone)" },
	{ "force", 'f', 0, 0, "Clobber bootstrap code on disk if we can't find it, or -b is not specified" },
	{ 0 }
};
struct arguments {
	char *slice;
	char *bootstrap;
	char *stage1;
	int mbr;
	int read;
	int write;
	int update;
	int dryrun;
	int force;
} *args;

/* Linked list of partitions. */
struct list {
	struct partition val;
	struct list *next;
};

struct {
	FILE *disk;
	FILE *stage1;
	FILE *bootstrap;
	size_t bootstrap_len;
	struct disklabel *label;
	struct list *ptable;
	int slice_point;                         /* if called with -m */
} state;

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = (struct arguments *)state->input;

	switch(key) {
	case 'b':
		arguments->bootstrap = arg;
		break;
	case '1':
		arguments->stage1 = arg;
		break;
	case 'm':
		arguments->mbr = 1;
		break;
	case 'r':
		arguments->read = 1;
		break;
	case 'w':
		arguments->write = 1;
		break;
	case 'n':
		arguments->dryrun = 1;
		break;
	case 'f':
		arguments->force = 1;
		break;
	case ARGP_KEY_ARG:
		if(state->arg_num >= 1)
			argp_usage(state);
		arguments->slice = arg;
		break;
	case ARGP_KEY_END:
		if(state->arg_num < 1)
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/* argp parser */
static struct argp argp = { options, parse_opt, args_doc, doc };

static void arg_sanity() {
	if(args->read && args->write) {
		fputs("Error: Cannot specify both '-r' and '-w'.\n", stderr);
		argp_help(&argp, stderr, ARGP_HELP_STD_ERR, program);
	}

	if(!(args->read || args->write)) {
		fputs("Error: Must specify either '-r' or '-w'.\n", stderr);
		argp_help(&argp, stderr, ARGP_HELP_STD_ERR, program);
	}

	if(!(args->force) && (args->write && !(args->bootstrap))) {
		fputs("Error: No bootstrap code specified for writing (and '-f' omitted).\n"
		      "\tIf this is really what you want to do, specify '-f'.\n", stderr);
		argp_help(&argp, stderr, ARGP_HELP_STD_ERR, program);
	}

	if(args->write && args->update && !(args->bootstrap)) {
		fputs("Error: Trying to update label, but no bootstrap code specified.\n"
		      "\tMake sure you specify '-b'.\n", stderr);
		argp_help(&argp, stderr, ARGP_HELP_STD_ERR, program);
	}
}

/* Read the partition table and find the first BSD partition to drop the slice on. */
static int determine_slice(FILE *disk) {
	struct mbr_partition p[4];
	int i;
	int e;

	/* Seek to the start of the partition table.
	 * The file is currently sitting at the start of the MBR.
	*/
	e = fseek(disk, 446, SEEK_SET);
	if(e < 0)
		error(1, 0, "Could not seek to MBR partition table.");

	e = fread(p, sizeof(struct mbr_partition), 4, disk);
	if(e < 4)
		error(1, 0, "Could not completely read MBR partition table.");

	/* Quick hack to print out the MBR information we need as well. */
	if(args->mbr && args->read)
		for(i = 0; i < 4; i++)
			printf("Slice %d: starts %u, ends %u\n", i,
			       p[i].lba*512, p[i].lba*512 + p[i].numsectors*512);

	for(i = 0; i < 4; i++)
		if(p[i].ptype == 0xA5)
			return p[i].lba * 512;

	/* Could not find a BSD partition. */
	return -1;
}

/* Opens the disk and returns it set to the point where the disklabel begins. */
FILE *open_disk(char *filename) {
	FILE *f;
	char mode[4];
	char operation[8];

	if(args->read) {
		strcpy(mode, "rb");
		strcpy(operation, "reading");
	} else {
		strcpy(mode, "r+b");
		strcpy(operation, "writing");
	}

	/* First open the disk. */
	f = fopen(filename, mode);
	if(f == NULL)
		error(1, errno, "Failed to open disk for %s", operation);

	/* Now read in the slice table from the MBR if we need to do that. */
	if(args->mbr) {
		int seek = determine_slice(f);
		if(seek < 0)
			error(1, 0, "Failed to find a valid BSD slice in the MBR.");
		state.slice_point = seek;
		seek = fseek(f, seek, SEEK_SET);
		if(seek < 0)
			error(1, errno, "Failed to seek to start of disklabel.");
	} else {
		state.slice_point = 0;
	}
	
	/* We keep the file position at the start of the slice. The first 512 bytes is actually
	 * boot1 code and not part of the label, but it's included in the label structure for
	 * portability reasons so we can just read it in / write it out without worries.
	*/

	return f;
}

FILE *open_bootstrap(char *filename) {
	FILE *f;
	int e;

	if(filename == NULL)
		return NULL;

	f = fopen(filename, "rb");
	if(f == NULL)
		error(1, errno, "Failed to open bootstrap file.");

	/* Get the bootstrap length. */
	e = fseek(f, 0, SEEK_END);
	if(e == -1)
		error(1, 0, "Could not seek to end of bootstrap file.");
	state.bootstrap_len = ftell(f);
	if(state.bootstrap_len == -1)
		error(1, 0, "Could not discover length of bootstrap file.");
	e = fseek(f, 0, SEEK_SET);
	if(e == -1)
		error(1, 0, "Could not seek to beginning of bootstrap file.");

	return f;
}

FILE *open_stage1(char *filename) {
	FILE *f;

	if(filename == NULL)
		return NULL;

	f = fopen(filename, "rb");
	return f;
}

static void new_label() {
	struct disklabel *label = state.label;
	struct partition *p;

	/* First 512 bytes is from the stage1 code, or zero if it doesn't exist. */
	if(args->stage1) {
		int i;
		int c;
		for(i = 0; i < 512; i++) {
			c = fgetc(state.stage1);
			if(c == EOF)
				error(1, 0, "Failed to read stage1 code.");
			label->reserved0[i] = c;
		}
	}

	label->magic = DISKMAGIC;
	label->align = 1024*1024;                /* FIXME: 1 MB for now */
	label->npartitions = 1;
	memset(&(label->stor_uuid), 0, sizeof(struct uuid));
	label->total_size = sizeof(struct disklabel) + sizeof(struct partition);
	label->bbase = label->total_size + (label->total_size % 512);
	label->pbase = label->bbase + state.bootstrap_len;
	label->pstop = 1024*1024*500;            /* FIXME: 500 MB for now */
	label->abase = 0;
	strcpy((char *)(label->packname), "nopack");

	/* Now for the partition table. */
	p = &(state.ptable->val);
	p->boffset = label->pbase + (512 - (label->pbase % 512)); /* 512-byte aligned */
	p->bsize = label->pstop - p->boffset;
	p->fstype = 8;                           /* MS-DOS (FAT) */
	memset(&(p->p_type_uuid), 0, sizeof(struct uuid));
	memset(&(p->p_stor_uuid), 0, sizeof(struct uuid));

	/* last node */
	state.ptable->next = malloc(sizeof(struct list));
	memset(state.ptable->next, 0, sizeof(struct list));
}

static void read_label() {
	/* It comes off the disk in exactly the format of struct disklabel. */
	struct disklabel *label = state.label;
	struct list *p;
	size_t size;
	int i;

	size = sizeof(struct disklabel);
	size = fread(label, size, 1, state.disk);
	if(size == 0)
		error(1, 0, "Error while reading disklabel from hard drive.");

	/* Read the partition table. */
	p = state.ptable;
	for(i = 0; i < label->npartitions; i++) {
		size = sizeof(struct partition);
		size = fread(&(p->val), size, 1, state.disk);
		if(size == 0)
			error(1, 0, "Error while reading partition table from hard drive.");

		p->next = malloc(sizeof(struct partition));
		memset(p->next, 0, sizeof(struct partition));
		p = p->next;
	}
}

static void update_label() {
	/* First must read the disklabel. */
	read_label();

	/* Now do the updates... */
}

void construct_label() {
	state.label = malloc(sizeof(struct disklabel));
	memset(state.label, 0, sizeof(struct disklabel));
	state.ptable = malloc(sizeof(struct list));
	memset(state.ptable, 0, sizeof(struct list));

	if(args->read)
		read_label();
	else if(args->write && args->update)
		update_label();
	else
		new_label();
}

/* Write out the label to disk. */
void commit() {
	struct list *p;
	int seek = 0;

	/* Commit the label. */
	if(args->mbr)
		seek += state.slice_point;

	seek = fseek(state.disk, seek, SEEK_SET);
	if(seek < 0)
		error(1, 0, "Failed to seek to beginning of disk for commit.");

	seek = fwrite(state.label, sizeof(struct disklabel), 1, state.disk);
	if(seek < 1)
		error(1, 0, "Failed to commit the disklabel.");

	/* Commit the partition table. */
	for(p = state.ptable; p->val.fstype != 0; p = p->next) {
		seek = fwrite(&(p->val), sizeof(struct partition), 1, state.disk);
		if(seek < 1)
			error(1, 0, "Failed to commit a partition.");
	}

	/* Commit the bootstrap code, if available. */
	if(args->bootstrap) {
		int buffer = EOF-1;
		seek = fseek(state.disk, state.slice_point + state.label->bbase, SEEK_SET);
		if(seek < 0)
			error(1, 0, "Failed to seek to the bootstrap code.");
		buffer = fgetc(state.bootstrap);
		while(buffer != EOF) {
			fputc(buffer, state.disk);
			if(buffer == EOF)
				error(1, 0, "Failed to write the bootstrap code.");
			buffer = fgetc(state.bootstrap);
		}
	}
}

void print_label() {
	struct list *p;
	int i;

	puts("Label parameters:");
	printf("magic: %x\n", state.label->magic);
	printf("crc: %x\n", state.label->crc);
	printf("align: %u\n", state.label->align);
	printf("npartitions: %u\n", state.label->npartitions);
	printf("total_size: %lu\n", state.label->total_size);
	printf("bbase: %lu\n", state.label->bbase);
	printf("pbase: %lu\n", state.label->pbase);
	printf("pstop: %lu\n", state.label->pstop);
	printf("abase: %lu\n", state.label->abase);

	for(p = state.ptable, i = 1; p->val.bsize != 0; p = p->next, i++) {
		printf("Partition %d:\n", i);
		printf("\tOffset: %lu\n", p->val.boffset);
		printf("\tSize: %lu\n", p->val.bsize);
		printf("\tType: %d\n", p->val.fstype);
	}
}

int main(int argc, char **argv) {
	struct arguments arguments;

	memset(&arguments, 0, sizeof(struct arguments));
	args = &arguments;
	argp_parse(&argp, argc, argv, 0, 0, args);
	arg_sanity();

	state.disk = open_disk(args->slice);
	state.stage1 = open_stage1(args->stage1);
	state.bootstrap = open_bootstrap(args->bootstrap);
	construct_label();

	if(args->read) {
		print_label();
	} else {
		commit();
		puts("Label committed to disk.");
	}

	if(state.bootstrap != NULL)
		fclose(state.bootstrap);
	fclose(state.disk);

	return 0;
}
