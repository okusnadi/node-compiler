/*
 * Copyright (c) 2016-2017 Minqi Pan and Shengyuan Liu
 *
 * This file is part of libsquash, distributed under the MIT License
 * For full terms see the included LICENSE file
 */

#include "squash.h"
#include "fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect(short condition, const char *reason)
{
	if (condition) {
		fprintf(stderr, ".");
	}
	else {
		fprintf(stderr, "x");
		fprintf(stderr, "\nFAILED: %s\n", reason);
		exit(1);
	}
	fflush(stderr);
}

static void test_basic_func()
{
	sqfs fs;
	sqfs_err ret;
	sqfs_inode root, node;
	sqfs_dir dir;
	sqfs_dir_entry entry;
	sqfs_name name;

	bool found;
	bool has_next;
	struct stat st;
	size_t name_size = sizeof(name);
	char buffer[1024];
	sqfs_off_t offset;

	fprintf(stderr, "Testing basic functionalities\n");
	fflush(stderr);

	// libsquash_fixture => sqfs
	memset(&fs, 0, sizeof(sqfs));
	ret = sqfs_open_image(&fs, libsquash_fixture, 0);
	expect(SQFS_OK == ret, "sqfs_open_image should succeed");
	expect(1481779989 == fs.sb->mkfs_time, "fs made at 2016-12-15 13:33:09 +0800");
	
	// sqfs => root sqfs_inode
	memset(&root, 0, sizeof(sqfs_inode));
	ret = sqfs_inode_get(&fs, &root, sqfs_inode_root(&fs));
	expect(SQFS_OK == ret, "successfully read the root inode");
	expect(SQUASHFS_DIR_TYPE == root.base.inode_type, "got a dir as the root");
	expect(1481778144 == root.base.mtime, "2016-12-15 13:02:24 +0800");
	
	// "/" => sqfs_inode and stat
	memcpy(&node, &root, sizeof(sqfs_inode));
	ret = sqfs_lookup_path(&fs, &node, "/", &found);
	expect(found, "of course we can find root");
	expect(SQFS_OK == ret, "happy sqfs_lookup_path");
	ret = sqfs_stat(&fs, &node, &st);
	expect(SQFS_OK == ret, "happy sqfs_stat");
	expect(S_ISDIR(st.st_mode), "stat thinks root is a dir");

	// "/what/the/f" => not found
	memcpy(&node, &root, sizeof(sqfs_inode));
	ret = sqfs_lookup_path(&fs, &node, "/what/the/f", &found);
	expect(SQFS_OK == ret, "sqfs_lookup_path is still happy");
	expect(!found, "but this thing does not exist");

	// "even_without_leading_slash" => not found
	memcpy(&node, &root, sizeof(sqfs_inode));
	ret = sqfs_lookup_path(&fs, &node, "even_without_leading_slash", &found);
	expect(SQFS_OK == ret, "sqfs_lookup_path is still happy");
	expect(!found, "but this thing does not exist");

	// ls "/"
	memcpy(&node, &root, sizeof(sqfs_inode));
	sqfs_lookup_path(&fs, &node, "/", &found);
	ret = sqfs_dir_open(&fs, &node, &dir, 0);
	expect(SQFS_OK == ret, "sqfs dir open is happy");
	sqfs_dentry_init(&entry, name);
	has_next = sqfs_dir_next(&fs, &dir, &entry, &ret);
	expect(0 == strcmp(sqfs_dentry_name(&entry), "bombing"), "/bombing");
	expect(S_ISREG(sqfs_dentry_mode(&entry)), "bombing is a regular file");
	expect(has_next, "bombing -> dir1/");
	has_next = sqfs_dir_next(&fs, &dir, &entry, &ret);
	expect(0 == strcmp(sqfs_dentry_name(&entry), "dir1"), "/dir1/");
	expect(S_ISDIR(sqfs_dentry_mode(&entry)), "dir1/ is a dir");
	expect(has_next, "dir1/ -> EOF");
	has_next = sqfs_dir_next(&fs, &dir, &entry, &ret);
	expect(!has_next, "EOF");

	// ls "/dir1"
	memcpy(&node, &root, sizeof(sqfs_inode));
	sqfs_lookup_path(&fs, &node, "/dir1", &found);
	ret = sqfs_dir_open(&fs, &node, &dir, 0);
	expect(SQFS_OK == ret, "sqfs dir open is happy");
	sqfs_dentry_init(&entry, name);
	has_next = sqfs_dir_next(&fs, &dir, &entry, &ret);
	expect(0 == strcmp(sqfs_dentry_name(&entry), ".0.0.4@something4"), "/.0.0.4@something4/");
	expect(S_ISDIR(sqfs_dentry_mode(&entry)), ".0.0.4@something4 is a dir");
	expect(has_next, ".0.0.4@something4 -> .bin");
	has_next = sqfs_dir_next(&fs, &dir, &entry, &ret);
	expect(0 == strcmp(sqfs_dentry_name(&entry), ".bin"), "/.bin/");
	expect(S_ISDIR(sqfs_dentry_mode(&entry)), ".bin is a dir");
	expect(has_next, ".bin -> @minqi");
	has_next = sqfs_dir_next(&fs, &dir, &entry, &ret);
	expect(0 == strcmp(sqfs_dentry_name(&entry), "@minqi"), "/@minqi/");
	expect(S_ISDIR(sqfs_dentry_mode(&entry)), "@minqi is a dir");
	expect(has_next, "@minqi -> something4");
	has_next = sqfs_dir_next(&fs, &dir, &entry, &ret);
	expect(0 == strcmp(sqfs_dentry_name(&entry), "something4"), "/something4");
	expect(S_ISLNK(sqfs_dentry_mode(&entry)), "something4 is a symlink");
	expect(has_next, ".0.0.4@something4 -> EOF");
	has_next = sqfs_dir_next(&fs, &dir, &entry, &ret);
	expect(!has_next, "EOF");

	// readlink "/dir1/something4"
	memcpy(&node, &root, sizeof(sqfs_inode));
	sqfs_lookup_path(&fs, &node, "/dir1/something4", &found);
	expect(found, "we can find /dir1/something4");
	sqfs_stat(&fs, &node, &st);
	expect(S_ISLNK(node.base.mode), "/dir1/something4 is a link");
	ret = sqfs_readlink(&fs, &node, name, &name_size);
	expect(SQFS_OK == ret, "sqfs_readlink is happy");
	expect(0 == strcmp(name, ".0.0.4@something4"), "something4 links to .0.0.4@something4");

	// read "/bombing"
	memcpy(&node, &root, sizeof(sqfs_inode));
	sqfs_lookup_path(&fs, &node, "/bombing", &found);
	expect(found, "we can find /bombing");
	sqfs_stat(&fs, &node, &st);
	expect(S_ISREG(node.base.mode), "/bombing is a regular file");
	expect(998 == node.xtra.reg.file_size, "bombing is of size 998");
	offset = node.xtra.reg.file_size;
	ret = sqfs_read_range(&fs, &node, 0, &offset, buffer);
	expect(buffer == strstr(buffer, "Botroseya Church bombing"), "read some content of the file");
	expect(NULL != strstr(buffer, "Iraq and the Levant"), "read some content of the file");

	// RIP.
	sqfs_destroy(&fs);

	fprintf(stderr, "\n");
	fflush(stderr);
}

static void test_stat()
{
	sqfs fs;
	struct stat st;
	sqfs_err error;
	int ret;
	int fd;

	fprintf(stderr, "Testing stat functions\n");
	fflush(stderr);
	memset(&fs, 0, sizeof(sqfs));
	sqfs_open_image(&fs, libsquash_fixture, 0);
	
	// stat "/"
	ret = squash_stat(&error, &fs, "/", &st);
	expect(0 == ret, "Upon successful completion a value of 0 is returned");
	expect(S_ISDIR(st.st_mode), "/ is a dir");
	ret = squash_lstat(&error, &fs, "/", &st);
	expect(0 == ret, "Upon successful completion a value of 0 is returned");
	expect(S_ISDIR(st.st_mode), "/ is a dir");
	
	// stat "/bombing"
	ret = squash_stat(&error, &fs, "/bombing", &st);
	expect(0 == ret, "Upon successful completion a value of 0 is returned");
	expect(S_ISREG(st.st_mode), "/bombing is a regular file");
	ret = squash_lstat(&error, &fs, "/bombing", &st);
	expect(0 == ret, "Upon successful completion a value of 0 is returned");
	expect(S_ISREG(st.st_mode), "/bombing is a regular file");
	fd = squash_open(&error, &fs, "/bombing");
	ret = squash_fstat(&error, &fs, fd, &st);
	expect(0 == ret, "Upon successful completion a value of 0 is returned");
	expect(S_ISREG(st.st_mode), "/bombing is a regular file");
	squash_close(&error, fd);
	
	//stat /dir/something4
	ret = squash_lstat(&error, &fs, "/dir1/something4", &st);
	expect(0 == ret, "Upon successful completion a value of 0 is returned");
	expect(S_ISLNK(st.st_mode), "/dir1/something4 is a symbolic link file");

	//stat /dir/something4
	ret = squash_stat(&error, &fs, "/dir1/something4", &st);
	expect(0 == ret, "Upon successful completion a value of 0 is returned");
	expect(S_ISDIR(st.st_mode), "/dir1/something4 is a symbolic link file and references is a dir");

	// RIP.
	sqfs_destroy(&fs);

	fprintf(stderr, "\n");
	fflush(stderr);
}

static void test_virtual_fd()
{
	int fd, fd2, fd3, fd4;
	sqfs fs;
	sqfs_err error;
	int ret;
	ssize_t ssize;
	char buffer[1024];
	sqfs_off_t offset;
	struct squash_file *file;

	fprintf(stderr, "Testing virtual file descriptors\n");
	fflush(stderr);
	memset(&fs, 0, sizeof(sqfs));
	sqfs_open_image(&fs, libsquash_fixture, 0);

	// open "/bombing"
	fd = squash_open(&error, &fs, "/bombing");
	expect(fd > 0, "successfully got a fd");
	fd2 = squash_open(&error, &fs, "/bombing");
	expect(fd2 > 0, "successfully got yet another fd");
	expect(fd2 != fd, "it is indeed another fd");
	fd3 = squash_open(&error, &fs, "/shen/me/gui");
	expect(-1 == fd3, "on failure returns -1");
	expect(SQFS_NOENT == error, "no such file");
	expect(SQUASH_VALID_VFD(fd), "fd is ours");
	expect(SQUASH_VALID_VFD(fd2), "fd2 is also ours");
	expect(!SQUASH_VALID_VFD(0), "0 is not ours");
	expect(!SQUASH_VALID_VFD(1), "1 is not ours");
	expect(!SQUASH_VALID_VFD(2), "2 is not ours");
	
	// read on and on
	file = SQUASH_VFD_FILE(fd);
	offset = file->node.xtra.reg.file_size;
	ssize = squash_read(&error, fd, buffer, 1024);
	expect(offset == ssize, "When successful it returns the number of bytes actually read");
	expect(buffer == strstr(buffer, "Botroseya Church bombing"), "read some content of the file");
	ssize = squash_read(&error, fd, buffer, 1024);
	expect(0 == ssize, "upon reading end-of-file, zero is returned");
	fd4 = squash_open(&error, &fs, "/");
	ssize = squash_read(&error, fd4, buffer, 1024);
	expect(-1 == ssize, "not something we can read");

	// read with lseek
	ret = squash_lseek(&error, fd, 3, SQUASH_SEEK_SET);
	expect(3 == ret, "Upon successful completion, it returns the resulting offset location as measured in bytes from the beginning of the file.");
	ssize = squash_read(&error, fd, buffer, 1024);
	expect(offset - 3 == ssize, "When successful it returns the number of bytes actually read");
	expect(buffer != strstr(buffer, "Botroseya Church bombing"), "read some content of the file");
	expect(buffer == strstr(buffer, "roseya Church bombing"), "read some content of the file");
	ssize = squash_read(&error, fd2, buffer, 100);
	ret = squash_lseek(&error, fd2, 10, SQUASH_SEEK_CUR);
	expect(110 == ret, " the offset is set to its current location plus offset bytes");
	ssize = squash_read(&error, fd2, buffer, 100);
	expect(buffer == strstr(buffer, "s at St. Peter"), "read from offset 110");
	ret = squash_lseek(&error, fd2, 0, SQUASH_SEEK_END);
	ssize = squash_read(&error, fd2, buffer, 1024);
	expect(0 == ssize, "upon reading end-of-file, zero is returned");

	// various close
	ret = squash_close(&error, fd);
	expect(0 == ret, "RIP: fd");
	ret = squash_close(&error, fd2);
	expect(0 == ret, "RIP: fd2");
	ret = squash_close(&error, 0);
	expect(-1 == ret, "cannot close something we do not own");
	expect(SQFS_INVALFD == error, "invalid vfd is the reason");
	expect(!SQUASH_VALID_VFD(fd), "fd is no longer ours");
	expect(!SQUASH_VALID_VFD(fd2), "fd2 is no longer ours");

	// RIP.
	sqfs_destroy(&fs);

	fprintf(stderr, "\n");
	fflush(stderr);
}

int filter_scandir(const struct dirent * ent)
{
	return (strncmp(ent->d_name,".",1) == 0);
}

int reverse_alpha_compar(const struct dirent **a, const struct dirent **b){
	return -strcmp((*a)->d_name, (*b)->d_name);
}
#ifndef __linux__
int alphasort(const struct dirent **a, const struct dirent **b){
	return strcmp((*a)->d_name, (*b)->d_name);
}

#endif

static void test_dirent()
{
	fprintf(stderr, "Testing dirent APIs\n");
	fflush(stderr);

	sqfs fs;
	sqfs_err error;
	int ret;
	int fd;
	SQUASH_DIR *dir;
	struct dirent *mydirent;

	memset(&fs, 0, sizeof(sqfs));
	sqfs_open_image(&fs, libsquash_fixture, 0);
	
	dir = squash_opendir(&error, &fs, "/dir1-what-the-f");
	expect(NULL == dir, "on error NULL is returned");
	dir = squash_opendir(&error, &fs, "/dir1");
	expect(NULL != dir, "returns a pointer to be used to identify the dir stream");
	expect(SQUASH_VALID_DIR(dir), "got a valid SQUASH_DIR");
	fd = squash_dirfd(&error, dir);
	expect(fd > 0, "returns a vfs associated with the named diretory stream");
	mydirent = squash_readdir(&error, dir);
	expect(NULL != mydirent, "returns a pointer to the next directory entry");
	expect(0 == strcmp(".0.0.4@something4", mydirent->d_name), "got .0.0.4@something4");
#ifndef __linux__
	expect(strlen(".0.0.4@something4") == mydirent->d_namlen, "got a str len");
#endif
	expect(DT_DIR == mydirent->d_type, "this ia dir");
	mydirent = squash_readdir(&error, dir);
	expect(NULL != mydirent, "returns a pointer to the next directory entry");
	expect(0 == strcmp(".bin", mydirent->d_name), "got a .bin");
#ifndef __linux__
	expect(strlen(".bin") == mydirent->d_namlen, "got a str len");
#endif
	expect(DT_DIR == mydirent->d_type, "this a dir");
	mydirent = squash_readdir(&error, dir);
	expect(NULL != mydirent, "got another entry");
	expect(0 == strcmp("@minqi", mydirent->d_name), "got a @minqi");
#ifndef __linux__
	expect(strlen("@minqi") == mydirent->d_namlen, "got a str len");
#endif
	expect(DT_DIR == mydirent->d_type, "got yet another dir");
	mydirent = squash_readdir(&error, dir);
	expect(NULL != mydirent, "got another entry");
	expect(0 == strcmp("something4", mydirent->d_name), "this is named something4");
#ifndef __linux__
	expect(strlen("something4") == mydirent->d_namlen, "got a strlen");
#endif
	expect(DT_LNK == mydirent->d_type, "so this one is a link");
	mydirent = squash_readdir(&error, dir);
	expect(NULL == mydirent, "finally reaching an EOF");
	long pos = squash_telldir(dir);
	squash_rewinddir(dir);
	mydirent = squash_readdir(&error, dir);
	expect(NULL != mydirent, "starting all over again");
	expect(0 == strcmp(".0.0.4@something4", mydirent->d_name), "got .0.0.4@something4");
	squash_seekdir(dir, pos);
	mydirent = squash_readdir(&error, dir);
	expect(NULL == mydirent, "back to before");
	ret = squash_closedir(&error, dir);
	expect(0 == ret, "returns 0 on success");

	dir = squash_opendir(&error, &fs, "/dir1/.bin");
	expect(NULL != dir, "returns a pointer to be used to identify the dir stream");
	mydirent = squash_readdir(&error, dir);
	expect(NULL == mydirent, "oops empty dir");

	struct dirent **namelist = 0;

	int numEntries = squash_scandir(&error, &fs, "/dir1", &namelist, filter_scandir, alphasort);

	expect(2 == numEntries, "scandir_filter is happy");


	expect(NULL != namelist[0], "returns a pointer to the next directory entry");
	expect(0 == strcmp(".0.0.4@something4", namelist[0]->d_name), "got .0.0.4@something4");
#ifndef __linux__
	expect(strlen(".0.0.4@something4") == namelist[0]->d_namlen, "got a str len");
#endif
	expect(DT_DIR == namelist[0]->d_type, "this ia dir");

	expect(NULL != namelist[1], "returns a pointer to the next directory entry");
	expect(0 == strcmp(".bin", namelist[1]->d_name), "got a .bin");
#ifndef __linux__
	expect(strlen(".bin") == namelist[1]->d_namlen, "got a str len");
#endif

	int i = 0;

	for(i = 0; i < numEntries; i++){
		free(namelist[i]);
	}
	free(namelist);


	namelist = 0;
	numEntries = squash_scandir(&error, &fs, "/", &namelist, NULL, reverse_alpha_compar);
	expect(2 == numEntries, "scandir_alphasort is happy");


	expect(NULL != namelist[0], "returns a pointer to the next directory entry");
	expect(0 == strcmp("dir1", namelist[0]->d_name), "got a dir1");
	expect(DT_DIR == namelist[0]->d_type, "this is a reg");
#ifndef __linux__
	expect(strlen("dir1") == namelist[0]->d_namlen, "got a str len");
#endif

	expect(NULL != namelist[1], "returns a pointer to the next directory entry");
	expect(0 == strcmp("bombing", namelist[1]->d_name), "got bombing");
#ifndef __linux__
	expect(strlen("bombing") == namelist[1]->d_namlen, "got a str len");
#endif
	expect(DT_REG == namelist[1]->d_type, "this is a reg");



	for(i = 0; i < numEntries; i++){
		free(namelist[i]);
	}
	free(namelist);

	fprintf(stderr, "\n");
	fflush(stderr);
}

static void test_squash_readlink()
{
	fprintf(stderr, "Testing squash_readlink...\n");
	fflush(stderr);

	sqfs_inode root, node;
	sqfs fs;
	sqfs_err error;
	int ret;
	int fd;
	bool found = false;

	sqfs_name name;
	size_t name_size = sizeof(name);
	struct stat st;
	memset(&st, 0, sizeof(st));

	memset(&fs, 0, sizeof(sqfs));
	sqfs_open_image(&fs, libsquash_fixture, 0);

	ssize_t  readsize = 0;
	readsize = squash_readlink(&error, &fs, "/dir1/something4" ,(char *)&name, name_size);
	expect(SQFS_OK == error, "squash_readlink is happy");
	char content[] = ".0.0.4@something4";
	expect(0 == strcmp(name, content), "something4 links to .0.0.4@something4");
	expect(strlen(content) == readsize, "squash_readlink return value is happy");

	readsize = squash_readlink(0, &fs, "/dir1/something4" ,(char *)&name, name_size);
	expect(-1 == readsize, "squash_readlink ‘error’ is null");

	char smallbuf[2] = {0,0};
	readsize = squash_readlink(&error, &fs, "/dir1/something4" ,smallbuf, 2);
	expect(-1 == readsize, "squash_readlink ‘buf’ is too small ret val");
	expect(SQFS_ERR == error, "squash_readlink ‘buf’ is too small");

	readsize = squash_readlink(&error, &fs, "/dir1/something123456" ,smallbuf, 2);
	expect(-1 == readsize, "squash_readlink no such file ret val");
	expect(SQFS_NOENT == error, "squash_readlink no such file error");
	fprintf(stderr, "\n");
	fflush(stderr);



}

int main(int argc, char const *argv[])
{
	squash_start();

	test_basic_func();
	test_stat();
	test_virtual_fd();
	test_dirent();
	test_squash_readlink();

	squash_halt();
	return 0;
}
