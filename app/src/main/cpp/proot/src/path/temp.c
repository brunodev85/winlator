#include <sys/types.h>  /* stat(2), opendir(3), */
#include <sys/stat.h>   /* stat(2), chmod(2), */
#include <unistd.h>     /* stat(2), rmdir(2), unlink(2), readlink(2), */
#include <errno.h>      /* errno(2), */
#include <dirent.h>     /* readdir(3), opendir(3), */
#include <string.h>     /* strcmp(3), */
#include <stdlib.h>     /* free(3), getenv(3), */
#include <stdio.h>      /* P_tmpdir, */
#include <talloc.h>     /* talloc(3), */

#include "cli/note.h"

/**
 * Return the path to a directory where temporary files should be
 * created.
 */
const char *get_temp_directory()
{
	static const char *temp_directory = NULL;
	char *tmp;

	if (temp_directory != NULL)
		return temp_directory;

	temp_directory = getenv("PROOT_TMP_DIR");
	if (temp_directory == NULL) {
		temp_directory = P_tmpdir;
		return temp_directory;
	}

	tmp = realpath(temp_directory, NULL);
	if (tmp == NULL) {
		note(NULL, WARNING, SYSTEM,
			"can't canonicalize %s, using %s instead of PROOT_TMP_DIR",
			temp_directory, P_tmpdir);

		temp_directory = P_tmpdir;
		return temp_directory;
	}

	temp_directory = talloc_strdup(talloc_autofree_context(), tmp);
	if (temp_directory == NULL)
		temp_directory = tmp;
	else
		free(tmp);

	return temp_directory;
}

/**
 * Handle the return of d_type = DT_UNKNOWN by readdir(3)
 * Not all filesystems support returning d_type in readdir(3)
 */
static int get_dtype(struct dirent *de)
{
	int dtype = de ? de->d_type : DT_UNKNOWN;
	struct stat st;

	if (dtype != DT_UNKNOWN)
		return dtype;
	if (lstat(de->d_name, &st))
		return dtype;
	if (S_ISREG(st.st_mode))
		return DT_REG;
	if (S_ISDIR(st.st_mode))
		return DT_DIR;
	if (S_ISLNK(st.st_mode))
		return DT_LNK;
	return dtype;
}

/**
 * Remove recursively the content of the current working directory.
 * This latter has to lie in temp_directory (ie. "/tmp" on most
 * systems).  This function returns -1 if a fatal error occured
 * (ie. the recursion must be stopped), the number of non-fatal errors
 * otherwise.
 *
 * WARNING: this function changes the current working directory for
 * the calling process.
 */
static int clean_temp_cwd()
{
	const char *temp_directory = get_temp_directory();
	const size_t length_temp_directory = strlen(temp_directory);
	char *prefix = NULL;
	int nb_errors = 0;
	DIR *dir = NULL;
	int status;

	prefix = talloc_size(NULL, length_temp_directory + 1);
	if (prefix == NULL) {
		note(NULL, WARNING, INTERNAL, "can't allocate memory");
		nb_errors++;
		goto end;
	}

	/* Sanity check: ensure the current directory lies in
	 * "/tmp".  */
	status = readlink("/proc/self/cwd", prefix, length_temp_directory);
	if (status < 0) {
		note(NULL, WARNING, SYSTEM, "can't readlink '/proc/self/cwd'");
		nb_errors++;
		goto end;
	}
	prefix[status] = '\0';

	if (strncmp(prefix, temp_directory, length_temp_directory) != 0) {
		note(NULL, ERROR, INTERNAL,
			"trying to remove a directory outside of '%s', "
			"please report this error.\n", temp_directory);
		nb_errors++;
		goto end;
	}

	dir = opendir(".");
	if (dir == NULL) {
		note(NULL, WARNING, SYSTEM, "can't open '.'");
		nb_errors++;
		goto end;
	}

	while (1) {
		struct dirent *entry;

		errno = 0;
		entry = readdir(dir);
		if (entry == NULL)
			break;

		if (   strcmp(entry->d_name, ".")  == 0
		    || strcmp(entry->d_name, "..") == 0)
			continue;

		status = chmod(entry->d_name, 0700);
		if (status < 0) {
			note(NULL, WARNING, SYSTEM, "cant chmod '%s'", entry->d_name);
			nb_errors++;
			continue;
		}

		if (get_dtype(entry) == DT_DIR) {
			status = chdir(entry->d_name);
			if (status < 0) {
				note(NULL, WARNING, SYSTEM, "can't chdir '%s'", entry->d_name);
				nb_errors++;
				continue;
			}

			/* Recurse.  */
			status = clean_temp_cwd();
			if (status < 0) {
				nb_errors = -1;
				goto end;
			}
			nb_errors += status;

			status = chdir("..");
			if (status < 0) {
				note(NULL, ERROR, SYSTEM, "can't chdir to '..'");
				nb_errors = -1;
				goto end;
			}

			status = rmdir(entry->d_name);
		}
		else {
			status = unlink(entry->d_name);
		}
		if (status < 0) {
			note(NULL, WARNING, SYSTEM, "can't remove '%s'", entry->d_name);
			nb_errors++;
			continue;
		}
	}
	if (errno != 0) {
		note(NULL, WARNING, SYSTEM, "can't readdir '.'");
		nb_errors++;
	}

end:
	TALLOC_FREE(prefix);

	if (dir != NULL)
		(void) closedir(dir);

	return nb_errors;
}

/**
 * Remove recursively @path.  This latter has to be a directory lying
 * in temp_directory (ie. "/tmp" on most systems).  This function
 * returns -1 on error, otherwise 0.
 */
static int remove_temp_directory2(const char *path)
{
	int result;
	int status;
	char *cwd;

	cwd = malloc(PATH_MAX);
	getcwd(cwd, PATH_MAX);

	status = chmod(path, 0700);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't chmod '%s'", path);
		result = -1;
		goto end;
	}

	status = chdir(path);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't chdir to '%s'", path);
		result = -1;
		goto end;
	}

	status = clean_temp_cwd();
	result = (status == 0 ? 0 : -1);

	/* Try to remove path even if something went wrong.  */
	status = chdir("..");
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't chdir to '..'");
		result = -1;
		goto end;
	}

	status = rmdir(path);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "cant remove '%s'", path);
		result = -1;
		goto end;
	}

end:
	if (cwd != NULL) {
		status = chdir(cwd);
		if (status < 0) {
			result = -1;
			note(NULL, ERROR, SYSTEM, "can't chdir to '%s'", cwd);
		}
		free(cwd);
	}

	return result;
}

/**
 * Like remove_temp_directory2() but always return 0.
 *
 * Note: this is a talloc destructor.
 */
static int remove_temp_directory(char *path)
{
	(void) remove_temp_directory2(path);
	return 0;
}

/**
 * Remove the file @path.  This function always returns 0.
 *
 * Note: this is a talloc destructor.
 */
static int remove_temp_file(char *path)
{
	int status;

	status = unlink(path);
	if (status < 0)
		note(NULL, ERROR, SYSTEM, "can't remove '%s'", path);

	return 0;
}

/**
 * Create a path name with the following format:
 * "/tmp/@prefix-$PID-XXXXXX".  The returned C string is either
 * auto-freed if @context is NULL.  This function returns NULL if an
 * error occurred.
 */
char *create_temp_name(TALLOC_CTX *context, const char *prefix)
{
	const char *temp_directory = get_temp_directory();
	char *name;

	if (context == NULL)
		context = talloc_autofree_context();

	name = talloc_asprintf(context, "%s/%s-%d-XXXXXX", temp_directory, prefix, getpid());
	if (name == NULL) {
		note(NULL, ERROR, INTERNAL, "can't allocate memory");
		return NULL;
	}

	return name;
}

/**
 * Create a directory that will be automatically removed either on
 * PRoot termination if @context is NULL, or once its path name
 * (attached to @context) is freed.  This function returns NULL on
 * error, otherwise the absolute path name to the created directory
 * (@prefix-ed).
 */
const char *create_temp_directory(TALLOC_CTX *context, const char *prefix)
{
	char *name;

	name = create_temp_name(context, prefix);
	if (name == NULL)
		return NULL;

	name = mkdtemp(name);
	if (name == NULL) {
		note(NULL, ERROR, SYSTEM, "can't create temporary directory");
		note(NULL, INFO, USER, "Please set PROOT_TMP_DIR env. variable "
			"to an alternate location (with write permission).");
		return NULL;
	}

	talloc_set_destructor(name, remove_temp_directory);

	return name;
}

/**
 * Create a file that will be automatically removed either on PRoot
 * termination if @context is NULL, or once its path name (attached to
 * @context) is freed.  This function returns NULL on error,
 * otherwise the absolute path name to the created file (@prefix-ed).
 */
const char *create_temp_file(TALLOC_CTX *context, const char *prefix)
{
	char *name;
	int fd;

	name = create_temp_name(context, prefix);
	if (name == NULL)
		return NULL;

	fd = mkstemp(name);
	if (fd < 0) {
		note(NULL, ERROR, SYSTEM, "can't create temporary file");
		note(NULL, INFO, USER, "Please set PROOT_TMP_DIR env. variable "
			"to an alternate location (with write permission).");
		return NULL;
	}
	close(fd);

	talloc_set_destructor(name, remove_temp_file);

	return name;
}
