/* vim:set ts=4 sw=4:
 * this program makes a tags file for reference.doc
 *
 * Usage: doctags reference.doc windows.doc ... >tags
 *
 * A tag in this context is an identifier between minus signs, e.g. *c_files*
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define LINELEN 200

main(argc, argv)
    int     argc;
    char    **argv;
{
    char    line[LINELEN];
    char    *p1, *p2;
    char    *p;
    FILE    *fd;

    if (argc <= 1)
    {
        fprintf(stderr, "Usage: doctags docfile ... >tags\n");
        exit(1);
    }
    while (--argc > 0)
    {
        ++argv;
        fd = fopen(argv[0], "r");
        if (fd == NULL)
        {
            fprintf(stderr, "Unable to open %s for reading\n", argv[0]);
            continue;
        }
        while (fgets(line, LINELEN, fd) != NULL)
        {
            p1 = strchr(line, '*');             /* find first '*' */
            while (p1 != NULL)
            {
                p2 = strchr(p1 + 1, '*');       /* find second '*' */
                if (p2 != NULL)
                {
                    for (p = p1 + 1; p < p2; ++p)
                        if (!isalnum(*p) && *p != '_')
                            break;
                    if (p == p2)                /* if it is all id-characters
*/
                    {
                        *p2 = '\0';
                        printf("%s\t%s\t/\\*%s\\*\n", p1 + 1, argv[0], p1 +
1);
                    }
                    p2 = strchr(p2 + 1, '*');
                }
                p1 = p2;
            }
        }
        fclose(fd);
    }
}
