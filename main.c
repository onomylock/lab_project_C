#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#define TABLE_SIZE 10

struct stat st;

struct file
{
    char filename[20];      //Имя файла
    int locate_point;       //Положение указателя чтения-записи
    ushort mode;            //Режим открытия файла
    ino_t inode;            //Серийный номер файла
    int i_td;               //Ссылка на таблицу дескрипторов
};

struct open_file
{
    char filename[20];      //Название файла
    size_t desc;            //Номер дискриптора файла 
    int i_tf;               //Ссылка на таблицу файлов
};

struct descriptor
{
    ino_t  inode;           //Серийный номер файла
    ushort mode;            //Модификатор открытия файла
    short  nlink;           //Количество ссылок на данный дескриптор
    ushort uid;             //User id
    ushort gid;             //Group id
    size_t size;            //Размер файла
    time_t atime;           //Время последнего доступа к файлу
    time_t mtime;           //Время последнего изменения файла
    time_t ctime;           //Время последнего изменения статуса файла
};

struct tables
{
    struct open_file tof[TABLE_SIZE]; //Таблица открытых файлов
    struct file tf[TABLE_SIZE];       //Таблица файлов
    struct descriptor td[TABLE_SIZE]; //Таблица дескрипторов

    //Количество записей в каждой таблице
    int size_tof;
    int size_tf;
    int size_td;
};

struct tables t;

size_t fcreat(const char *filename, mode_t mode)
{
    int fd;
    
    //Если ошибки не встретились, создаем файл
    if (fd = creat(filename, mode) < 0)
    {
        switch(errno)
        {
            case ENOENT: 
            {
                printf("ERR: Route or file name not found.\n");
                break;
            }

            case EMFILE: 
            {
                printf("ERR: Too many open files.\n");
                break;
            }
        
            case EACCES:
            {
                printf("ERR: Access denied.\n");
                break;
            }

            default:
            {
                printf("Error creating file\n");
                break;
            }     
        }
        exit(-1);
    }

    //Увеличиваем размер таблицы открытых файлов и таблицы файлов
    t.size_tf++;
    t.size_tof++;

    if(stat(filename,&st) != 0)
    {
        printf("ERR: stat");
        return -1;
    }

    //Таблица открытых файлов
    t.tof[t.size_tof].desc = fd;
    strncpy(t.tof[t.size_tof].filename, filename, 20);
    t.tof[t.size_tof].i_tf = t.size_tf;

    //Таблица файлов
    strncpy(t.tf[t.size_tf].filename, filename, 20);    
    t.tf[t.size_tf].inode = st.st_ino;
    t.tf[t.size_tf].mode = mode;
    t.tf[t.size_tf].locate_point = 0;

    //Таблица дескрипторов
    if (t.size_td == -1)
    {
        t.size_td++;
        t.td[0].inode = st.st_ino;
        t.td[0].atime = st.st_atime;
        t.td[0].ctime = st.st_ctime;
        t.td[0].mtime = st.st_mtime;
        t.td[0].nlink = st.st_nlink;
        t.td[0].size = st.st_size;
        t.td[0].mode = st.st_mode;
        t.td[0].uid = st.st_uid;
        t.td[0].gid = st.st_gid;

        t.tf[t.size_tf].i_td = 0;
    }
    else
    {
        _Bool flag = false;
        for (int i = 0; i < t.size_td; i++)
        {
            if (t.td[i].inode == st.st_ino)
            {
                t.tf[t.size_tf].i_td = i;
                t.td[i].nlink++;
                flag = true;
                break;
            }  
        } 

        if (!flag)
        {
            t.size_td++;
            t.td[t.size_td].inode = st.st_ino;
            t.td[t.size_td].atime = st.st_atime;
            t.td[t.size_td].ctime = st.st_ctime;
            t.td[t.size_td].mtime = st.st_mtime;
            t.td[t.size_td].nlink = st.st_nlink;
            t.td[t.size_td].size = st.st_size;
            t.td[t.size_td].mode = st.st_mode;
            t.td[t.size_td].uid = st.st_uid;
            t.td[t.size_td].gid = st.st_gid;

            t.tf[t.size_tf].i_td = t.size_td;
        }
    }

    return fd;
}

size_t flwrite(size_t desc, char *str, size_t size)
{
    size_t res;
    errno = 0; 

    //Если ошибки не встретились, записываем данные в файл
    if ((res = write(desc, str, size)) < 0)
    {
        switch (errno)
        {
        case EBADF:
            printf("EBADF\n");
            break;
        case EFAULT:
            printf("buf\n");
            break;
        case EINTR:
            printf("signal\n");
            break;
        case EINVAL:
            printf("direct\n");
            break;
        case EIO:
            printf("IO\n");
            break;
        default:
            break;
        }
        exit(-1);
    }

    //Если количество записанных байтов равно размеру заданных байтов на запись
    if (res == size)
    {
        for (int i = 0; i <= t.size_tof; i++)
        {
            if (desc == t.tof[i].desc)
            {
                //Увеличиваем положение указателя чтения-записи
                t.tf[t.tof[i].i_tf].locate_point += size;
                if(fstat(desc,&st) != 0)
                {
                    printf("ERR: stat");
                    return -1;
                }
                //Изменяем время последней модификации файла и его размер
                t.td[t.tf[t.tof[i].i_tf].i_td].mtime = st.st_mtime;
                t.td[t.tf[t.tof[i].i_tf].i_td].size =+ st.st_size;
                break;
            }
        }
    }
    return res;
}

size_t flread(size_t desc, char *buf, size_t size)
{
    ssize_t res;
    errno = 0;
    //Если ошибки не встретились, считываем данные из файла
    if ((res = read(desc, buf, size)) <= 0)
    {
        switch (errno)
        {
        case EBADF:
            printf("EBADF\n");
            break;
        case EFAULT:
            printf("EFAULT\n");
            break;
        case EINTR:
            printf("EINTR\n");
            break;
        case EINVAL:
            printf("EINVAL\n");
            break;
        case EIO:
            printf("EIO\n");
            break;
        default:
            break;
        }
        exit(-1);
    }
    
    //Если количество считанных байтов равно размеру заданных байтов на считывание
    if (res == size)
    {
        for (int i = 0; i <= t.size_tof; i++)
        {
            if (desc == t.tof[i].desc)
            {
                //Увеличиваем положение указателя чтения-записи
                t.tf[t.tof[i].i_tf].locate_point += size;
                
                if(fstat(desc,&st) != 0)
                {
                    printf("ERR: stat");
                    return -1;
                }
                //Изменяем время последнего доступа к файлу
                t.td[t.tf[t.tof[i].i_tf].i_td].atime = st.st_atime;
                break;
            }
        }
    }
    return res;
}

size_t flopen(const char *filename, int flag, mode_t mode)
{
    int fd;
    errno = 0;
    //Если ошибки не встретились, открываем файл
    if ((fd = open(filename, flag, mode)) < 0)
    {
        switch (errno)
        {
        case EACCES:
            printf("EACCES\n");
            break;
        case EFAULT:
            printf("EFAULT\n");
            break;
        case EEXIST:
            printf("EEXIST\n");
            break;
        default:
            break;
        }
        exit(-1);
    }

    //Увеличиваем размер таблицы открытых файлов и таблицы файлов
    t.size_tf++;
    t.size_tof++;

    if(stat(filename,&st) != 0)
    {
        printf("ERR: stat");
        return -1;
    }

    //Таблица открытых файлов
    t.tof[t.size_tof].desc = fd;
    strncpy(t.tof[t.size_tof].filename, filename, 20);
    t.tof[t.size_tof].i_tf = t.size_tf;

    //Таблица файлов
    strncpy(t.tf[t.size_tf].filename, filename, 20);    
    t.tf[t.size_tf].inode = st.st_ino;
    t.tf[t.size_tf].mode = mode;
    t.tf[t.size_tf].locate_point = 0;

    //Таблица дескрипторов
    _Bool find = false;
    for (int i = 0; i <= t.size_td; i++)
    {
        if (t.td[i].inode == st.st_ino)
        {
            t.tf[t.size_tf].i_td = i;
            t.td[i].nlink++;
            find = true;
            break;
        }  
    } 

    if (!find)
    {
        t.size_td++;
        t.td[t.size_td].inode = st.st_ino;
        t.td[t.size_td].atime = st.st_atime;
        t.td[t.size_td].ctime = st.st_ctime;
        t.td[t.size_td].mtime = st.st_mtime;
        t.td[t.size_td].nlink = st.st_nlink;
        t.td[t.size_td].size = st.st_size;
        t.td[t.size_td].mode = st.st_mode;
        t.td[t.size_td].uid = st.st_uid;
        t.td[t.size_td].gid = st.st_gid;

        t.tf[t.size_tf].i_td = t.size_td;
    }
    return fd;
}

size_t flclose(size_t desc)
{
    //Ищем указанный дескриптор в таблице открытых файлов
    for (int i = 0; i <= t.size_tof; i++)
    {
        //Если указанный дескриптор найден
        if (t.tof[i].desc == desc)
        {
            //Получаем индекс в таблице файлов
            int i_tf = t.tof[i].i_tf; 
            
            //Удаляем строку в таблице открытых файлов
            for(int j = i; j < t.size_tof; j++)
            {
                t.tof[j] = t.tof[j + 1]; 
            }
            t.size_tof--; 

            //Получаем индекс в таблице дескрипторов
            int i_td = t.tf[i_tf].i_td; 

            //Удаляем строку в таблице файлов
            for (int i = i_tf; i < t.size_tf; i++)
            {
                t.tf[i] = t.tf[i + 1]; 
            }
            t.size_tf--;

            //Корректируем индекс в таблице открытых файлов
            for(int l = 0; l <= t.size_tof; l++){
                if(t.tof[l].i_tf > i_tf){
                    t.tof[l].i_tf--;
                }
            }

            //Удаляем строку в таблице дескрипторов, если количество ссылок на нее равно 1
            if (t.td[i_td].nlink == 1)
            {
                for(int m = i_td; m < t.size_td; m++)
                {
                    t.td[m] = t.td[m+1];
                }
                t.size_td--;
            }
            else
            {
                t.td[i_td].nlink--;
            }
            

            //Корректируем индексы файлов
            for (int i = 0; i <= t.size_tof; i++)
            {
                if (t.tof[i].i_tf > i_tf)
                {
                    t.tof[i].i_tf--;
                }
            }
        }
    }
    return close(desc);
}

int print_tables()
{
    printf("********************************\n");
    printf("Tables of open files\n");
    printf("filename\t| desc\t| i_tf\t\n");
    for(int i = 0; i <= t.size_tof; i++)
    {
        printf("%s\t|%lu\t|%d\t\n", t.tof[i].filename, t.tof[i].desc, t.tof[i].i_tf); 
    }
    printf("\n");

    printf("Tables of files\n");
    printf("filename\t| inode\t\t| locate_p\t| i_td\n");
    for(int i = 0; i <= t.size_tf; i++)
    {
        printf("%s\t|%lu\t|%d\t\t|%d\t\n", t.tf[i].filename, t.tf[i].inode, t.tf[i].locate_point, t.tf[i].i_td);
    }
    printf("\n");

    printf("Tables of descriptors \n");
    printf("mode\t| nlink\t| uid\t| gid\t| size\t| atime\t\t| mtime\t\t| ctime\n");
    for(int i = 0; i <= t.size_td; i++)
    {
        printf("%d\t|%d\t|%d\t|%d\t|%lu\t|%ld\t|%ld\t|%ld\t\n", t.td[i].mode, t.td[i].nlink, t.td[i].uid, t.td[i].gid, t.td[i].size, t.td[i].atime, t.td[i].mtime, t.td[i].ctime);
    }
    printf("\n");
}

int main()
{
    t.size_td = -1;
    t.size_tf = -1;
    t.size_tof = -1;
    char *filename = "text_main", *buf, *text = "abcde";

    //Выделяем память под массив
    buf = malloc(6);

    //Создаем файл 
    size_t fd = fcreat(filename, 0664); 
    print_tables();

    //Открываем файл на запись
    size_t fd1 = flopen(filename, O_WRONLY, 0664);
    print_tables();

    //Открываем файл на чтение
    size_t fd2 = flopen(filename, O_RDONLY, 0664);
    print_tables();

    //Записываем данные в файл
    size_t wr = flwrite(fd1, text, 5);
    print_tables();

    //Считываем данные из файла
    size_t rd = flread(fd2, buf, 5);
    print_tables();

    //Закрываем файл
    size_t cl2 = flclose(fd2);
    print_tables();

    size_t cl1 = flclose(fd1);
    print_tables();


    size_t cl = flclose(fd);
    print_tables();

    //Освобождаем память из массива
    free(buf);

    return 0;
}