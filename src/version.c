/* vi:ts=4:sw=4:tw=78:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*

 Started with Stevie version 3.6 (Fish disk 217) - GRWalter (Fred)

 VIM 1.0	- Changed so many things that I felt that a new name was required
			(I didn't like the name Stevie that much: I don't have an ST).
			- VIM stands for "Vi IMitation".
			- New storage structure, MULTI-LEVEL undo and redo,
			improved screen output, removed an awful number of bugs,
			removed fixed size buffers, added counts to a lot of commands,
			added new commands, added new options, added 'smart indent',
			added recording mode, added script files, moved help to a file,
			etc. etc. etc.
			- Compiles under Manx/Aztec C 5.0. You can use "rez" to make VIM
			resident.
			- Bram Moolenaar (Mool)

 VIM 1.09 - spaces can be used in tags file instead of tabs (compatible with
			Manx ctags).

 VIM 1.10 - Csh not required anymore for CTRL-D. Search options /e and /s added.
			Shell option implemented. BS in replace mode does not delete
			character. Backspace, wrapmargin and tags options added.
			Added support for Manx's QuickFix mode (just like "Z").
			The ENV: environment variables instead of the Old Manx environment
			variables are now used, because Vim was compiled with version 5.0d
			of the compiler. "mool" library not used anymore. Added index to
			help screens.

 VIM 1.11 - removed bug that caused :e of same file, but with name in upper
			case, to re-edit that file.

 VIM 1.12 - The second character of several commands (e.g. 'r', 't', 'm') not
			:mapped anymore (UNIX vi does it like this, don't know why); Some
			operators did not work when doing a 'l' on the last character in
			a line (e.g. 'yl'); Added :mapping when executing registers;
			Removed vi incompatibility from 't' and 'T' commands; :mapping! also
			works for command line editing; Changed a few details to have Vim
			run the macros for solving a maze and Towers of Hanoi! It now also
			runs the Turing machine macros!

 VIM 1.13 - Removed a bug for !! on empty line. "$" no longer puts cursor at
			the end of the line when combined with an operator. Added
			automatic creation of a script file for recovery after a crash.
			Added "-r" option. Solved bug for not detecting end of script file.
			".bak" is now appended, thus "main.c" and "main.h" will have
			separate backup files.

 VIM 1.14 - Removed a few minor bugs. Added "-n" option to skip autoscript.
			Made options more Vi compatible. Improved ^C handling. On serious
			errors typahead and scripts are discarded. 'U' is now correctly
			undone with 'u'. Fixed showmatch() handling of 'x' and '\x'.
			Solved window size dependency for scripts by adding ":winsize"
			commands to scripts. This version released on Fish disk 591.

 VIM 1.15 - No extra return in recording mode (MCHAR instead of MLINE buffer).
			plural() argument is now a long. Search patterns shared between
			:g, :s and /. After recovery a message is given. Overflow of mapbuf
			is detected. Line number possible with :read. Error message when
			characters follow a '$' in a search pattern. Cause for crash
			removed: ":s/pat/repl/g" allocated not enough memory. Option
			"directory" added. Option "expandtab" added. Solved showmode non-
			functioning. Solved bug with window resizing. Removed some *NULL
			references. CTRL-], * and # commands now skips non-identifier
			characters. Added tag list, CTRL-T, :pop and :tags commands.
			Added jump list, CTRL-O and CTRL-I commands. Added "shiftround"
			option. Applied AUX and Lattice mods from Juergen Weigert.
			Finally made linenr_t a long, files can be > 65000 lines!
			:win command could be written to script file halfway a command.
			Option shelltype added. With ^V no mapping anymore.
			Added :move, :copy, :t, :mark and :k. Improved Ex address parsing.
			Many delimiters possible with :s.

 VIM 1.16 - Solved bug with zero line number in Ex range. Added file-number to
			jump list. Solved bug when scrolling downwards. Made tagstack vi
			compatible. Cmdline editing: CTRL-U instead of '@'. Made Vim DICE
			compatible. Included regexp improvements from Olaf Seibert,
			mapstring() removed. Removed bug with CTRL-U in insert mode.
			Count allowed before ". Added uppercase (file) marks. Added
			:marks command. Added joinspaces option. Added :jumps command. Made
			jumplist compatible with tag list. Added count to :next and :Next.

 VIM 1.17 - Removed '"' for Ex register name. Repaired stupid bug in tag code.
			Now compiled with Aztec 5.2a. Changed Arpbase.h for use with 2.04
			includes. Added repdel option. Improved :set listing. Added count
			to 'u' and CTRL-R commands. "vi:" and "ex:" in modelines must now
			be preceded with a blank. Option "+command" for command line and
		    :edit command added.

 VIM 1.18 - Screen was not updated when all lines deleted. Readfile() now
			puts cursor on first new line. Catch strange disk label.
			Endless "undo line missing" loop removed. With empty file 'O' would
			cause this. Added window size reset in windexit(). Flush .vim file
			only when buffer has been changed. Added the nice things from
			Elvis 1.5: Added "equalprg" and "ruler" option. Added quoting.
			Added third meaning to 'backspace' option: backspacing over start
			of insert. Added "-c {command}" command line option. Made generating
			of prototypes automatically. Added insert mode command CTRL-O and
			arrow keys. CTRL-T/CTRL-D now always insert/delete indent. When
			starting an edit on specified lnum there was redraw of first lines.
			Added 'inputmode' option. Added CTRL-A and CTRL-S commands. '`' is
			now exclusive (as it should be). Added digraphs as an option.
			Changed implementation of parameters. Added :wnext command.
			Added ':@r' command. Changed handling of CTRL-V in command line.
			Block macros now work. Added keyword lookup command 'K'. Added
			CTRL-N and CTRL-P to command line editing. For DOS 2.0x the Flush
			function is used for the autoscript file; this should solve the
			lockup bug. Added wait_return to msg() for long messages.

 VIM 1.19 - Changes from Juergen Weigert:
			Terminal type no longer restricted to machine console. New
			option -T terminal. New set option "term". Builtin termcap 
			entries for "amiga", "ansi", "atari", "nansi", "pcterm". 
			Ported to MSDOS. New set option "textmode" ("tx") to control 
			CR-LF translation. CTRL-U and CTRL-D scroll half a screen full,
			rather than 12 lines. New set option "writebackup" ("wb") to 
			disable even the 'backup when writing' feature.
			Ported to SunOS. Full termcap support. Does resize on SIGWINCH.

			Made storage.c portable. Added reading of ".vimrc". Added
			'helpfile' option. With quoting the first char of an empty line
			is inverted. Optimized screen updating a bit. Startup code 
			looks for VIMINIT variable and .vimrc file first. Added option
			helpfile. Solved bug of inserting deletes: redefined ISSPECIAL.
			Changed inchar() to use key codes from termcap. Added parameters
			for terminal codes. Replaced aux device handling by amiga window
			handling. Added optional termcap code. Added 'V', format
			operator.

 VIM 1.20 - wait_return only ignores CR, LF and space. 'V' also works for
            single line. No redrawing while formatting text. Added CTRL-Z.
			Added usage of termcap "ks" and "ke". Fixed showmatch().
			Added timeout option. Added newfile argument to readfile().

 VIM 1.21 - Added block mode. Added 'o' command for quoting. Added :set inv.
			Added pos2ptr(). Added repeating and '$' to Quoting.

 VIM 1.22 - Fixed a bug in doput() with count > 1.
			Port to linux by Juergen Weigert included.
			More unix semantics in writeit(), forceit flag ignores errors while 
			preparing backup file. For UNIX, backup is now copied, not moved.
			When the current directory is not writable, vim now tries a backup
			in the directory given with the backupdir option. For UNIX, raw mode
			has now ICRNL turned off, that allowes ^V^M. Makefiles for BSD,
			SYSV, and linux unified in makefile.unix. For MSDOS
			mch_get_winsize() implemented. Reimplemented builtin termcaps in
			term.c and term.h. set_term() now handles all cases. Even builtins
			when TERMCAP is defined. Show "..." while doing filename completion.

 VIM 1.23 -	Improved MSDOS version: Added function and cursor keys to builtin 
			pcterm. Replaced setmode by settmode, delay by vim_delay and 
			delline by dellines to avoid name conflicts. Made F1 help key.
			Renamed makecmdtab to mkcmdtab and cmdsearch to csearch for 
			8 char name limit. Wildcard expansion adds *.* to names without a 
			dot. Added shell execution.
			For unix: writeit() overwrites readonly files when forced write,
			more safety checks. Termcap buffer for linux now 2048 bytes.
			Expandone() no longer appends "*" to file name. Added "graphic"
			option. Added ':' command to quoting.
			
 VIM 1.24	Adjusted number of spaces inserted by dojoin(). MSDOS version uses 
			searchpath() to find helpfile. Fixed a few small problems. Fixed 
			nasty bug in getperm() for SAS 6.0. Removed second argument from 
			wait_return(). Script files accessed in binary mode with MSDOS. 
			Added 'u' and 'U' commands to quoting (make upper or lower case). 
			Added "CTRL-V [0-9]*" to enter any byte value. Fixed doput().
			Dodis() displays register 0. Added CTRL-B to insert mode. Attempt 
			to fix the lockup bug by adding Delay() to startscript(). -v 
			option now implies -n option. doformat() added to improve 'V' 
			command. Replace bool_t with int. Fixed handling of \& and ~ in
			regsub(). Added interrupt handling in msdos.c for ctrl-break and
			critical errors. Added scrolljump option. Added :stop. Added -d
			argument. Fixed bug in quickfix startup from cli. Fixed enforcer
			hit with aux:. Added CTRL-C handling to unix.c. Fixed "O<BS><CR>" 
			bug with autoindent. Worked around :cq not working by adding a 
			printf()!? Added default mapping for MSDOS PageUp etc. Fixed 
			cursor position after 'Y'. Added shift-cursor commands. Changed 
			ExpandFile() to keep names with errors. Added CLEAR and CURSUPD 
			arguments to updateScreen(). Fixed CTRL-@ after a change command.
			modname() changes '.' into '_'. Added emptyrows to screen.c. 
			Fixed redo of search with offset. Added count to 'z' command. 
			Made :so! work with :global. Added writing of cursor postition to 
			startscript(). Minimized terminal requirements. Fixed problem 
			with line in tags file with mixed spaces and tabs. Fixed problem 
			with pattern "\\" in :s and :g. This version posted on Usenet.

 VIM 1.25	Improved error messages for :set. Open helpfile in binary mode 
			for MSDOS. Fixed ignorecase for Unix in cstrncmp(). Fixed read 
			from NULL with :tags after vim -t. Repaired 'z' command. Changed 
			outnum() for >32767. In msdos.c flushbuf did write(1, .. instead 
			of write(0, .. Added secure to fix security. Fixed pointer 
			use after free() bug in regsub() (made :s fail under MSDOS). 
			Added nofreeNULL(), needed for some UNIXes. Improved window 
			resizing for Unix. Fixed messages for report == 0. Added 
			bsdmemset(). Changed a few small things for portability. Added 
			:list. Made '0' and '^' exclusive. Fixed regexp for /pattern* 
			(did /(pattern)* instead of /pattern(n)*). Added "']" and "'[". 
			Changed Delay(2L) into Delay(10L). Made 'timeout' option 
			vi-compatible, added 'ttimeout' option. Changed TIOCSETP to 
			TIOCSETN in unix.c. Added "ti" and "te" termcap entries, makes 
			sun cmdtool work. Added stop- and starttermcap(). Use cooked 
			output for listings on Amiga only. Added "starting" flag, no ~s 
			anymore with every startup. Modname made portable; Added 
			'shortname' option, Fixed problems with .vim file on messydos. 
			Global .exrc/.vimrc for Unix added. Added patches for SCO Xenix. 
			Add :w argument to list of alternate file names. Applied a few 
			changes for HPUX. Added Flock in writeit() for safety. Command 
			":'a,'bm." moved to 'b instead of current line. Argument in 
			'shell' option allowed. Re-implemented :copy and :move. Fixed 
			BS-CR-BS on empty line bug in edit.c. -t option was ignored if 
			there is a file ".vim". Changed amiga.c to work without 
			arp.library for dos 2.0. Fixed "\$" and "\^" in regexp. Fixed 
			pipe in filter command. Fixed CTRL-U and CTRL-D. With '}' indent 
			in front of the cursor is included in the operated text. Fixed 
			tag with '[' in search pattern. Added CTRL-V to 'r'. Fixed "tc" 
			entry in termlib.c. term_console now default off. Added :noremap 
			and ^V in :map argument. Replaced CTRL by Ctrl because some 
			unixes have this already. Fixed "Empty file" message disappearing 
			when there is no .exrc file. Added CTRL-K for entering digraphs. 
			Removed escape codes from vim.hlp, added handling of inversion to 
			help().

 VIM 1.26	For Unix: Removed global .exrc; renamed global .vimrc to vimrc.
 			Moved names of *rc and help files to makefile. Added various 
			little changes for different kinds of Unix. Changed CR-LF 
			handling in dosource() for MSDOS. Added :mkvimrc. Fixed 
			WildExpand in unix.c for empty file. Fixed incompatibility with 
			msdos share program (removed setperm(fname, 0) from fileio.c).
			Added ":set compatible". Fixed 'history=0'.

 VIM 1.27	Added USE_LOCALE. Changed swapchar() to use toupper() and 
			tolower(). Changed init order: .vimrc before EXINIT. Half-fixed 
			lines that do not fit on screen. A few minor bug fixes. Fixed 
			typehead bug in Read() in unix.c. Added :number. Reset IXON flag 
			in unix.c for CTRL-Q. In tags file any Ex command can be used. Ex 
			search command accepts same flags as normal search command. Fixed 
			'?' in tag search pattern. 'New file' message was wrong when 'bk' 
			and 'wb' options were both off.

 Vim 1.29 to 1.31 and Vim 2.0	See ../CHANGES2.0.

 Vim 2.0 (released 1993 December 14)
 			When reading and writing files and in some other cases use short
 			filename if ":cd" not used. Fixes problem with networks. Deleted
			"#include <ctype.h>" from regexp.c. ":v" without argument was not
			handled correctly in doglob(). Check for tail recursion removed
			again, because it forbids ":map! foo ^]foo", which is OK. Removed
			redraw on exit for msdos. Fixed return value for FullName in
			unix.c. Call_shell does not always use cooked mode, fixes problem
			with typing CR while doing filename completion in unix. "r<TAB>"
			now done by edit() to make expandtab works. Implemented FullName
			for msdos. Implemented the drive specifier for the :cd command for
			MSDOS. Added CTRL-B and CTRL-E to command line editing. Del key
			for msdos not mapped to "x" in command mode, could not delete last
			char of count. Fixed screen being messed up with long commands
			when 'sc' is set. Fixed use of CR-LF in tags file. Added check
			for abbreviation when typing ESC or CTRL-O in insert mode. Doing
			a ":w file" does overwrite when "file" is the current file. Unmap
			will check for 'to' string if there is no match with 'from'
			string; Fixes ":unab foo" after ":ab foo bar". Fixed problem in
			addstar() for msdos: Check for negative index. Added possibility
			to switch off undo ":set ul=-1". Allow parameters to be set to
			numbers >32000 for machines with 16 bit ints.

 Vim 2.1 to 3.0: see ../CHANGES3.0 (released 1994 August 12)

 Vim 3.1 (finished 1995 January 16)

Fixed :s/\(.*\)/\1/ , was replacing CR with line break.

Doing CTRL-@ when there is no inserted text yet quits insert mode.

"r" sets the last inserted text.

Added mkproto to makefile.unix. You need a special version of mkproto.

If file system full and write to swap file failed, was getting error message
for lnum > line_count (with ":preserve").

Changed method to save characters for BS in replace mode. Now works correctly
also when 'et' set and entering a TAB and replacing with CR several times.

Removed reverse replace mode. It is too complicated to do right and nobody
will probably use it anyway. Reverse insert is still possible.

Made keyword completion work in replace mode.

When writing part of the buffer to the current file ! is required.

When CTRL-T fails (e.g. when buffer was changed) don't change position
in tag stack.

The 'Q' operator no longer affects empty lines. (Webb)

Added CTRL-W CTRL-T (got to top window) and CTRL-W CTRL-B (go to bottom
window)

Fixed cursor not visible when doing CTRL-Z for unix (White).

When in insert mode and wrapping from column one to the last character, don't
stick at the end but in the column (Demirel).

The command "aY did not put anything in register a if 'ye' was set. (Demirel)

Inserting a tab with 'et' set did not work correctly when there was a real tab
in front of it (Brown).

Added support for static tags "file:tag ..." (Weigert).

Added 'startofline' option (Waggoner/Webb).

Restore 'wrapmargin' when 'paste' is reset (Colon).

Recognize a terminal name as xterm when it starts with "xterm". Also catch
"xterms" (Riehm).

Added special keys page-up, page-down, end and home (Cornelius).

Fixed '^' appearing in first window when CTRL-V entered in second window.

Added ":stag", same as ":tag", but also split window.

Fixed core dump when using CTRL-W ] twice (tag stack was invalid) (Webb).

Added patches for Coherent from Fred Smith (fredex@fcshome.stoneham.ma.us).

Give error message when doing "Vim -r" without a file name (used to crash).

Changed error message when doing ":n" while editing last file in file list.

The commands ":print", ":number" and ":list" did not leave the cursor on the
last line.

Added completion of old setting in command line editing (Webb).

Fixed using count to reselect visual area when area was one line (Webb).

Fixed setting curswant properly after visual selection (Webb).

Fixed problem that column number was ridiculous when using V with : (Webb).

Added column number to ":marks" command (Demirel).

Removed 'yankendofline' option, you can just use ":map Y y$" (Demirel).

Added highlighting for ":number" command (Webb).

When ex command is given that is not implemented in Vim give better error
message (Webb).

After truncating an autoindent leave curswant after the indent (Webb).

Put cursor on first non-white after a few ex commands and after "2>>" (Webb).

When deleting/inserting lines also adjust the marks in the tag stack (Webb).

Added ":retab" command (Webb).

Made file message a bit shorter and added 'shortmess' option (Webb).

Fixed ":n #", put the cursor on the right line like ":e #" (Webb).

Recompute column for shown command when rearranging windows (Webb).

Added "[P" and "]P" as synonym for "[p". These commands are now redoable.
Fixed cursor positioning and characterwise text (Webb).

When a command line is already in the history, the old entry is removed. When
searching through the command history, the entry where started from is
remembered. (Webb).

Renamed showmatch() to findmatch() (it didn't show anything).

Fixed paging for showing matches on command line. This can also be interrupted
(Webb).

Fixed commandline completion when in the range. Fixed ":tag *s^D". Added
support for completion of static tags (from elvis's ctags program). (Webb).

Check screen size after Vim has been suspended (Webb).

Fixed bug where } would only be smart-indented to line up with the line
containing the { when there was some spacing before the }. (Webb).

For setting numeric options hex and octal can be used (Webb).

Added ":ls", synonym for ":files" (Webb).

Added "[g", "[d", ":checkpath" and friends, find identifier or macro in
included files (Webb).

Added tags to documentation. Made "doctags" program to produce the tags file.

Added completion commands for insert mode and CTRL-X sub-mode (Webb).

In a search pattern '*' is not magic when used as the first character. Fixed
'^' recognized as start of line in "/[ ^I]^". (Webb)

When jumping to file under cursor, give proper error message instead of beep.
After using 'path' option, also look relative to the current file. (Webb).

"%" now works to match comments of the form "/ * Comment / * * /". (Webb)

Show meta keys in mappings as M-x. Use highlight option '8' for higlighting
the meta keys. Special keys are displayed with their name, e.g. <C_UP>. When
using CTRL-V special-key in a mapping, this is replaced by the internal key
code. Can use "#C_UP" for cursor-up key in mapping. (Webb).

Added the VIMINFO stuff (Webb).

Added 'infercase' option (Webb).

After undo, put cursor on first non-blank instead of in column 0 (Webb).

Accept CTRL-E and CTRL-Y when waiting for confirmation to replace (Webb).

Allow tags to start with a number (Webb).

Added options "flash" and "novice", they are not used (Demirel).

When searching for a tag, after files 'tags' option failed, also try the tags
file in the same directory as the current file.

Added the automatic commands: execute commands after starting to edit a file 
(Webb).

When moving the cursor to the end of the line in insert mode with CTRL-O $,
put it one beyond the line.

Added automatic formatting of comments, the 'comments', 'nestedcomments' and
'formatoptions' options (Webb).

When creating a new buffer, set 'readonly' to false by default. Fixes getting
an emtpy readonly buffer after ":new" in a readonly buffer.

Give error message that includes the commandline for ":" commands that are not
typed by the user.

Fixed cursor positioned at '@' of not fitting line after doing the auto
commands.

Removed extra redraw when doing CTRL-] caused by executing autocommands.

Added "[n" (show next occurence of pattern under the cursor), "[N" (show all
occurences of pattern under the cursor after current line), and "]n" (jump to
next occurence of pattern under the cursor)

Changed "--more--" prompt to be more informative.

Added check on close() for writing the .bak file.

Adjusted formatting of comments for use of replace stack.

Changed 'comments and 'nestedcomments' to be comma separated, so they can
include trailing spaces. Removed 's' from  'formatoptions. Allows for "/""*"
to be recognized but not "*".

Init terminal to CBREAK instead of RAW in unix.c. (Weigert)

 Vim 3.2 (finished 1995 January 24)

After 'Q' put cursor at first non-blank of the last formatted line.

Use standard cproto instead of a home brew version of mkproto.

Updated automatic formatting of comments to add a space where neccesary; fixed
problem with indent after comment leader.

Changed CBREAK back into RAW, CTRL-C exited the program.

Made 'formatoptions', 'comments' and 'nestedcomments' options local to buffer.

 Vim 3.3 (finished 1995 February 10)

Don't map the key for the y/n question for the :s///c command.

Fixed problem when auto-formatting with space after the cursor.

Fixed problem with CTRL-O . in insert mode when repeated command also involves
insert mode.

Fixed "line count wrong" error with undo that deletes the first line.

After undo "''" puts the cursor back to where it was before the undo.

Allow "map a ab", head recursive mapping (just like vi).

Don't truncate line when doing ESC after CR when entering a comment.

Improved ":center", ":right" and ":left"; blank lines are no longer affected,
tabs are taken into account.

Fixed premsg() to show command when first key of an operator was not typed
(e.g. from a wait-for-return), and have to wait for the second key to be
typed.

Fixed outputting meta characters when switching highlighting on/off.

Added '-' register for deletes of less than one line.

Added argument file-number to getfile() and doecmd() to be able to edit a
specific buffer. Fixes problem with CTRL-^ to buffer without a file name.

When :bdel and :bunload are used to remove buffers that have active windows,
those windows are closed instead of giving an error message. Don't give an
error message when some, but not all, of the buffers do not exist.

Fixed an occasional core dump when using ^P or ^N in insert mode under certain
conditions (Webb).

Fixed bug where inserting a new-line before a line starting with 'if' etc.
would cause a smart-indent because of that 'if'.  This also fixes a small
problem with comment formatting: consider the line "/""* text", and replace
the space with a new-line.  It should insert the comment leader " *", but it
did not before. (Webb)

Fixed bug where nowrap is set and doing 'j' or 'k' caused a sideways scroll
with the cursor still in the middle of the screen.  Happened when moving from
a line with few tabs to a line with many tabs. (Webb)

Accept an end-of-line in the same way a blank is accepted for the 'comments'
option.

With ":file name" command, update status lines for new file name.

When file name changed, also change name of swapfile.

Fixed not checking for function arguments for gcc. Fixed three mistakes.

The Amiga trick for MSDOS compatible filesystems is now also done for UNIX.
Fixes problems with wrong swap file name on MSDOS partition for FreeBSD and
Linux.

Fixed bug: In message.c removed adding of K_OFF to K_MAXKEY.

Default options for backup file is now 'backup' off and 'writebackup' on.

Removed flush_buffers() from beep(), made beep_flush() that includes it.

An ESC in normal mode does not flush the map buffer, only beeps. Makes
	":map g axx^[^[ayy^[" work.

When a ".swp" file already exists, but the file name in it is different, don't
give the "swap file exists" error message. Helpful when putting all swap files
in one directory.

In an Ex address the '+' character is not required before a number, ".2d" is
the same as ".+2d", "1copy 2 3 4" is the same as "1copy 2+3+4".

"O" in an empty buffer now inserts a new line as it should. ":0r file" in an
empty file appends an empty line after the file, just like vi.

Fixed "more" for :global command.

Don't accept ":g", global command without any argument.

Separated history for command line and search strings.

Only typed command lines and search strings are put in the history.

 Vim 3.4 (finished 1995 March 9)

Improved the handling of empty lines for "[p" and the like.

After using ":set invlist" cursor would not stick to the column.

Removed #ifdef VIMINFO from around init_history(). Would not compile if
VIMINFO is not defined.

Fixed problem of trailing 'q' with executing recorded buffer when 'sc' set.

Changed the displaying of message for many commands. Makes ":1p|2p" work.
Avoids having to type some extra returns.

Added error message for using '!' in the command line where it is not allowed.

When recovering "Original file may have been changed" message was overwritten.
Also changed it into an error message and don't wait for return after "using
swap file" message.

Added wait_return() when filtering finds an error when executing the shell
command.

Remove white space after comment leader when hitting ESC.

Added 'a' reply to substitute with confirmation, is like 'y' for all remaining
replacements.

For xterm: Added cursor positioning by mouse control, added 'mouse' option,
added visual selection with mouse, added window selection with mouse.

Fixed missing index to viminfo_hisidx in cmdline.c.

Recognize <C_UP> anywhere in the lhs of a mapping instead of #C_UP at the
beginning (Webb).

Included some initializations for GCC (Webb).

Fixed CTRL-Z not working for Appolos (Webb).

Fixed ignoring CTRL-M at end of the line in viminfo file (Webb).

Fixed CTRP-P/CTRL-N on a non-comment line causing an extra newline to be
inserted in when formatting is off (Webb).

Fixed moving marks with the ":move" command (Webb).

Fixed CTRL-P not working after CTRL-N hit end of the file (Webb).

Fixed BS in replace mode when deleting a NL where spaces have been deleted.

Removed 'delspaces' argument for Opencmd(), it was always TRUE.

Fixed overwriting "at top of tag stack" error message. Only show file message
for a tag when it is in another file.

Fixed not sleeping after an error message that is removed by a redraw.

Implemented incremental search and 'incsearch' option (Kesteloo).

Added 'backupext' option, default is ".~" instead of ".bak". Avoids accidently
overwriting ".bak" files that the user made by hand.

Fixed ":s///c", vgetc() was called twice and screen scrolled up.

Fixed problems with undo and empty buffer.

Fixed cursor not ending on right character when doing CTRL-T or CTRL-D in the
indent in insert mode (Webb).

Removed #ifdefs for WEBB_COMPLETE and WEBB_KEYWORD_COMPL.

When ":set columns=0" was done, core dump or hang might happen. Fixed this by
adding check_winsize() in doset().

 Vim 3.5 (finished 1995 April 2)

Added HOME and END keys to insert mode. HOME goes to column one, END goes to
end of line. They now also behave this way in normal mode.

Changed implementation of builtin termcaps. Now it's more easy to make changes.

Special keys are now handled with two characters, or a value above 0x100. All
256 keyboard codes can be used without restriction.

Made incremental search in visual mode display the visual area instead of the
matched string.

Fixed mouse not being used when using builtin_xterm.

Fixed message from :cn being deleted by screen redraw.

Fixed window resizing causing trouble while waiting for "-- more --" message.

":center" and ":right" ignore trailing blanks.           

Increased default for 'undolevels' for unix to 1000.

Fixed using CTRL-N and CTRL-P in replace mode.

Added mouse positioning on the command line.

Don't wait for a return after abandoning the command line.

Added a few extra checks to GCC in makefile.unix. Changed a few parameter
names to avoid unneccesary warning messages.

When splitting a window, the new window inherits the alternate file name.

With ":split [file]" and ":new [file]", the alternate file name in the current
window is set to [file].

Added CTRL-W CTRL-^ command: split and edit alternate file.

In replace mode NL does not replace a character but is inserted.

For ":s///c" the complete match is highlighted (like with incsearch).

Updated messages displayed when ".swp" file is found and at end of recovery.

Fixed not updating the visual area correctly when using the mouse.

Added 'smartmatch' option, to be able to switch back to vi-compatible matching.
Fixed matching braces inside quotes.

Autocommands now executed for each file, also when using "vim -o *.c".

Fixed docmdline() resetting RedrawingDisabled and other things when used
recursively.

For Unix: If input or output is not from/to a terminal, just give a warning
message, don't quit.

Replaced noremap list by noremapstr, much simpler.

Made ":map x<F1> foo" work.

Fixed "illegal line number" error when doing ":e!" after adding some lines at
the end of the file.

Fixed problem with viminfo file when newlines embedded in strings.

Changed default for 'keywordprg' to "man", much more useful for most of us.

Fixed core dump when using commands like ":swap" in .vimrc file.

Fixed ":map #1 :help" not working.

Fixed abbreviations not working properly.

Added insert and delete keys in termcap. Delete key in normal mode now behaves
like 'x'. Insert key in normal mode starts insert mode, in insert/replace mode
toggles between insert and replace mode, in command line editing toggles
insert/overstrike.

Made DEL in insert mode delete the character under the cursor. Join lines if
cursor after end of line and 'bs' set.

Added ":open" command to cmdtab.tab (not supported), to make ":o" not to be
recognized as ":only".

Fixed calling free() in buf_write() when smallbuf[] is used.

When writing the file fails and there is a backup file, try to put the backup
in place of the new file. Avoids loosing the original file when trying to
write again and overwriting the backup file.

Fixed problems with undo and redo in empty buffer.

Removed double redraw when returning from ":stop" command. (Webb)

Added 'indentchars' option: Characters that are included in indentifiers.
(Webb)

Smart indent when entering '{' is not rounded to 'shiftwidth'. (Webb)

Included setting of window size for iris_ansi window. (Webb).

Included better checks for minimal window size and give error message when it
is too small. (Webb).

Fixed mouse events being switched on again when exiting.

When opening a memfile, always write block 0 to disk, so findswapname() can
compare the file name in it. Avoids extra "swap file exists" messages.


 Vim 3.6 (finished 1995 April 28)

Made mouse click cursor movement inclusive.

Included catching of deadly signals in unix.c.

After recovering there was one extra empty line at the end.

Added ":recover [file]" command.

Fixed calling wait_return twice after message "ATTENTION: Found swap file .."

Fixed ":retab", it quitted if the first line had no changes. Save only the
lines that are changed for undo, not the whole file.

Fixed initialization problem in getcmdline() (Demirel).

If VIMINIT is set, but it is empty, ignore it (Demirel).

Fixed core dump when reporting number of buffers deleted.
Fixed core dump when deleting the current buffer.

Fixed ordering of codes in keymap.h (Webb).

Fixed mouse not turned off when doing CTRL-Z.

Changed 'identchar' to 'indentchars'.

If 'identchars' not set, use '_' instead of '"' as default.

Fixed autocommands not executed if file didn't exist.

Don't sleep after "ATTENTION" message.

Use enum for KS_ defines in keymap.h (Webb).

Fixed CTRL-C during shell command or ":make" to cause Vim to preserve the file
and quit.

When mouse click is on the status line, only make that window the current
window, don't move the cursor.

Fixed mouse positioning when 'number' is set.

Fixed cursor being moved when "]g" fails because buffer was changed.

Fixed cursor in wrong position after ":set invwrap" and cursor was below a
long line.

Fixed ":split file" causing a "hit return" message.

Fixed term codes of one character causing a hang (e.g. DEL).

Fixed mappings not working after using a non-remappable mapping.

Fixed core dump when screen is made bigger while in --more-- mode.

Fixed "@:" executing last Ex command without prepending ":".

When setting the cursor with the mouse, set curswant, so it sticks to that
column.

When going to another window in visual mode: When it is the same buffer,
update visual area, when jumping to another buffer reset visual mode.

Made visual reselect work after "J" command.

Left mouse click does not exit visual mode, just moves the cursor. 'v', 'V'
and CTRL-V set the visual mode, but don't exit visual mode. Use ESC instead.

Fixed problem that 'showcmd' would cause mappings to happen in the wrong mode.
e.g. ":map g 1G^M:sleep 3^M:g" would show ":1G".

Added "*""/" to 'comments' option again, useful when doing "O" on it.

Removed setkeymap() stuff, it has never worked and probably never will.

Fixed hang on exit when 'viminfo' set and cmdline history is empty.

Removed -traditional from gcc, caused arguments to system functions not to be
checked. Also removed "-Dconst=", because it caused warning for builtin
functions.

Fixed problem with MS-DOS compatible file system under Unix: Did not recognize
existing swap file. Now a proper check is done if the swap file is on an
MS-DOS compatible filesystem and 'shortname' is set if required. This also
causes the backup file name to be correct.

Added empty-buffer flag again. When an emtpy buffer is edited and written out,
that file is also empty.

Added "No lines in buffer" message when last line in buffer is deleted.

Fixed problem when mapping ends in a character that start a key code, would
wait for other characters for each character.

Added checks for creating too long lines on non-UNIX systems.

Added mouse positioning for MSDOS.

Changed commands for searching identifiers and defines in included files. It
is more logical now. Lower case for search first match, upper case for list
matches and Control for jump to first match. 'i' for identifier and 'd' for
define.

Added ":isearch", ":ilist", ":ijump", ":isplit", ":dsearch, etc. Allows for an
identifier to be found that is not in the text yet.

When file system is full, the error message for writing to the swap file was
repeated over and over, making it difficult to continue editing. Now it is
only given once for every key hit.

Added "gd", Go Declaration, search for identifier under cursor from the start
of the current function. "gD" searches from start of the file. Included files
are not used.

The number of lines reported when starting to edit a file was one too much.

Fixed problem that in some situations the screen would be scrolled up at every
message.

Removed the screenclear() calls for MSDOS in fileio.c, some messages got lost
by it. Just usr CTRL-L to redraw the screen in the rare case that you get the
"insert disk b: in drive a:" message.

Fixed "--more--" message not always working for normal-mode commands like "[i".

Made "[i", "[ CTRL-I" and "CTRL-W g" and the like ignore comment lines.

When 'q' is typed at "--more--" message, don't display an extra line, don't
wait for return to be hit.

Apply auto commands before reading the file, making modelines overrule auto
commands.

Allow trailing bar for ":autocmd", so it can be used in EXINIT.

For ":s" don't accept digit as separator.

When doing ":/pat/p" on the last line without 'wrapscan' set, used to start in
line 1 anyway. Now an error message is given and the command aborted.

When an error was detected in an Ex address (e.g. pattern not found) the
command was executed anyway, now it is aborted.

When doing ":/pat/p" with 'wrapscan' set, the "continuing at top" message was
displayed twice. Now it is overwritten.

Added "\?", "\/" and "\&" to ex address parsing, use previous search or
substitute pattern.

When there is not previous search pattern, would get two error messages. The
last one, "invalid search string" is now omitted.

When reading viminfo file for marks, accept file name with embedded spaces.

Set alternate file name for ":file fname" command.

When last file in argument list has been accessed, quit without asking.

When preserving file on a deadly signal, delete the swapfiles for buffers that
have no modifications.

A single error message from .vimrc would be overwritten by file message.

When an error message is given while sourcing or starting up, the source of
the error is mentioned.

Put character attributes in a separate byte in NextScreen, makes updating of
highlighted parts more reliable.

Don't display "search hit bot" message twice for ":/pat/p" and overwrite it
with any error message.

Fixed extra wait_return after error message on starting up.

Move '-' in 'identchars' to the end when using CTRL-N/P in insert mode. Avoids
most common "Invalid range" errors. Other chars still need a backslash.

 Vim 3.7 (not finished yet)

Adjusted formatting of comments to keep spaces after comment leader.

A space in the 'comments' option stands for any non-empty amount of white
space.

Changed T_LAST_KEY to t_mouse in term.c (Webb).

Also use 'id' option when searching for one-character id's, like with "w^N".

Fixed cursor in the wrong place after using DEL in insert mode at the end of
the line when the next line is empty.

When using "D" in an empty line, give beep.

Remove trailing spaces for Ex commands that accept a "+command" argument, when
it is not followed by another command. E.g. when using ":e file ".

Fixed problem that after using visual block mode any yank, delete and tilde
command would still use the visual coordinates. Added "block_mode" variable.

Fixed CTRL-V's not being removed from argument to :map.

Fixed not always redrawing correctly with ":unhide" command.

Made "|" exclusive, this is Vi compatible.

Set alternate file name when using buffer commands like ":bmod", ":buf",
":bdel", etc.

Added "interrupted" message when ":retab" is interrupted (Webb).

Doing "%" on an empty line now beeps (Webb).

Made number of included files to be remembered unlimited instead of just 50
(Webb).

For unix: Give error message when more than one file name given to Ex commands
that accept only one.

Fixed: When "+command" argument given to Ex command, wildcards in file name
after it were not correctly expanded.

Added two types to 'higlight' option: 'm' for -- more -- and 't' for titles
(Webb).

Added 'd' to -- more -- responses: down half a page (Webb).

Changes to quickfix (Webb): :cl lists only recognized errors, use :cl! to list
all. :cn and :cp don't go to an unrecognized error but give an error message
when at the first/last error. When a column number is not found, put the
cursor at the first non-blank. When deciding to redisplay the message or not,
take TABS into account. When :cp or :cn fails for some reason, don't change
the error index. use linetabsize() instead of strsize() to decide when to
redisplay a message. Changed the format of the :cl listing. Don't show the
column number if it is 0. Use msg_prt_line to print the error text, this takes
care of TABS. Error number or count can also be before :cc, :cp and :cn.

Added file name argument to ":doautocmd" (Webb).

Added check for length of "[i", could run into ruler (Webb).

Sometimes a long message was redisplayed, causing -- more -- twice (Webb).

Improved smartindent: When entering '}' and the matching '{' is preceded with
a '(', set indent to line containing the matching ')' (Webb).

Changed a few outstr() into msg_outstr(), makes screen output correct for some
cases, e.g. when using "!!cp file" the message for reading the temporary file
was messed up.

When filtering, use the 'shellredir' option to redirect the output. When
possible 'shellredir' is initialized to include stderr.

Environment variables in options are now replaced at any position, not just at
the start (Webb).

Incremental search no longer affects curswant and last used search pattern
(Webb).

Implemented "Vim -r" to list swap files.

When out of memory because undo is impossible, and 'y' is typed in answer to
the 'continue anyway' question, don't flush buffers (makes Maze macros work on
MS-DOS).

When asked a yes/no question, ESC is the same as 'n'.

Removed a few large arrays from the stack, MSDOS was running out of stack
space.

Fixed problem not being able to exit although all files from argument list
were accessed, when using "vim -o file ..." or ":all".

Sometimes mappings would not be recognized, because the wrong flags in
noremapstr[] were checked.

Fixed having to type return twice when starting to edit another file and the
message is too long.

*/

char		   *Version = "VIM 3.7";
#if !defined(__DATE__) || !defined(__TIME__)
char		   *longVersion = "Vi IMproved 3.7 by Bram Moolenaar (1995 June 1)";
#else
char		   *longVersion = "Vi IMproved 3.7 by Bram Moolenaar (1995 June 1, compiled " __DATE__ " " __TIME__ ")";
#endif
