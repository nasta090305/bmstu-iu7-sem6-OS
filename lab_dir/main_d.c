#include "apue.h"
#include <time.h>

#define FTW_F 1    /* файл, не являющийся каталогом */
#define FTW_D 2    /* каталог */
#define FTW_DNR 3  /* каталог, который не может быть прочитан */
#define FTW_NS 4   /* файл, информацию о котором нельзя получить */

static char *shortpath; 
static size_t path_len;

typedef int Myfunc(const char *, int, int);
typedef int DoPath(Myfunc *, int);

static Myfunc myfunc;
static int myftw(char *, Myfunc *, DoPath *);
static int dopath(Myfunc *, int);

int main(int argc, char *argv[])
{
    int ret;
    if (argc != 2)
        err_quit("Использование: ftw <начальный_каталог>");
    ret = myftw(argv[1], myfunc, dopath); 
    exit(ret);
}

static int myftw(char *pathname, Myfunc *func, DoPath *dopath)
{
    int nesting = 0;
    int len = PATH_MAX + 1;
    shortpath = path_alloc(&len);
    strcpy(shortpath, pathname);
    shortpath[len - 1] = 0;
    return(dopath(func, nesting)); 
}

static int dopath(Myfunc* func, int nesting)
{
    struct stat statbuf;
    struct dirent *dirp;
    DIR *dp;
    int ret, n;

    if (lstat(shortpath, &statbuf) < 0)      // ошибка при получении информации о файле
        return(func(shortpath, nesting, FTW_NS));

    if (S_ISDIR(statbuf.st_mode) == 0)      // не каталог
        return(func(shortpath, nesting, FTW_F));

    if ((ret = func(shortpath, nesting, FTW_D)) != 0)
        return(ret);
    
    if ((dp = opendir(shortpath)) == NULL)   // каталог не может быть прочитан
        return(func(shortpath, nesting, FTW_DNR));

    if (chdir(shortpath) < 0) 
        err_sys("не удалось изменить директорию"); 
    
    int stop = 1;
    nesting++;
    while ((dirp = readdir(dp)) != NULL && stop) 
    {
        if (!(strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0))
        {
            strcpy(&shortpath[0], dirp->d_name);
            if ((ret = dopath(func, nesting)) != 0)
                stop = 0;
        }
    }
    nesting--;
    printf("XXX\n");
    
    chdir("..");

    if (closedir(dp) < 0)
        err_ret("невозможно закрыть каталог %s", shortpath);
    
    return(ret);
}

static int myfunc(const char *pathname, int nesting, int type)
{
    if (type == FTW_F || type == FTW_D) 
    {
        const char *filename;
        int i = 0;

        for (int i = 0; i < nesting; ++i)
            if (i != nesting - 1)
                printf("│   ");
            else
                printf("└───");
        
        if (nesting > 0) 
        {
            filename = strrchr(pathname, '/');
            if (filename != NULL)
                printf("%s\n", filename + 1);
            else
                printf("%s\n", pathname);
        } else 
            printf("%s\n", pathname);

    } else if (type == FTW_DNR) 
        err_ret("закрыт доступ к каталогу %s", pathname);
    else if (type == FTW_NS)
        err_ret("ошибка вызова функции stat для %s", pathname);
    else
        err_ret("неизвестный тип %d для файла %s", type, pathname);

    return 0;
}

