#include "file.h"

int file_size(const char *filename)
{
    struct stat file_info;
    return (!stat (filename, &file_info))?file_info.st_size:-1;
}

bool file_exists (const char *filename)
{
    struct stat file_info;
    return (!stat (filename, &file_info))?true:false;
}

bool dir_exists(const char *dirname)
{
    struct stat sb;
    stat(dirname, &sb);

    return (errno == ENOENT || (sb.st_mode & S_IFMT) != S_IFDIR)?false:true;
}

int is_dir(const char *dirname)
{
    struct stat stats;
    return stat(dirname, &stats) == 0 && S_ISDIR(stats.st_mode);
}

bool is_dir_empty(const char *dirname)
{
    struct dirent *pdir;
    DIR *d;

    d = opendir(dirname); /* check for errors */

    while ((pdir=readdir(d)))
    {
        if ( !strcmp(pdir->d_name, ".") ||
             !strcmp(pdir->d_name, "..") )
            continue;

        return false;
    }

    return true;
}

void num2human(long double n, uint16_t bconv)
{
    const char *label[] = {"","k","M","G","T","P","E","Z","Y", 0};
    uint64_t power = 1;
    uint8_t  i = 0;

    while(label[i] != 0)
    {
        if ((n >= power) && (n < (power * bconv)))
        {
            printf("%Lg %s", n/power, label[i]);
            break;
        }
        else if (label[i+1] == 0)
        {
            printf("%Lg %s",n/power, label[i]);
        }

        i++;
        power *= bconv;
    }
}
