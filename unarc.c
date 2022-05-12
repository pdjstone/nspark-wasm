
/*
 * unarchive files
 *
 * Revision 1.21  99/03/17 MU
 * Added support for extraction with .inf files
 * Also capitilised the hexadecimal output strings.
 *
 * $Header: unarc.c 1.19 95/08/01 $
 * $Log:	unarc.c,v $
 * Revision 1.20  95/08/01  xx:xx:xx  BB
 * Fixed for Borland C/C++
 *
 * Revision 1.19  94/12/12  17:29:30  arb
 * Added support for ArcFS writesize
 *
 * Revision 1.18  93/08/20  12:30:02  arb
 * Added support for ArcFS archive detection
 *
 * Revision 1.17  93/08/20  11:54:55  arb
 * Prevent printing of spaces and LF if in quiet mode
 *
 * Revision 1.16  93/03/05  14:45:43  arb
 * Added <string.h> for RISCOS, needed for strcpy
 *
 * Revision 1.15  92/12/23  13:26:05  duplain
 * Added total-printing if in verbose mode.
 *
 * Revision 1.14  92/12/08  10:20:33  duplain
 * Added call to append_type() if apptype non zero.
 *
 * Revision 1.13  92/12/07  17:19:21  duplain
 * reformatted source.
 *
 * Revision 1.12  92/11/04  16:56:35  duplain
 * Added printing of CRC value if debugging.
 *
 * Revision 1.11  92/10/29  13:40:05  duplain
 * Fixed prompt_user function definition.
 *
 * Revision 1.10  92/10/23  14:31:28  duplain
 * Changed prompt_user() output text.
 *
 * Revision 1.9  92/10/06  12:17:49  duplain
 * Changed header used with verbose option.
 *
 * Revision 1.8  92/10/05  11:00:37  duplain
 * Recoded user-prompt when files already exists during unarchiving.
 *
 * Revision 1.7  92/10/02  17:41:14  duplain
 * Fix "end-of-subarchive" problem.
 *
 * Revision 1.6  92/10/02  10:03:20  duplain
 * Changed "OS_FILE" to "OS_File" for log file entry.
 *
 * Revision 1.5  92/10/01  13:39:30  duplain
 * Removed "goto do_retry" when header->complen > arcsize.
 *
 * Revision 1.4  92/10/01  12:17:16  duplain
 * Fixed calculation of archive file size when unpacking/testing.
 *
 * Revision 1.3  92/10/01  11:22:59  duplain
 * Added retry processing.
 *
 * Revision 1.2  92/09/30  10:27:51  duplain
 * Added logfile processing.
 *
 * Revision 1.1  92/09/29  18:02:27  duplain
 * Initial revision
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "spark.h"
#include "store.h"
#include "pack.h"
#include "compress.h"
#include "main.h"

/* BB changed next line because of conflict with Borland's io.h */

/* #include "io.h" */
#include "nsparkio.h"

/* BB added next line */
#if defined(__MSDOS__) || defined(_WIN32)
#include <io.h>					/* for read() */
#else
#ifndef RISCOS
#include <unistd.h>
#endif
#endif							/* __MSDOS__ */
#include <stdlib.h>
#include "misc.h"
#include "os.h"
#include "error.h"
#include "crc.h"
#include "arcfs.h"

char prompt_user(char *filename);
char *get_newname(void);


int
do_unsquash()
{
	SqshHeader sqsh_header;
	Header header;
	FILE *ifp = NULL, *ofp = NULL;
	Status r;
	long offset;
	long datastart;
	char *ofname = NULL;
	char *p;
	int result = 0;

	ifp = fopen(archive, "rb");
	if (!ifp)
	{
		error("Can not open %s for reading\n", archive);
		result = 1;
		goto error;
	}

	if (!to_stdout)
	{
		ofname = calloc(1, strlen(archive) + sizeof(",xxx"));
		if (!ofname)
		{
			error("Out of memory\n");
			result = 1;
			goto close_file;
		}
		/* Output filename is the same as the input filename with
		 * the correct filetype appended */
		strcpy(ofname, archive);
		/* Strip RISCOS filetype from output filename */
		p = ofname + (strlen(ofname) - 4);
		if (*p == ',' || *p == '.')
		{
			*p = '\0';
		}
	}

	r = read_sqsh_header(ifp, &sqsh_header);
	if (r != NOERR)
	{
		error("Invalid squash file");
		return 1;
	}

	memset(&header, 0, sizeof(header));
	datastart = ftell(ifp);
	fseek(ifp, 0, SEEK_END);
	offset = ftell(ifp);

	/* The uncompress code uses Spark entry header structures.
	 * Use the squash header and other info to set up the Spark header
	 * with needed info. */
	header.complen = (Word)(offset - datastart);
	sqsh_header_to_header(&sqsh_header, &header);
	fseek(ifp, datastart, SEEK_SET);

	crcsize = 0;
	writesize = header.origlen;

	if (!to_stdout)
	{
		/* Append the correct filetype to the output filename */
		append_type(&header, ofname);
	}

	if (!force && !to_stdout)
	{
		ofp = fopen(ofname, "rb");
		if (ofp)
		{
			char c;
			char *newname;

			fclose(ofp);
			c = prompt_user(ofname);
			switch (c)
			{
				case 'n':
					exit(1);
				case 'r':
					newname = get_newname();
					free(ofname);
					ofname = strdup(newname);
					break;
				case 'a':
				case 'y':
					/* overwrite file */
					break;
			}
		}
	}

	if (to_stdout)
	{
		ofp = stdout;
	}
	else
	{
		ofp = fopen(ofname, "wb");
		if (!ofp)
		{
			error("Can not open %s for writing\n", ofname);
			result = 1;
			goto close_file;
		}
	}

	r = uncompress(&header, ifp, ofp, UNIX_COMPRESS);
	if (r == NOERR)
	{
		if (stamp && !to_stdout)
		{
			filestamp(&header, ofname);
		}
	}
	else
	{
		error("Failed to decompress file");
		result = 1;
		goto close_file;
	}

close_file:
	fclose(ifp);
	if (!to_stdout && ofp)
	{
		fclose(ofp);
	}
error:

	return result;
}

int
do_unarc()
{
	FILE *ifp, *ofp = NULL, *lfp = NULL;
	Header *header = NULL;
	char *pathname = NULL;
	char fullname[PATHNAMELEN];
	int level = 0, ret = 0, retrying = 0;
	Word arcsize;
	Word nbytes = 0;
	int nfiles = 0;
	Status status;
	arcfs = 0; // reset to allow re-calling main() 

	ifp = fopen(archive, R_OPENMODE);
	if (!ifp)
	{
		ifp = fopen(name_dot_arc(archive), R_OPENMODE);
		if (!ifp)
		{
			if (!quiet)
				error("cannot open archive \"%s\"", archive);
			return (1);
		}
		archive = name_dot_arc(archive);
	}

	arcsize = filesize(archive);
	if (!arcsize && !quiet)
	{
		error("cannot get size of archive file");
		fclose(ifp);
		return (1);
	}

	/* MU changed to accomodate the -I option */
	if (!inffiles)
	{
		if (!testing && !listing && logfile)
		{
			lfp = fopen(logfile, W_OPENMODE);
			if (!lfp)
				warning("unable to open logfile \"%s\"", logfile);
		}
	}

	if (!quiet && verbose)
	{
		msg("filename                          size load/date   exec/time type storage\n");
		msg("--------                          ---- ----------- --------- ---- -------\n");
	}

	/*
	 * read compressed files from archive until end of file...
	 */
	while (1)
	{
		static Header dirheader;
		Byte comptype, first;

		header = NULL;
		if ((arcfs == 0) && ((first = read_byte(ifp)) != STARTBYTE))
		{
			if ((first == 'A') &&
				(read_byte(ifp) == 'r') &&
				(read_byte(ifp) == 'c') &&
				(read_byte(ifp) == 'h') &&
				(read_byte(ifp) == 'i') &&
				(read_byte(ifp) == 'v') &&
				(read_byte(ifp) == 'e') && (read_byte(ifp) == '\0'))
			{
				debug("ArcFS format archive\n");
				arcfs_init();
				arcfs = 1;
			}
			else
				break;
		}
	  retry_loop:
		header = read_header(ifp);
		comptype = header->comptype & 0x7f;
#ifdef DEBUGGING
		debug("archive file length = %ld  level = %d\n", arcsize,
			   level);
		print_header(header);
#endif							/* DEBUGGING */

		/*
		 * If this is a compress _file_ then check archive size against
		 * compress-file length...
		 */
		if (comptype && !(comptype == CT_NOTCOMP2 &&
						  /* BB changed constants in next line to long */
						  (header->load & 0xffffff00l) == 0xfffddc00l))
		{
			if (header->complen > arcsize)
			{
				debug("compressed len > archive file len");
				header = NULL;
				break;
			}
			else
				arcsize -= header->complen;
		}

		if (!comptype)
		{						/* end of archive ? */
			if (!level)
				break;
			level--;
			/*
			 * stamp directory now that all files have
			 * been written into it
			 */
			if (!testing && !listing && !to_stdout && stamp && inlist(pathname))
				if (filestamp(&dirheader, pathname) < 0 && !quiet)
					error("error stamping %s", pathname);
			pathname = uplevel();
			continue;
		}
		
		/*
		 * test for directory or file (file type = &DDC = archive)
		 */
		if (comptype == CT_NOTCOMP2 &&
			/* BB changed constants in next line to long */
			(header->load & 0xffffff00l) == 0xfffddc00l)
		{
			level++;
			pathname = downlevel(header->name);
			dirheader = *header;	/* make copy of header */

			/*
			 * create directory
			 */
			if (!testing && !listing && !to_stdout && inlist(pathname))
				switch (exist(pathname))
				{
				case NOEXIST:
					if (makedir(pathname) < 0 && !quiet)
					{
						msg("error making %s... aborting", pathname);
						ret = 4;
						break;
					}
					break;

				case ISFILE:
					if (!quiet)
						msg("%s exists as a file... aborting",
							   pathname);
					ret = 4;
					goto unarc_exit;
				case ISDIR:
				default:
					break;
				}
		}
		else
		{						/* file */
			if (pathname)
				sprintf(fullname, "%s%c%s", pathname, PATHSEP,
						header->name);
			else
				strcpy(fullname, header->name);

			if (!inlist(fullname))
			{
				fseek(ifp, (long) header->complen, 1);
				continue;
			}

			/*
			 * print the archive file details...
			 */
			if (!quiet)
			{
				msg("%-30s", fullname);
				if (verbose)
					print_details(header);
			}

			/* add to totals */
			nbytes += header->origlen;
			nfiles++;

			if (listing)
			{
				/* if listing, nothing more to do */
				if (!quiet)
					putc('\n', stderr);
				fseek(ifp, (long) header->complen, 1);
				continue;
			}
			else if (!quiet)
				putc(' ', stderr);

			/*
			 * append the filetype to the name
			 */
			if (apptype && !to_stdout)
				append_type(header, fullname);

			/*
			 * if actually unarchiving then check if the file already exists
			 */
			if (!testing && !to_stdout)
			{
			  test_exist:
				switch (exist(fullname))
				{
				case ISFILE:
					if (!force)
					{
						char c = prompt_user(fullname);
						if (c == 'a')
							force = 1;
						else if (c == 'n')
						{
							fseek(ifp, (long) header->complen, 1);
							continue;
						}
						else if (c == 'r')
						{
							char *newname = get_newname();
							if (pathname)
								sprintf(fullname, "%s%c%s", pathname,
										PATHSEP, newname);
							else
								strcpy(fullname, newname);
							goto test_exist;
						}
						/* if (c == 'y') FALLTHROUGH */
					}
					break;
				case ISDIR:
					if (!quiet)
						msg("exists as a directory... skipping");
					continue;
				case NOEXIST:
				default:
					break;
				}
				ofp = fopen(fullname, W_OPENMODE);
				if (!ofp)
				{
					if (!quiet)
						msg("unable to create");
					continue;
				}
			}
			else if (!testing)
			{
				ofp = stdout;
			}

			/*
			 * do the unpacking
			 */
			crcsize = writesize = header->origlen;
			switch (comptype)
			{
			case CT_NOTCOMP:
			case CT_NOTCOMP2:
				status = unstore(header, ifp, ofp);
				break;
			case CT_CRUNCH:
				status = uncompress(header, ifp, ofp, CRUNCH);
				break;
			case CT_PACK:
				status = unpack(header, ifp, ofp);
				break;
			case CT_SQUASH:
				status = uncompress(header, ifp, ofp, SQUASH);
				break;
			case CT_COMP:
				status = uncompress(header, ifp, ofp, COMPRESS);
				break;
			default:
				error("unsupported archive type %d", comptype);
				if (retrying)
					goto do_retry;
				fseek(ifp, (long) header->complen, 1);
				continue;
			}

			if (!testing && !to_stdout && ofp)
			{
				fclose(ofp);
				ofp = NULL;
			}

			/*
			 * check if unarchiving failed.
			 * (RERR check is not in switch() because `break'
			 * needs to escape from while())
			 */
			if (status == RERR)
			{
				if (!quiet)
					error("error reading archive");
				ret = 3;
				break;
			}
			switch (status)
			{
			case WERR:
				if (!quiet)
					msg("error writing file");
				break;
			case CRCERR:
				if (!quiet)
					msg("failed CRC check");

				/* BB changed format in next line to long hex */
				/* debug("  calculated CRC=0x%x", crc); */
#ifdef __MSDOS__
				debug("  calculated CRC=0X%lX", crc);
#else
				debug("  calculated CRC=0X%X", crc);
#endif							/* __MSDOS__ */
				break;
			case NOERR:
				if (!testing && !to_stdout && stamp)
				{
					if (filestamp(header, fullname) < 0 && !quiet)
						msg("\nerror stamping %s", fullname);
				}
				/* 
				 * XXX: if the filename has had it's filetype appended to
				 * it, it may be longer than 10 characters long making this
				 * file useless.  We could truncate the filename but there is
				 * no need to under UNIX... bit of a mismatch!
				 */
				if (!testing)
				{
					if (inffiles)
					{
						strcpy(logfile, fullname);
						strcat(logfile, ".inf");
						lfp = fopen(logfile, W_OPENMODE);

						if (lfp)
						{

/*
					// BASIC 
					if ((header->load => 0xFFFFFB41) && (header->load <= 0xFFFFFB46))
					{
						header->load = 0xFFFF1900;
						header->exec = 0xFFFF802B;
					}

					// *EXEC ie !BOOT 
					if (header->load == 0xFFFFFE41)
					{
						header->load = 0x00000000;
						header->exec = 0xFFFFFFFF;
					}
*/
							fprintf(lfp, "%s %08lX %08lX\n",
									riscos_path(fullname), (long)header->load,
									(long)header->exec);
							fclose(lfp);

						}

					}
					else
					{
						if (lfp)
						{
							fprintf(lfp,
									"SYS \"OS_File\", 1, \"%s\", &%08lX, &%08lX,, &%X\n",
									riscos_path(fullname), (long)header->load,
									(long)header->exec, header->attr);
						}
					}

				}
				break;
			default:
				break;
			}

			if (!quiet)
				putc('\n', stderr);
			if (ret)
				break;
		}
	}

	/*
	 * find out why header wasn't found
	 */
	if (!header)
		switch (check_stream(ifp))
		{
		case FNOERR:
			if (!quiet)
				msg("bad archive header");
			if (retry && !listing)
			{
				msg("... retrying");
			  do_retry:
				retrying++;
				while (check_stream(ifp) == FNOERR)
					if (read_byte(ifp) == STARTBYTE)
					{
						Byte byte = read_byte(ifp);
						switch (byte & 0x7f)
						{
						case (CT_NOTCOMP):
						case (CT_NOTCOMP2):
						case (CT_CRUNCH):
						case (CT_PACK):
						case (CT_SQUASH):
						case (CT_COMP):
							ungetc((int) byte, ifp);
							goto retry_loop;
							/* NOTREACHED */
						default:
							break;
						}
					}
			}
			else
			{
				retrying = 0;
				if (!quiet)
					putc('\n', stderr);
			}
			ret = 2;
			break;
		case FRWERR:
			if (!quiet)
				msg("error reading archive");
			ret = 3;
			break;
		case FEND:
		default:
			break;
		}

  unarc_exit:

	if (verbose)
		msg("total of %ld bytes in %d files\n", (long)nbytes, nfiles);

	if (ofp && !to_stdout)
		fclose(ofp);
	if (lfp)
		fclose(lfp);
	if (ifp)
		fclose(ifp);
	return (ret);
}

/*
 * the file being extracted already exists, so ask user what to do...
 */
char
prompt_user(char *filename)
{
	int c;
	char buffer[80];

	while (1)
	{
		fprintf(stderr, "\n\"%s\" exists, overwrite ? (Yes/No/All/Rename): ",
			   filename);
		fflush(stderr);
		if (read(0, buffer, sizeof(buffer) - 1)<1) {
			c = 'n';
			break;
		}
		if (isupper(*buffer))
			c = tolower(*buffer);
		else
			c = *buffer;
		if (c == 'y' || c == 'n' || c == 'a' || c == 'r')
			break;
	}
	return (c);
}

/*
 * user wants to rename file, so get the leaf name of the new file...
 */
char *
get_newname(void)
{
	int c;
	static char buffer[80];

	while (1)
	{
		fprintf(stderr, "enter new filename: ");
		fflush(stderr);
		c = read(0, buffer, sizeof(buffer) - 1);
		buffer[c ? c - 1 : 0] = '\0';
		if (!*buffer)
			continue;
		for (c = 0; buffer[c]; c++)
			if (buffer[c] == PATHSEP)
			{
				msg("*** new file must extract into this directory ***");
				continue;
			}
		return (buffer);
	}
}
