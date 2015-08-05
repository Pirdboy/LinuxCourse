#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <sys/stat.h>    // stat
#include <netinet/in.h>  // sockaddr_in
#include <dirent.h>   // 目录相关
#include <unistd.h>  // chdir

#define SERVER_PORT 9000
#define BUFFER_SIZE 1024
#define FILE_NAME_MAX_SIZE 512

int client_fd=0;
int len=0;
struct sockaddr_in server_addr;
const char* serverIP = "127.0.0.1";
//char command[100]={0};  //输入的命令
//char cmdbuf[15]={0};
//char msg[50]={0};
//char fmbuffer[FILE_NAME_MAX_SIZE+1]={0};
//char fbuffer[BUFFER_SIZE]={0};
//char listbuffer[BUFFER_SIZE/2]={0};
char* command;  //输入的命令
char* cmdbuf;
char* msg;
char* fmbuffer;
char* fbuffer;
char* listbuffer;

void allocateMemory();
void freeMemory();
void connectToServer();
void tellServer();
void doHelp();
void docd(const char* dir);
void dolist_c();
void dolist_s();
void doDownload(const char* fileName);
void doUpload(const char* fileName);
void doPwd();
void doExit();
int checkFileOnServer(const char* fname);
int checkFileOnClient(const char* fname);

int main()
{
    //程序开始,调用系统调用为要用到的char*分配内存
    allocateMemory();
    // 声明一个服务器端的socket地址结构,并用服务器的IP和端口对其进行初始化,用于后面连接服务器
    bzero(&server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET; //IPv4协议
    if (inet_pton(AF_INET, serverIP, &server_addr.sin_addr) != 1) //IP地址
    {
        perror("Server IP Address Error:");
        exit(1);
    }
    server_addr.sin_port = htons(SERVER_PORT); //端口号
    //程序开始
    tellServer();
    //循环等待用户输入命令
    printf("请输入您要执行的客户端命令(输入help可查看支持的命令):\n");
    for(;;)
    {
        bzero(command,sizeof(command));
        scanf("%[^\n]",command);
        getchar(); //吃掉回车
        //解析输入的命令
        int i1,i2,i3;
        char arg1[50]={0};
        char arg2[50]={0};
        for(i1=0;i1<strlen(command);i1++){
            if(command[i1]!=' ')
                break;
        }
        for(i2=i1;i2<strlen(command);i2++){
            if(command[i2]==' ')
                break;
            else
                arg1[i2-i1]=command[i2];
        }
        for(;i2<strlen(command);i2++){
            if(command[i2]!=' ')
                break;
        }
        for(i3=i2;i3<strlen(command);i3++){
            if(command[i3]==' ')
                break;
            else
                arg2[i3-i2]=command[i3];
        }
        if(strlen(arg1)==0){
            printf("命令输入错误.\n");
        }
        else if(strcmp(arg1,"help")==0)
        {
            //查看帮助
            doHelp();
        }
        else if(strcmp(arg1,"cd")==0)
        {
            //切换目录
            docd(arg2);
        }
        else if(strcmp(arg1,"download")==0)
        {
            //下载文件
            doDownload(arg2);
        }
        else if(strcmp(arg1,"list")==0)
        {
            if(strlen(arg2)==0){
                printf("list参数错误...\n");
            }
            else{
                if(strcmp(arg2,"-c")==0){
                    dolist_c();
                }
                else if(strcmp(arg2,"-s")==0){
                    dolist_s();
                }
                else{
                    printf("list参数错误...\n");
                }
            }
        }
        else if(strcmp(arg1,"pwd")==0){
            //查看当前目录
            doPwd();
        }
        else if(strcmp(arg1,"upload")==0)
        {
            //上传文件
            doUpload(arg2);
        }
        else if(strcmp(arg1,"exit")==0)
        {
            //关闭客户端,需要服务器端关闭连接
            doExit();
            freeMemory();
            exit(0);
        }
        else{
            //输入的命令不对
            printf("不支持的命令.\n");
        }
        printf("请输入您要执行的客户端命令(输入help可查看支持的命令):\n");
    }
    //关闭客户端socket
    close(client_fd);
    return 0;
}
void allocateMemory()
{
    command = (char*)syscall(337,100*sizeof(char));
    cmdbuf = (char*)syscall(337,15*sizeof(char));
    msg = (char*)syscall(337,50*sizeof(char));
    fmbuffer = (char*)syscall(337,(FILE_NAME_MAX_SIZE+1)*sizeof(char));
    fbuffer = (char*)syscall(337,BUFFER_SIZE*sizeof(char));
    listbuffer = (char*)syscall(337,(BUFFER_SIZE/2)*sizeof(char));
}
void freeMemory()
{
    syscall(338,command);
    syscall(338,cmdbuf);
    syscall(338,msg);
    syscall(338,fmbuffer);
    syscall(338,fbuffer);
    syscall(338,listbuffer);
}

void doHelp()
{
    printf("|-------------------------------------------|\n");
    printf("| 客户端支持的命令:                         |\n");
    printf("| cd 目录名: 切换目录名                     |\n");
    printf("| download 文件名: 下载服务器上的文件       |\n");
    printf("| help: 查看可支持的命令                    |\n");
    printf("| list -c: 查看客户端当前目录的文件列表     |\n");
    printf("| list -s: 查看服务器端默认目录的文件列表   |\n");
    printf("| pwd: 查看客户端当前所在目录               |\n");
    printf("| upload 文件名: 上传客户端本地的文件       |\n");
    printf("| exit: 退出客户端                          |\n");
    printf("|-------------------------------------------|\n");
}

void docd(const char *dir)
{
    if(strlen(dir)==0){
        doPwd();
        return;
    }
    if(chdir(dir)==0){
        printf("已切换到目录:%s\n",getcwd(NULL,0));
    }
    else
        printf("没有这个目录..\n");
}
void doDownload(const char *fileName)
{
    if(strlen(fileName)==0){
        printf("缺少文件名...\n");
        return;
    }
    connectToServer();
    bzero(cmdbuf,sizeof(cmdbuf));
    strcpy(cmdbuf,"D");
    //先发送指令
    send(client_fd,cmdbuf,15,0);
    //再发送文件名
    bzero(fmbuffer,FILE_NAME_MAX_SIZE+1);
    strcpy(fmbuffer,fileName);
    send(client_fd,fmbuffer,FILE_NAME_MAX_SIZE+1,0);
    //再接受服务器的提示消息
    bzero(msg,50);
    if(recv(client_fd,msg,50,0)<0){
        perror("Client Recieve Data Error:");
        exit(1);
    }
    if(strcmp(msg,"fail")==0)
    {
        printf("该文件不存在,请检查文件名...\n");
    }
    else if(strcmp(msg,"success")==0)
    {
        //再接受服务器端传过来的数据
        len=0;
        bzero(fbuffer,BUFFER_SIZE);
        FILE* fp = fopen(fileName,"w");
        if(fp==NULL)
        {
            printf("File:%s Can Not Open To Write\n",fileName);
            exit(1);
        }
        while( (len = recv(client_fd,fbuffer,BUFFER_SIZE,0))>0 )
        {
            if(fwrite(fbuffer,sizeof(char),len,fp)<len)
            {
                printf("File:\t%s Write Failed\n", fileName);
                break;
            }
            bzero(fbuffer, BUFFER_SIZE);
        }
        printf("下载文件:%s 完毕\n",fileName);
        fclose(fp);
    }
    close(client_fd);
}
void doUpload(const char *fileName)
{
    if(strlen(fileName)==0){
        printf("缺少文件名...\n");
        return;
    }
    FILE* fp = fopen(fileName,"r");
    if(NULL == fp)
    {
        printf("该文件不存在,请检查文件名...\n");
    }
    else{
        connectToServer();
        //char buf[15]={0};
        bzero(cmdbuf,15);
        strcpy(cmdbuf,"U");
        //先发送指令
        send(client_fd,cmdbuf,15,0);
        //再发送文件名
        //char buf2[FILE_NAME_MAX_SIZE+1]={0};
        bzero(fmbuffer,sizeof(fmbuffer));
        strcpy(fmbuffer,fileName);
        send(client_fd,fmbuffer,FILE_NAME_MAX_SIZE+1,0);
        //先接受服务器返回的信息看服务器是否能够创建同名文件
        bzero(msg,50);
        if(recv(client_fd,msg,50,0)<0){
            perror("Client Recieve Data Error:");
            fclose(fp);
            close(client_fd);
            exit(1);
        }
        if(strcmp(msg,"fail")==0)
        {
            printf("服务器上不能创建文件:%s... 上传失败\n",fileName);
        }
        else if(strcmp(msg,"success")==0)
        {
            //再发送文件
            len=0;
            bzero(fbuffer,BUFFER_SIZE);
            int ok=1;
            while((len=fread(fbuffer,sizeof(char),BUFFER_SIZE,fp))>0)
            {
                if(send(client_fd,fbuffer,len,0)<0)
                {
                    printf("上传文件:%s .失败\n",fileName);
                    ok=0;
                }
                bzero(fbuffer,BUFFER_SIZE);
            }
            if(ok)
                printf("上传文件:%s .成功\n",fileName);
        }
        fclose(fp);
        close(client_fd);
    }
}

void dolist_c()
{
    DIR* mydir;
    struct dirent* myitem;
    struct stat iteminfo;  //记录了ls -l所显示的信息
    mydir = opendir(".");
    if(mydir == NULL){
         fprintf(stderr, "Can't open directory %s\n", getcwd(NULL,0));
    }
    else{
        printf("文件列表:\n");
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
void dolist_s()
{
    connectToServer();
    bzero(cmdbuf,15);
    strcpy(cmdbuf,"L");
    //将"L"传给服务器表示要查看文件列表
    send(client_fd,cmdbuf,15,0);
    printf("Server默认目录下文件列表:\n");
    bzero(listbuffer,BUFFER_SIZE/2);
    len=0;
    while((len=recv(client_fd,listbuffer,BUFFER_SIZE/2,0)) != 0)
    {
        printf("%s\n",listbuffer);
        bzero(listbuffer,BUFFER_SIZE/2);
    }
    close(client_fd);
}
void doPwd()
{
    printf("当前目录:%s\n",getcwd(NULL,0));
}
void doExit()
{
    //退出时向服务器发送信息
    connectToServer();
    bzero(cmdbuf,15);
    strcpy(cmdbuf,"E");
    //将"E"传给服务器表示客户端退出了
    send(client_fd,cmdbuf,15,0);
    printf("客户端退出...\n");
    close(client_fd);
}
void connectToServer()
{
    //客户端创建socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_fd < 0)
    {
        perror("Client Create Socket Error:");
        exit(1);
    }
    //对客户端Socket进行一些设置
    int nNetTimeout = 5000; //5秒
    //发送时限
    setsockopt(client_fd,SOL_SOCKET,SO_SNDTIMEO,(char*)&nNetTimeout,sizeof(int));
    //接收时限
    setsockopt(client_fd,SOL_SOCKET,SO_RCVTIMEO,(char*)&nNetTimeout,sizeof(int));
    //在closesocket后继续重用socket
    int opt=1; //true
    setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //进行连接
    socklen_t server_addrlen = sizeof(server_addr);
    if (connect(client_fd, (struct sockaddr*)&server_addr, server_addrlen) < 0)
    {
        perror("Can Not Connect To Server IP:");
        exit(0);
    }
}
void tellServer()
{
    connectToServer();
    bzero(cmdbuf,15);
    strcpy(cmdbuf,"S");
    //将"E"传给服务器表示客户端退出了
    send(client_fd,cmdbuf,15,0);
    printf("客户端启动:\n");
    close(client_fd);
}

