#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <sys/stat.h>    // stat
#include <netinet/in.h>  // sockaddr_in
#include <dirent.h>   // 目录相关
#include <unistd.h>  // chdir
#include <pthread.h>  // 线程相关

#define SERVER_PORT 9000
#define COUNT_OF_LISTEN 10
#define BUFFER_SIZE 1024
#define FILE_NAME_MAX_SIZE 512

int clientCount=0; //连接的客户端个数
//客户端部分
int len=0;
int serverSocket_fd;
char currDir[100]={'.',0};
char command[100]={0};  //输入的命令
char cmdbuf[15]={0};
char msg[50]={0};
char fmbuffer[FILE_NAME_MAX_SIZE+1]={0};
char fbuffer[BUFFER_SIZE]={0};
char listbuffer[BUFFER_SIZE/2]={0};
void* clientThread(void* arg);

//服务器文件管理部分
char sercommand[100]={0};
char arg1[50]={0};
char arg2[50]={0};
char arg3[50]={0};
void doCd(const char* dirName);
void doExit();
void doHelp();
void doList();
void doPwd();
void doRm(const char* fileName);
void doRename(const char* oldName,const char* newName);
void analyseCmd();
void* ServerThread(void* arg);

int main()
{
    //服务器的文件管理,创建一个线程
    pthread_t sthread;
    int r = pthread_create(&sthread,NULL,ServerThread,NULL);
    if(r!=0){
        perror("Server Thread Create Error");
        exit(1);
    }

    // 声明并初始化服务器端的socket地址结构
    struct sockaddr_in serverSocket_addr;
    bzero(&serverSocket_addr,sizeof(serverSocket_addr));
    serverSocket_addr.sin_family = AF_INET;
    serverSocket_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverSocket_addr.sin_port = htons(SERVER_PORT);
    //socket()函数创建服务器端的socket

    if( (serverSocket_fd = socket(AF_INET,SOCK_STREAM,0)) < 0 )
    {
        perror("Server Create Socket Error:");
        exit(1);
    }
    //对服务器Socket进行一些设置
    int nNetTimeout = 5000; //5秒
    //发送时限
    setsockopt(serverSocket_fd,SOL_SOCKET,SO_SNDTIMEO,(char*)&nNetTimeout,sizeof(int));
    //接收时限
    setsockopt(serverSocket_fd,SOL_SOCKET,SO_RCVTIMEO,(char*)&nNetTimeout,sizeof(int));
    //在closesocket后继续重用socket
    int opt=1; //true
    setsockopt(serverSocket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //将服务器socket与其socket地址结构绑定
    if (0 > (bind(serverSocket_fd, (struct sockaddr*)&serverSocket_addr, sizeof(serverSocket_addr))))
    {
        perror("Server Bind Error:");
        exit(1);
    }
    //设置最大监听数
    if (0 > (listen(serverSocket_fd, COUNT_OF_LISTEN)))
    {
        perror("Server Listen Failed:");
        exit(1);
    }
    //循环监听客户端
    printf("服务器监听客户端模块启动...\n");
    for(;;)
    {
        //printf("服务器已经正常启动,监听中...\n");
        //将从客户端接收到的socket的信息写到client_addr中
        struct sockaddr_in client_addr;
        socklen_t client_addr_length = sizeof(client_addr);
        int clientSocket_fd = accept(serverSocket_fd,(struct sockaddr*)&client_addr, &client_addr_length);
        if ((clientSocket_fd < 0) && (errno == EINTR)) //信号中断
            continue;
        if(clientSocket_fd < 0)
        {
            perror("Server Accept Client Error:");
            break;
        }
        //为客户端socket创建一个线程来处理该客户端的请求
        pthread_t mythread;

        int ret = pthread_create(&mythread,NULL,clientThread,&clientSocket_fd);
        //线程创建函数的返回值不为0则表示有错误
        if(ret!=0){
            perror("Client Thread Create Error");
            close(clientSocket_fd);
            exit(1);
        }
    }
    //关闭服务器端socket
    close(serverSocket_fd);
    return 0;
}

void* clientThread(void* arg)
{
    int clientfd = *((int*)arg);
    bzero(cmdbuf,15);
    if(recv(clientfd,cmdbuf,15,0)<0){
        perror("Server Recieve Data Error:");
        close(clientfd);
        pthread_exit(NULL);
        return;
    }
    if(strcmp(cmdbuf,"S")==0){
        printf("有客户端连接进来, 当前客户端数目为:%d..\n",++clientCount);
        close(clientfd);
        pthread_exit(NULL);
        return;
    }
    else if(strcmp(cmdbuf,"E")==0){
        //退出
        printf("有客户端退出, 当前客户端数目为:%d..\n",--clientCount);
        close(clientfd);
        pthread_exit(NULL);
        return;
    }
    else if(strcmp(cmdbuf,"U")==0){
        //处理上传
        printf("有客户端上传文件:\n");
        //先获得客户端传来的文件名
        bzero(fmbuffer,FILE_NAME_MAX_SIZE+1);
        if(recv(clientfd,fmbuffer,FILE_NAME_MAX_SIZE+1,0)<0){
            perror("Server Recieve Data Error:");
            close(clientfd);
            pthread_exit(NULL);
            return;
        }
        printf("%s\n",fmbuffer);
        //先尝试创建这个文件,返回一个消息给客户端
        bzero(msg,sizeof(msg));
        FILE* fp = fopen(fmbuffer,"w");
        if (NULL == fp)
        {
            strcpy(msg,"fail");
            send(clientfd,msg,sizeof(msg),0);
            printf("不允许上传,File:%s Can Not Open To Write\n", fmbuffer);
        }
        else
        {
            strcpy(msg,"success");
            send(clientfd,msg,sizeof(msg),0);
            bzero(fbuffer,BUFFER_SIZE);
            len=0;
            int ok=1;
            while((len=recv(clientfd,fbuffer,BUFFER_SIZE,0))>0)
            {
                if(fwrite(fbuffer,sizeof(char),len,fp)<len)
                {
                    ok=0;
                    printf("File:%s Write Failed\n",fmbuffer);
                }
                bzero(fbuffer,BUFFER_SIZE);
            }
            if(ok)
                printf("从客户端成功接收到文件:%s...\n",fmbuffer);
            fclose(fp);
        }
        close(clientfd);
    }
    else if(strcmp(cmdbuf,"D")==0){
        //处理下载
        printf("有客户端下载文件:");
        //先获得客户端传来的文件名
        //char fm[FILE_NAME_MAX_SIZE+1]={0};
        bzero(fmbuffer,FILE_NAME_MAX_SIZE+1);
        if(recv(clientfd,fmbuffer,FILE_NAME_MAX_SIZE+1,0)<0){
            perror("Server Recieve Data Error:");
            close(clientfd);
            pthread_exit(NULL);
            return;
        }
        printf("%s\n",fmbuffer);
        //再根据文件名打开文件
        FILE* fp = fopen(fmbuffer,"r");
        bzero(msg,50);
        if(NULL==fp){
            printf("文件:%s 不存在\n",fmbuffer);
            strcpy(msg,"fail");
            printf("msg是:%s\n",msg);
            send(clientfd,msg,50,0);
        }
        else{
            strcpy(msg,"success");
            send(clientfd,msg,50,0);
            //开始传文件数据
            bzero(fbuffer,BUFFER_SIZE);
            len=0;
            int ok=1;
            while((len = fread(fbuffer,sizeof(char),BUFFER_SIZE,fp))>0)
            {
                if(send(clientfd,fbuffer,len,0)<0){
                    printf("传输文件:%s 失败..\n",fmbuffer);
                    ok=0;
                    break;
                }
                bzero(fbuffer,BUFFER_SIZE);
            }
            if(ok){
                printf("文件:%s 传输完成..\n",fmbuffer);
            }
            fclose(fp);
        }
    }
    else if(strcmp(cmdbuf,"L")==0){
        //显示服务器文件列表
        bzero(listbuffer,BUFFER_SIZE/2);
        DIR* mydir;
        struct dirent* myitem;
        struct stat iteminfo;
        mydir = opendir(".");
        if(mydir == NULL){
            fprintf(stderr, "Can't open Server directory..\n");
        }
        else{
            while((myitem = readdir(mydir)) != NULL)
            {
                stat(myitem->d_name,&iteminfo);
                if(S_ISDIR(iteminfo.st_mode)){
                    if (strcmp(myitem->d_name, ".") == 0 ||
                            strcmp(myitem->d_name, "..") == 0 )
                        continue;
                    strcpy(listbuffer,myitem->d_name);
                    strcat(listbuffer,"\t\tdir");
                }
                else{
                    strcpy(listbuffer,myitem->d_name);
                    strcat(listbuffer,"\t\tfile");
                }
                send(clientfd,listbuffer,BUFFER_SIZE/2,0);
                bzero(listbuffer,BUFFER_SIZE/2);
            }
        }
        closedir(mydir);
    }
    //关闭客户端socket
    close(clientfd);
    pthread_exit(NULL);
}
void analyseCmd()
{
    int i1,i2,i3,i4;
    bzero(arg1,50);bzero(arg2,50);bzero(arg3,50);
    for(i1=0;i1<strlen(sercommand);i1++){
        if(sercommand[i1]!=' ')
            break;
    }
    for(i2=i1;i2<strlen(sercommand);i2++){
        if(sercommand[i2]==' ')
            break;
        else
            arg1[i2-i1]=sercommand[i2];
    }
    for(;i2<strlen(sercommand);i2++){
        if(sercommand[i2]!=' ')
            break;
    }
    for(i3=i2;i3<strlen(sercommand);i3++){
        if(sercommand[i3]==' ')
            break;
        else
            arg2[i3-i2]=sercommand[i3];
    }
    for(;i3<strlen(sercommand);i3++){
        if(sercommand[i3]!=' ')
            break;
    }
    for(i4=i3;i4<strlen(sercommand);i4++){
        if(sercommand[i4]==' ')
            break;
        else
            arg3[i4-i3]=sercommand[i4];
    }
}
void doExit()
{
    printf("服务器程序退出.\n");
    close(serverSocket_fd);
    exit(0);
}

void doCd(const char *dirName)
{
    if(chdir(dirName)==0){
        printf("服务器默认目录切换到:%s\n",getcwd(NULL,0));
    }
    else
        printf("没有这个目录..\n");
}

void doHelp()
{
    printf("|-----------------------------------------------------------|\n");
    printf("| 服务器文件管理帮助:                                       |\n");
    printf("| cd 目录名: 切换服务器的默认目录                           |\n");
    printf("| list: 查看服务器默认目录下文件列表                        |\n");
    printf("| pwd: 查看服务器的默认目录                                 |\n");
    printf("| rm 文件(目录)名: 删除文件或目录                           |\n");
    printf("| rename 旧文件(目录)名 新文件(目录)名: 重命名文件或目录    |\n");
    printf("| exit: 退出服务器程序                                      |\n");
    printf("|-----------------------------------------------------------|\n");
}
void doList()
{
    DIR* mydir;
    struct dirent* myitem;
    struct stat iteminfo;  //stat结构存储目录项的详细信息
    mydir = opendir(".");
    if(mydir == NULL){
         fprintf(stderr, "Can't open directory %s\n", currDir);
    }
    else{
        printf("*****服务器默认目录下的文件列表*****\n");
        while((myitem = readdir(mydir)) != NULL)
        {
            stat(myitem->d_name,&iteminfo);
            if(S_ISDIR(iteminfo.st_mode)){
                if (strcmp(myitem->d_name, ".") == 0 ||
                        strcmp(myitem->d_name, "..") == 0 )
                    continue;
                printf("%s\t\t%s\n",myitem->d_name,"dir");
            }
            else{
                printf("%s\t\t%s\n",myitem->d_name,"file");
            }
        }
    }
    closedir(mydir);
}

void doPwd()
{
    printf("当前目录:%s\n",getcwd(NULL,0));
}

void doRm(const char *fileName)
{
    FILE* fp = fopen(fileName,"r");
    if(fp==NULL){
        printf("该文件(目录)不存在.\n");
        return;
    }
    else{
        fclose(fp);
        DIR* dp;
        if((dp=opendir(fileName))==NULL){
            //是文件
            char syscmd[50]={0};
            strcpy(syscmd,"rm ");
            strcat(syscmd,fileName);
            system(syscmd);
        }
        else{
            //是目录
            closedir(dp);
            char syscmd[50]={0};
            strcpy(syscmd,"rm -r ");
            strcat(syscmd,fileName);
            system(syscmd);
        }
        printf("删除成功.\n");
    }
}
void doRename(const char *oldName, const char *newName)
{
    FILE* fp = fopen(oldName,"r");
    if(fp==NULL){
        printf("源文件(目录)不存在.\n");
    }
    else{
        fclose(fp);
        fp = fopen(newName,"r");
        if(fp!=NULL){
            printf("新文件名已经存在,不能使用.\n");
            fclose(fp);
        }
        else{
            char syscmd[100]={0};
            strcpy(syscmd,"mv ");
            strcat(syscmd,oldName);strcat(syscmd," ");strcat(syscmd,newName);
            system(syscmd);
            printf("重命名完成.\n");
        }
    }
}


void* ServerThread(void *arg)
{
    printf("服务器文件管理模块启动..\n");
    printf("请输入服务器文件管理命令(输入help可查看支持的命令):\n");
    //服务器端的文件管理:删除,重命名
    for(;;)
    {
        bzero(sercommand,100);
        scanf("%[^\n]",sercommand);
        getchar(); //吃掉回车
        //对命令解析
        analyseCmd();
        //执行命令
        if(strlen(arg1)==0){
            printf("命令输入错误:\n");
        }
        else if(strcmp(arg1,"cd")==0){
            if(strlen(arg2)==0){
                doPwd();
            }
            else{
                doCd(arg2);
            }
        }
        else if(strcmp(arg1,"exit")==0){
            doExit();
        }
        else if(strcmp(arg1,"list")==0){
            doList();
        }
        else if(strcmp(arg1,"pwd")==0){
            doPwd();
        }
        else if(strcmp(arg1,"rm")==0){
            if(strlen(arg2)==0){
                printf("缺少文件(目录)名...\n");
            }
            else{
                doRm(arg2);
            }
        }
        else if(strcmp(arg1,"rename")==0){
            if(strlen(arg2)==0){
                printf("缺少源文件(目录)名..\n");
            }
            else if(strlen(arg3)==0){
                printf("缺少新文件(目录)名..\n");
            }
            else{
                doRename(arg2,arg3);
            }
        }
        else if(strcmp(arg1,"help")==0){
            doHelp();
        }
        else{
            printf("不支持的命令.\n");
        }
        printf("请输入服务器文件管理命令(输入help可查看支持的命令):\n");
    }
}

