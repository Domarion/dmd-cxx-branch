
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/link.c
 */

#include        <stdio.h>
#include        <ctype.h>
#include        <assert.h>
#include        <stdarg.h>
#include        <string.h>
#include        <stdlib.h>

#include        <sys/types.h>
#include        <sys/wait.h>
#include        <unistd.h>

#define HAS_POSIX_SPAWN 1
#include        <spawn.h>

#include        "root/file.hpp"
#include        "root/port.hpp"

#include        "mars.hpp"

#include        "root/rmem.hpp"

#include        "arraytypes.hpp"

int executecmd(const char *cmd, const char *args);
int executearg0(const char *cmd, const char *args);

/****************************************
 * Write filename to cmdbuf, quoting if necessary.
 */

void writeFilename(OutBuffer *buf, const char *filename, size_t len)
{
    /* Loop and see if we need to quote
     */
    for (size_t i = 0; i < len; i++)
    {   char c = filename[i];

        if (isalnum((utf8_t)c) || c == '_')
            continue;

        /* Need to quote
         */
        buf->writeByte('"');
        buf->write(filename, len);
        buf->writeByte('"');
        return;
    }

    /* No quoting necessary
     */
    buf->write(filename, len);
}

void writeFilename(OutBuffer *buf, const char *filename)
{
    writeFilename(buf, filename, strlen(filename));
}

/*****************************
 * As it forwards the linker error message to stderr, checks for the presence
 * of an error indicating lack of a main function (NME_ERR_MSG).
 *
 * Returns:
 *      1 if there is a no main error
 *     -1 if there is an IO error
 *      0 otherwise
 */
int findNoMainError(int fd)
{
    static const char nmeErrorMessage[] = "undefined reference to `_Dmain'";

    FILE *stream = fdopen(fd, "r");
    if (stream == nullptr) return -1;

    const size_t len = 64 * 1024 - 1;
    char buffer[len + 1]; // + '\0'
    size_t beg = 0, end = len;

    bool nmeFound = false;
    for (;;)
    {
        // read linker output
        const size_t n = fread(&buffer[beg], 1, len - beg, stream);
        if (beg + n < len && ferror(stream)) return -1;
        buffer[(end = beg + n) + 1] = '\0';

        // search error message, stop at last complete line
        const char *lastSep = strrchr(buffer, '\n');
        if (lastSep) buffer[(end = lastSep - &buffer[0])] = '\0';

        if (strstr(&buffer[0], nmeErrorMessage))
            nmeFound = true;

        if (lastSep) buffer[end++] = '\n';

        if (fwrite(&buffer[0], 1, end, stderr) < end) return -1;

        if (beg + n < len && feof(stream)) break;

        // copy over truncated last line
        memcpy(&buffer[0], &buffer[end], (beg = len - end));
    }
    return nmeFound ? 1 : 0;
}


/*****************************
 * Run the linker.  Return status of execution.
 */

int runLINK()
{
    pid_t childpid;
    int status;

    // Build argv[]
    Strings argv;

    const char *cc = getenv("CC");
    if (!cc)
        cc = "gcc";
    argv.push(cc);
    argv.insert(1, &global.params.objfiles);

    if (global.params.dll)
        argv.push("-shared");

    // None of that a.out stuff. Use explicit exe file name, or
    // generate one from name of first source file.
    argv.push("-o");
    if (global.params.exefile.length)
    {
        argv.push(global.params.exefile.ptr);
    }
    else if (global.params.run)
    {
        char name[L_tmpnam + 14 + 1];
        strcpy(name, P_tmpdir);
        strcat(name, "/dmd_runXXXXXX");
        int fd = mkstemp(name);
        if (fd == -1)
        {   error(Loc(), "error creating temporary file");
            return 1;
        }
        else
            close(fd);
        global.params.exefile = mem.xstrdup(name);
        argv.push(global.params.exefile.ptr);
    }
    else
    {   // Generate exe file name from first obj name
        const char *n = global.params.objfiles[0];
        char *ex;

        n = FileName::name(n);
        const char *e = FileName::ext(n);
        if (e)
        {
            e--;                        // back up over '.'
            ex = (char *)mem.xmalloc(e - n + 1);
            memcpy(ex, n, e - n);
            ex[e - n] = 0;
            // If generating dll then force dll extension
            if (global.params.dll)
                ex = const_cast<char *>(FileName::forceExt(ex, global.dll_ext.ptr));
        }
        else
            ex = const_cast<char *>("a.out");       // no extension, so give up
        argv.push(ex);
        global.params.exefile = ex;
    }

    // Make sure path to exe file exists
    ensurePathToNameExists(Loc(), global.params.exefile.ptr);

    if (global.params.symdebug)
        argv.push("-g");

    if (global.params.is64bit)
        argv.push("-m64");
    else
        argv.push("-m32");

    if (global.params.map || global.params.mapfile.length)
    {
        argv.push("-Xlinker");
        argv.push("-Map");
        if (!global.params.mapfile.length)
        {
            const char *fn = FileName::forceExt(global.params.exefile.ptr, "map");

            const char *path = FileName::path(global.params.exefile.ptr);
            const char *p;
            if (path[0] == '\0')
                p = FileName::combine(global.params.objdir.ptr, fn);
            else
                p = fn;

            global.params.mapfile = const_cast<char *>(p);
        }
        argv.push("-Xlinker");
        argv.push(global.params.mapfile.ptr);
    }

    for (size_t i = 0; i < global.params.linkswitches.length; i++)
    {   const char *p = global.params.linkswitches[i];
        if (!p || !p[0] || !(p[0] == '-' && (p[1] == 'l' || p[1] == 'L')))
        {
            // Don't need -Xlinker if switch starts with -l or -L.
            // Eliding -Xlinker is significant for -L since it allows our paths
            // to take precedence over gcc defaults.
            argv.push("-Xlinker");
        }
        argv.push(p);
    }

    /* Add each library, prefixing it with "-l".
     * The order of libraries passed is:
     *  1. any libraries passed with -L command line switch
     *  2. libraries specified on the command line
     *  3. libraries specified by pragma(lib), which were appended
     *     to global.params.libfiles.
     *  4. standard libraries.
     */
    for (size_t i = 0; i < global.params.libfiles.length; i++)
    {   const char *p = global.params.libfiles[i];
        size_t plen = strlen(p);
        if (plen > 2 && p[plen - 2] == '.' && p[plen -1] == 'a')
            argv.push(p);
        else
        {
            char *s = (char *)mem.xmalloc(plen + 3);
            s[0] = '-';
            s[1] = 'l';
            memcpy(s + 2, p, plen + 1);
            argv.push(s);
        }
    }

    for (size_t i = 0; i < global.params.dllfiles.length; i++)
    {
        const char *p = global.params.dllfiles[i];
        argv.push(p);
    }

    /* Standard libraries must go after user specified libraries
     * passed with -l.
     */
    const char *libname = (global.params.symdebug)
                                ? global.params.debuglibname.ptr
                                : global.params.defaultlibname.ptr;
    size_t slen = strlen(libname);
    if (slen)
    {
        char *buf = (char *)malloc(3 + slen + 1);
        strcpy(buf, "-l");
        /* Use "-l:libname.a" if the library name is complete
         */
        if (slen > 3 + 2 &&
            memcmp(libname, "lib", 3) == 0 &&
            (memcmp(libname + slen - 2, ".a", 2) == 0 ||
             memcmp(libname + slen - 3, ".so", 3) == 0)
           )
        {
            strcat(buf, ":");
        }
        strcat(buf, libname);
        argv.push(buf);             // turns into /usr/lib/libphobos2.a
    }

//    argv.push("-ldruntime");
    argv.push("-lpthread");
    argv.push("-lm");
    // Changes in ld for Ubuntu 11.10 require this to appear after phobos2
    argv.push("-lrt");
    // Link against libdl for phobos usage of dlopen
    argv.push("-ldl");

    if (global.params.verbose)
    {
        // Print it
        for (size_t i = 0; i < argv.length; i++)
            fprintf(global.stdmsg, "%s ", argv[i]);
        fprintf(global.stdmsg, "\n");
    }

    argv.push(nullptr);

    // set up pipes
    int fds[2];

    if (pipe(fds) == -1)
    {
        perror("unable to create pipe to linker");
        return -1;
    }

    childpid = fork();
    if (childpid == 0)
    {
        // pipe linker stderr to fds[0]
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);

        execvp(argv[0], const_cast<char **>(argv.tdata()));
        perror(argv[0]);           // failed to execute
        return -1;
    }
    else if (childpid == -1)
    {
        perror("unable to fork");
        return -1;
    }
    close(fds[1]);
    const int nme = findNoMainError(fds[0]);
    waitpid(childpid, &status, 0);

    if (WIFEXITED(status))
    {
        status = WEXITSTATUS(status);
        if (status)
        {
            if (nme == -1)
            {
                perror("error with the linker pipe");
                return -1;
            }
            else
            {
                printf("--- errorlevel %d\n", status);
                if (nme == 1) error(Loc(), "no main function specified");
            }
        }
    }
    else if (WIFSIGNALED(status))
    {
        printf("--- killed by signal %d\n", WTERMSIG(status));
        status = 1;
    }
    return status;
}

/**********************************
 * Delete generated EXE file.
 */

void deleteExeFile()
{
    if (global.params.exefile.length)
    {
        //printf("deleteExeFile() %s\n", global.params.exefile.ptr);
        remove(global.params.exefile.ptr);
    }
}

/***************************************
 * Run the compiled program.
 * Return exit status.
 */

int runProgram()
{
    //printf("runProgram()\n");
    if (global.params.verbose)
    {
        fprintf(global.stdmsg, "%s", global.params.exefile.ptr);
        for (size_t i = 0; i < global.params.runargs.length; ++i)
            fprintf(global.stdmsg, " %s", global.params.runargs[i]);
        fprintf(global.stdmsg, "\n");
    }

    // Build argv[]
    Strings argv;

    argv.push(global.params.exefile.ptr);
    for (size_t i = 0; i < global.params.runargs.length; ++i)
    {   const char *a = global.params.runargs[i];
        argv.push(a);
    }
    argv.push(nullptr);

    pid_t childpid;
    int status;

    childpid = fork();
    if (childpid == 0)
    {
        const char *fn = argv[0];
        if (!FileName::absolute(fn))
        {   // Make it "./fn"
            fn = FileName::combine(".", fn);
        }
        execv(fn, const_cast<char **>(argv.tdata()));
        perror(fn);             // failed to execute
        return -1;
    }

    waitpid(childpid, &status, 0);

    if (WIFEXITED(status))
    {
        status = WEXITSTATUS(status);
        //printf("--- errorlevel %d\n", status);
    }
    else if (WIFSIGNALED(status))
    {
        printf("--- killed by signal %d\n", WTERMSIG(status));
        status = 1;
    }
    return status;
}
